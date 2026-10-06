/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "rpmsgSession.h"
#include "glinkSessionError.h"
#include "qshSSR.h"
#include "sns_client.pb.h"
#include <cinttypes>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <sys/resource.h>

#include "LoggerFactory.h"
#include "sessionLoggerUtil.h"

using namespace ::com::quic::sensinghub::session::V1_0::implementation;
using namespace ::com::quic::sensinghub::sessionlogger::V1_0;
using namespace std;

#define SNS_RPMSG_MAX_REQUEST_SIZE 1024
#define SNS_RPMSG_MAX_INDICATION_SIZE 4096
#define SNS_RPMSG_ACK_TIMEOUT_MAX_RETRY 100
#define SNS_RPMSG_ACK_TIMEOUT_MS 5
static const uint64_t NSEC_PER_SEC = 1000000000ull;
/* SSR is notified in data thread. To avoid sending
   request in the same thread as data thread, assign
   worker thread to fetching new conn handles after SSR.
*/
static std::unique_ptr<qshWorker> restore_ssr_conn_hndl_worker;
static std::unique_ptr<qshWorker> ind_hndl_worker;
const uint32_t RPMSG_ERRORS_HANDLED_ATTEMPTS = 10; // TODO
std::atomic<bool> rpmsgSession::_is_thread_created = false;
std::atomic<int> rpmsgSession::_chnl_fd = -1;
std::atomic<uint32_t> rpmsgSession::_ack_conn_hndl = 0;
std::atomic<bool> rpmsgSession::_is_req_success = false;
qshWakelock *rpmsgSession::_wakelock_inst = NULL;
std::string rpmsgSession::_chnl_name = "";
std::atomic<uint32_t> rpmsgSession::_rpmsg_err_cnt = 0;
std::atomic<uint32_t> rpmsgSession::_rpmsg_clients = 0;
std::atomic<bool> rpmsgSession::_rpmsg_is_ssr_in_progress = false;
int rpmsgSession::_wakeup_pipe[2];
std::mutex rpmsgSession::_rpmsg_write_mutex;
std::mutex rpmsgSession::_resp_queue_mutex;
std::mutex rpmsgSession::_rpmsg_open_close_mutex;
std::mutex rpmsgSession::_rpmsg_db_mutex;
std::mutex rpmsgSession::_rpmsg_setcb_mutex;
rpmsg_conn_mp_type rpmsgSession::_clientid_conn_db;
rpmsg_conn_mp_type rpmsgSession::_restore_ssr_conn_db;
std::unordered_map<suid, vector<uint32_t>, rpmsg_cb_sensorUID_hash>
    rpmsgSession::_suid_conn_handles_db;
std::deque<rpmsg_stream_req_info> rpmsgSession::_resp_queue;
std::vector<std::string> rpmsgSession::_rpmsg_chnls_list;
std::thread rpmsgSession::_rpmsg_read_threadhdl;

rpmsgSession::rpmsgSession(int no_of_chnls, int hub_id, string hub_name)
    : _is_ftrace_enabled(false), _reconnecting(false),
      _connection_closed(false) {
  sns_logd("rpmsgSession conn = 0x%llx constructor", (uint64_t)this);
  if (hub_name.empty()) {
    sns_loge("rpmsgSession: hub_name is empty for hub_id=%d", hub_id);
    throw std::invalid_argument("hub_name cannot be empty");
  }
  if (0 == _rpmsg_chnls_list.size()) {
    for (int id = 0; id < no_of_chnls; id++) {

      string chnl_name =
          "qsh_" + to_string(hub_id) + "_" + to_string(id);

      sns_logi("Adding RPMSG channel %s", chnl_name.c_str());

      _rpmsg_chnls_list.push_back(chnl_name);
    }
  }
  _worker = make_unique<qshWorker>();

  _logger = LoggerFactory::getLogger("APSS", this);
  sns_logi("%s Logger is set for %llu", __func__, this);
}

int rpmsgSession::open() {
  sns_logv("rpmsgSession: acquiring _rpmsg_open_close_mutex");
  lock_guard<mutex> lk(_rpmsg_open_close_mutex);
  _connection_closed = false;

  try {
    rpmsg_connect(true);
    if (_wakelock_inst == nullptr)
      _wakelock_inst = qshWakelock::getInstance(RPMSG_WAKELOCK_NAME);
    if (_wakelock_inst == nullptr) {
      sns_loge("Failed to create wakelock instance.");
    }
    _conn_status = rpmsg_conn_status::RPMSG_CONNECTION_STATUS_INIT;
  } catch (const exception &e) {
    sns_loge("%s failed: %s", __func__, e.what());
    return -1;
  }
  sns_logv("rpmsgSession: releasing _rpmsg_open_close_mutex");
  return 0;
}

void rpmsgSession::close() {
  sns_logv("rpmsgSession: acquiring _rpmsg_open_close_mutex");
  lock_guard<mutex> lk(_rpmsg_open_close_mutex);
  _connection_closed = true;
  rpmsgSession::_rpmsg_clients--;
  rpmsg_cleanup_conn();
}

rpmsgSession::~rpmsgSession() {
  sns_logi("rpmsgSession conn = 0x%llx destructor", (uint64_t)this);
  sns_logv("rpmsgSession: acquiring _rpmsg_open_close_mutex");
  lock_guard<mutex> lk(_rpmsg_open_close_mutex);
  _worker.reset();
  /* No clean up is required if channel is already closed */
  if (_connection_closed) {
    sns_logv("rpmsgSession: already closed, skipping destructor cleanup");
    return;
  }

  rpmsgSession::_rpmsg_clients--;
  _connection_closed = true;
  rpmsg_cleanup_conn();
  sns_logv("rpmsgSession: releasing _rpmsg_open_close_mutex");
}

void rpmsgSession::rpmsg_cleanup_conn() {
  /* Delete this object's entry in _client_id_conn_db */
  sns_logv("rpmsgSession: acquiring _rpmsg_db_mutex");
  _rpmsg_db_mutex.lock();
  for (auto &it : _callback_map_table) {
    uint32_t conn_hndl = update_clientid_conn_db(it.first);
    if (0 != conn_hndl)
      send_disconnect_req(it.first, conn_hndl);
  }
  _callback_map_table.clear();
  _rpmsg_db_mutex.unlock();
  sns_logv("rpmsgSession: released _rpmsg_db_mutex");
  rpmsg_disconnect(true);
}

/* Discover the /dev/rpmsgN node created for `chnl_name` on ctrl device
 * `ctrl_name` by scanning /sys/class/rpmsg for a device whose "name"
 * attribute matches. The node may not appear immediately after the ioctl
 * returns, so this polls briefly.
 */
static int find_rpmsg_dev_node(const std::string &chnl_name,
                                std::string &out_dev_path) {
  for (int attempt = 0; attempt < MAX_RPMSG_EPT_DISCOVER_RETRIES; attempt++) {
    DIR *dir = opendir(RPMSG_DEV_SYSFS_DIR);
    if (dir != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        string devname = entry->d_name;
        if (devname == "." || devname == "..")
          continue;
        string name_attr_path =
            string(RPMSG_DEV_SYSFS_DIR) + "/" + devname + "/name";
        FILE *f = fopen(name_attr_path.c_str(), "r");
        if (f == nullptr)
          continue;
        char buf[64] = {0};
        char *res = fgets(buf, sizeof(buf), f);
        fclose(f);
        if (res == nullptr)
          continue;
        string attr_val = buf;
        while (!attr_val.empty() &&
               (attr_val.back() == '\n' || attr_val.back() == '\r'))
          attr_val.pop_back();
        if (attr_val == chnl_name) {
          out_dev_path = string("/dev/") + devname;
          closedir(dir);
          return 0;
        }
      }
      closedir(dir);
    }
    usleep(RPMSG_EPT_DISCOVER_POLL_MS * 1000);
  }
  return -ENOENT;
}

#define MAX_RPMSG_CTRL_ANCESTOR_LEVELS 6

/* Resolve <class_dev_path>/device's real sysfs path, then walk upward
 * through its ancestors (up to max_levels) looking for a file named
 * attr_name whose contents equal expected_value. Mirrors udev's
 * ATTRS{} (ancestor-walk) semantics, since rpmsg_name is not a direct
 * attribute of the rpmsg_ctrl class device.
 */
static bool sysfs_ancestor_attr_matches(const std::string &class_dev_path,
                                         const std::string &attr_name,
                                         const std::string &expected_value,
                                         int max_levels) {
  char resolved[PATH_MAX];
  std::string device_link = class_dev_path + "/device";
  if (realpath(device_link.c_str(), resolved) == nullptr &&
      realpath(class_dev_path.c_str(), resolved) == nullptr) {
    return false;
  }

  std::string cur = resolved;
  for (int level = 0; level <= max_levels; level++) {
    std::string attr_path = cur + "/" + attr_name;
    FILE *f = fopen(attr_path.c_str(), "r");
    if (f != nullptr) {
      char buf[64] = {0};
      char *res = fgets(buf, sizeof(buf), f);
      fclose(f);
      if (res != nullptr) {
        std::string attr_val = buf;
        while (!attr_val.empty() &&
               (attr_val.back() == '\n' || attr_val.back() == '\r'))
          attr_val.pop_back();
        if (attr_val == expected_value)
          return true;
      }
    }

    size_t pos = cur.find_last_of('/');
    if (pos == std::string::npos || pos == 0)
      break;
    cur = cur.substr(0, pos);
  }
  return false;
}

/* Discover the /dev/rpmsg_ctrlN device belonging to the `edge_name`
 * remote-processor edge (e.g. "lpaicp"), matched via the ancestor
 * sysfs attribute "rpmsg_name" (see sysfs_ancestor_attr_matches()).
 * Polls briefly in case the edge hasn't come up yet.
 */
static int find_rpmsg_ctrl_dev_node(const std::string &edge_name,
                                     std::string &out_ctrl_dev_path) {
  for (int attempt = 0; attempt < MAX_RPMSG_EPT_DISCOVER_RETRIES; attempt++) {
    DIR *dir = opendir(RPMSG_CTRL_SYSFS_DIR);
    if (dir != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        string devname = entry->d_name;
        if (devname == "." || devname == "..")
          continue;
        /* ctrl devices share this directory with data channel devices
         * (e.g. "rpmsg_ctrl0" alongside "rpmsg3", "rpmsg4", ...); only
         * consider entries that are actually ctrl devices. */
        if (devname.compare(0, strlen(RPMSG_CTRL_DEV_PREFIX),
                             RPMSG_CTRL_DEV_PREFIX) != 0)
          continue;
        string class_dev_path = string(RPMSG_CTRL_SYSFS_DIR) + "/" + devname;
        if (sysfs_ancestor_attr_matches(class_dev_path, "rpmsg_name",
                                         edge_name,
                                         MAX_RPMSG_CTRL_ANCESTOR_LEVELS)) {
          out_ctrl_dev_path = string("/dev/") + devname;
          closedir(dir);
          return 0;
        }
      }
      closedir(dir);
    }
    usleep(RPMSG_EPT_DISCOVER_POLL_MS * 1000);
  }
  return -ENODEV;
}

int rpmsgSession::create_rpmsg_endpoint(const std::string &chnl_name,
                                        std::string &out_dev_path) {
  string ctrl_dev_path;
  int ret = find_rpmsg_ctrl_dev_node(RPMSG_LPAICP_EDGE_NAME, ctrl_dev_path);
  if (ret != 0) {
    sns_loge("rpmsgSession: could not find rpmsg_ctrl device for edge %s, "
             "ret = %d",
             RPMSG_LPAICP_EDGE_NAME, ret);
    return ret;
  }

  sns_logi("Using RPMSG controller %s", ctrl_dev_path.c_str());

  int ctrl_fd = ::open(ctrl_dev_path.c_str(), O_RDWR);
  if (ctrl_fd < 0) {
    sns_loge("rpmsgSession: open ctrl dev %s failed, errno = %d",
             ctrl_dev_path.c_str(), errno);
    return -errno;
  }

  struct rpmsg_endpoint_info ept_info;
  memset(&ept_info, 0, sizeof(ept_info));

  snprintf(ept_info.name, sizeof(ept_info.name), "%s", chnl_name.c_str());

  ept_info.src = RPMSG_ADDR_ANY;
  ept_info.dst = RPMSG_ADDR_ANY;

  if (ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &ept_info) < 0) {
    sns_loge("RPMSG_CREATE_EPT_IOCTL failed errno=%d (%s)", errno,
             strerror(errno));
    ::close(ctrl_fd);
    return -errno;
  }

  sns_logi("RPMSG_CREATE_EPT_IOCTL success");

  ::close(ctrl_fd);

  ret = find_rpmsg_dev_node(chnl_name, out_dev_path);

  if (ret != 0) {
    sns_loge("Could not discover rpmsg dev node ret=%d", ret);
    return ret;
  }

  sns_logi("Endpoint %s created at %s", chnl_name.c_str(),
           out_dev_path.c_str());

  return 0;
}

void rpmsgSession::destroy_rpmsg_endpoint(int chnl_fd) {
  if (chnl_fd < 0)
    return;
  if (ioctl(chnl_fd, RPMSG_DESTROY_EPT_IOCTL, nullptr) < 0) {
    sns_loge("rpmsgSession: RPMSG_DESTROY_EPT_IOCTL failed on fd = %d, "
             "errno = %d",
             chnl_fd, errno);
  }
}

int rpmsgSession::rpmsg_open() {
  int fd = -1;
  int retry_count = 0;
  bool is_open_flock_success = false;

  for (auto name : _rpmsg_chnls_list) {
    string local_name;
    int create_ret = create_rpmsg_endpoint(name, local_name);
    if (create_ret != 0) {
      sns_loge("rpmsgSession: failed to create/discover rpmsg endpoint for "
               "channel = %s, ret = %d",
               name.c_str(), create_ret);
      continue;
    }
    sns_logi("rpmsgSession about to open rpmsg channel on node = %s",
             local_name.c_str());

    do {
      fd = ::open(local_name.c_str(), O_RDWR | O_NONBLOCK);
      if (fd >= 0)
        break;
      else {
        if (errno == ETIMEDOUT) {
          retry_count++;
          fd = -ETIMEDOUT;
          sns_loge("rpmsgSession rpmsg channel open timeout on node = %s, "
                   "retries count =  %d",
                   local_name.c_str(), retry_count);
          sleep(1);
        } else {
          sns_loge("rpmsgSession open on node = %s failed with errno = %d, %s",
                   local_name.c_str(), errno, strerror(errno));
          fd = -errno;
          break;
        }
      }
    } while (retry_count != MAX_RPMSG_OPEN_RETRIES);

    if (fd == -ETIMEDOUT) {
      break;
    } else if (fd < 0) {
      sns_loge("not able to open channel = %s", local_name.c_str());
      continue;
    } else {
      sns_logi("rpmsgSession: rpmsg open successful, fd = %d", fd);
      if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        sns_loge("flock failed, trying for next channel");
        ::close(fd);
        continue;
      } else {
        sns_logv("flock acquired successfully");
        _chnl_name = name;
        is_open_flock_success = true;
        break;
      }
    }
  }

  if (!is_open_flock_success) {
    sns_loge("rpmsgSession: all rpmsg channels are occupied at the moment");
    return -1;
  }
  return fd;
}

void rpmsgSession::rpmsg_connect(bool is_new_client) {
  sns_logd("rpmsgSession rpmsg_connect called");

  if (_chnl_fd > 0 && is_new_client) {
    sns_logi("rpmsgSession channel is already opened");
    rpmsgSession::_rpmsg_clients++;
    return;
  }

  if ((_chnl_fd = rpmsg_open()) > 0) {
    sns_logi("Rpmsg open successful");
  }

  if (_chnl_fd < 0) {
    throw glink_error(_chnl_fd, "rpmsgSession open failed");
  }
  if (is_new_client) {
    rpmsgSession::_rpmsg_clients++;
  }
  if ((_chnl_fd > 0) && (!_is_thread_created)) {
    /*Create PIPE to send custom message to rpmsg poll thread during channel
     * close */
    if (pipe(_wakeup_pipe) == -1) {
      throw glink_error(_wakeup_pipe[0], "rpmsgSession pipe creation failed");
    }
    _rpmsg_read_threadhdl = std::thread(rpmsg_thread_routine);
    _is_thread_created = true;
    if(nullptr == restore_ssr_conn_hndl_worker)
      restore_ssr_conn_hndl_worker = make_unique<qshWorker>();
    if(nullptr == ind_hndl_worker)
      ind_hndl_worker = make_unique<qshWorker>();
  }
}

int rpmsgSession::rpmsg_read(int fd, uint8_t *buf, size_t buf_len) {
  int ret_val = -1;
  int retry_count = 0;

  sns_logv("rpmsgSession rpmsg_read fd = %d buf_len = %zu", fd, buf_len);
  while (ret_val == -1) {
    ret_val = ::read(fd, buf, buf_len);
    if (ret_val == -1) {
      if (EAGAIN == errno) {
        sns_loge("rpmsgSession read EAGAIN has occured on fd = %d", fd);
        retry_count++;
        if (MAX_RPMSG_READ_RETRIES == retry_count) {
          _rpmsg_err_cnt++;
          sns_loge("rpmsgSession read MAX_RPMSG_READ_RETRIES = %d has finished "
                   "& g_rpmsg_err_cnt = %d",
                   MAX_RPMSG_READ_RETRIES, _rpmsg_err_cnt.load());
          return -EAGAIN;
        }
        continue;
      } else {
        _rpmsg_err_cnt++;
        sns_loge("rpmsgSession read failed with errno = %d, %s", errno,
                 strerror(errno));
        return -errno;
      }
    }
  }

  sns_logv("rpmsgSession complete read bytes = %d", ret_val);
  return ret_val;
}

static bool capture_bytes_cb(pb_istream_t *stream, const pb_field_t * /*field*/,
                             void **arg) {
  auto out = static_cast<std::string *>(*arg);
  uint8_t scratch[256];
  while (stream->bytes_left) {
    size_t n = std::min(sizeof(scratch), (size_t)stream->bytes_left);
    if (!pb_read(stream, scratch, n))
      return false;
    out->append(reinterpret_cast<char *>(scratch), n);
  }
  return true;
}

static bool emit_bytes_cb(pb_ostream_t *stream, const pb_field_t *field,
                          void *const *arg) {
  const auto *in = static_cast<const std::string *>(*arg);
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  return pb_encode_string(
      stream, reinterpret_cast<const pb_byte_t *>(in->data()), in->size());
}

bool sns_ipc_rpmsg_decode_tag_and_check(pb_istream_t *stream,
                                        pb_wire_type_t *wire_type,
                                        uint32_t *tag, bool *eof) {
  if (!pb_decode_tag(stream, wire_type, tag, eof) ||
      *wire_type != PB_WT_STRING) {
    return false;
  }
  if (!(*tag == sns_client_glink_msg_connect_ack_tag ||
        *tag == sns_client_glink_msg_resp_tag ||
        *tag == sns_client_glink_msg_ind_tag)) {
    return false;
  }
  return true;
}

void rpmsgSession::rpmsg_thread_routine() {
  struct pollfd poll_fd[2];
  uint8_t *encoded_msg = nullptr;
  ssize_t ret_val = 0;
  uint8_t ch;
  bool exit = false;

  if (setpriority(PRIO_PROCESS, gettid(), -2) != 0) {
    sns_loge("rpmsg polling thread: setpriority failed with errno = %d", errno);
  } else {
    sns_logi("rpmsg polling thread started with nice -2");
  }

  while (!exit) {
    poll_fd[0].fd = _wakeup_pipe[0];
    poll_fd[0].events = POLLIN;
    poll_fd[1].fd = _chnl_fd;
    poll_fd[1].events = POLLIN | POLLHUP;

    sns_logi("rpmsgSession waiting on poll wakeup_pipe %d rpmsg_fd %d",
             poll_fd[0].fd, poll_fd[1].fd);
    ret_val = poll(poll_fd, 2, POLL_TIMEOUT_IN_MS);
    if (ret_val > 0) {
      if (poll_fd[0].revents & POLLIN) {
        ::read(poll_fd[0].fd, &ch, 1);
        if (ch == 'd') {
          sns_logi("rpmsgSession: explicitly requested thread close");
          exit = true;
        }
      } else if (poll_fd[1].revents & POLLIN) {
        sns_logv("rpmsgSession: received POLLIN event");
        encoded_msg = new uint8_t[SNS_RPMSG_MAX_INDICATION_SIZE];
        if (nullptr == encoded_msg) {
          sns_loge("rpmsgSession: Memory allocation failed for encoded_msg");
          continue;
        }
        ret_val = rpmsg_read(poll_fd[1].fd, encoded_msg,
                             SNS_RPMSG_MAX_INDICATION_SIZE);

        if (ret_val > 0) {

          sns_client_glink_msg pb_glink_msg = sns_client_glink_msg_init_default;
          pb_istream_t stream = pb_istream_from_buffer(
              reinterpret_cast<const pb_byte_t *>(encoded_msg),
              static_cast<size_t>(ret_val));

          uint32_t tag;
          pb_wire_type_t wire_type;
          bool eof;
          uint64_t msg_len;
          if (sns_ipc_rpmsg_decode_tag_and_check(&stream, &wire_type, &tag,
                                                 &eof)) {
            if (pb_decode_varint(&stream, &msg_len)) {
              switch (tag) {
                case sns_client_glink_msg_connect_ack_tag: {

                  /* Ack event is received only for connect request. */
                  sns_logd("rpmsgSession: received connect_ack event ");
                  sns_client_glink_connect_ack connect_ack =
                    sns_client_glink_connect_ack_init_default;
                  if (!pb_decode_noinit(&stream,
                                      sns_client_glink_connect_ack_fields,
                                      &connect_ack)) {
                    sns_loge("connect_ack decode failed");
                  } else {
                    _ack_conn_hndl = connect_ack.has_connection_handle
                                       ? connect_ack.connection_handle
                                       : 0;
                    sns_logi("rpmsgSession: received conn_hndl = %u",
                           connect_ack.has_connection_handle
                               ? connect_ack.connection_handle
                               : 0);
                    _is_req_success.store(true);
                  }
                  if (nullptr != encoded_msg) {
                    delete encoded_msg;
                    encoded_msg = nullptr;
                  }
                } break;

                case sns_client_glink_msg_resp_tag: {
                  sns_client_glink_resp resp = sns_client_glink_resp_init_default;
                  if (!pb_decode(&stream, sns_client_glink_resp_fields, &resp)) {
                    sns_loge("resp decode failed");
                  } else {
                    sns_logd("rpmsgSession: received resp event");
                    sns_logd("rpmsgSession: Acquiring _resp_queue_mutex");
                    _resp_queue_mutex.lock();
                    if (!_resp_queue.empty()) {
                      auto resp_info = _resp_queue.front();
                      _resp_queue.pop_front();
                      _resp_queue_mutex.unlock();
                      sns_logv("rpmsgSession: released _resp_queue_mutex. "
                             "Acquring _rpmsg_db_mutex");
                      _rpmsg_db_mutex.lock();
                      auto conn =
                        get_conn(resp_info.conn_hndl, resp_info.sensor_uid);
                      _rpmsg_db_mutex.unlock();
                      sns_logv("rpmsgSession: released _rpmsg_db_mutex");
                      if (conn == nullptr) {
                        sns_loge(
                          "rpmsgSession: conn is null. Cannot send the resp");
                      } else {
                        if (conn->_conn_status ==
                          rpmsg_conn_status::RPMSG_CONNECTION_STATUS_ACTIVE) {
                        conn->rpmsg_response_handler(resp_info, resp);
                        } else {
                          sns_loge(
                            "rpmsgSession: connection not yet active. Dropping "
                            "packet with conn = 0x%llx and conn_handle %u",
                            (uint64_t)conn, resp_info.conn_hndl);
                        }
                      }
                    } else {
                      sns_loge("rpmsgSession: response queue is empty. Releasing "
                             "_resp_queue_mutex");
                      _resp_queue_mutex.unlock();
                    }
                  }
                  if (nullptr != encoded_msg) {
                    delete encoded_msg;
                    encoded_msg = nullptr;
                  }
                } break;

                case sns_client_glink_msg_ind_tag: {
                  ind_hndl_worker->addTask([stream, wire_type, tag, eof, ret_val, encoded_msg] () mutable {
                    uint64_t event_len = 0;
                    uint32_t conn_handle = 0;
                    if (!pb_decode_tag(&stream, &wire_type, &tag, &eof)) {
                      sns_loge("Msg decode failed tag=%d type=%d", tag, wire_type);
                    } else if (wire_type == PB_WT_32BIT &&
                           tag == sns_client_glink_ind_connection_handle_tag) {
                      if (!pb_decode_fixed32(&stream, &conn_handle)) {
                        sns_loge("Handle decode failed ");
                      } else {
                        if (!pb_decode_tag(&stream, &wire_type, &tag, &eof)) {
                          sns_loge("Msg decode failed tag=%d type=%d", tag,
                               wire_type);
                        }
                        if (wire_type == PB_WT_STRING &&
                          tag == sns_client_glink_ind_event_tag) {
                          if (!pb_decode_varint(&stream, &event_len)) {
                            sns_loge("Event length decode failed ");
                          } else {
                          // Calculate the index of sns_client_event_msg
                            if (ret_val > stream.bytes_left) {

                              size_t index = ret_val - stream.bytes_left;

                              pb_istream_t subStream = pb_istream_from_buffer(
                                reinterpret_cast<const pb_byte_t *>(
                                  (uint8_t *)encoded_msg),
                                static_cast<size_t>(ret_val));
                              sns_client_glink_msg msg =
                              sns_client_glink_msg_init_default;

                              if (!pb_decode(&subStream,
                                         sns_client_glink_msg_fields, &msg)) {
                                sns_loge("rpmsgSession: decoding failed %s",
                                     PB_GET_ERROR(&subStream));
                              }

                              sns_client_glink_ind ind_msg = msg.msg.ind;

                              uint32_t ind_conn_hndl;
                              suid sensors_uid;
                              if (ind_msg.has_connection_handle) {
                                ind_conn_hndl = ind_msg.connection_handle;
                                sns_logd("rpmsgSession: received indication for "
                                     "conn_hndl = %d",
                                     ind_conn_hndl);
                              } else {
                                sns_loge("rpmsgSession: ind does not have "
                                     "connection handle. Cannot notify event.");
                                return;
                              }
                              if (ind_msg.has_event) {
                                if (get_suid(sensors_uid, ind_msg) != -1) {
                                  sns_logv(
                                  "rpmsgSession: acquiring _rpmsg_db_mutex");
                                  _rpmsg_db_mutex.lock();
                                  auto conn = get_conn(ind_conn_hndl, sensors_uid);
                                  _rpmsg_db_mutex.unlock();
                                  sns_logv(
                                  "rpmsgSession: releasing _rpmsg_db_mutex");
                                  if (conn != nullptr) {
                                    if (conn->_conn_status ==
                                      rpmsg_conn_status::
                                        RPMSG_CONNECTION_STATUS_ACTIVE) {
                                      conn->rpmsg_indication_handler(
                                      sensors_uid,
                                      (void*)encoded_msg,
                                      stream.bytes_left, index);
                                    } else
                                      sns_loge("rpmsgSession: connection not yet "
                                           "active. Dropping packet with conn "
                                           "= 0x%llx and conn_handle %u",
                                           (uint64_t)conn, ind_conn_hndl);
                                  }
                                } else {
                                  sns_loge("rpmsgSession: ind does not have value. "
                                       "Cannot notify event.");
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  });
                } break;
                default: break;
              }
            }
          }
        } else {
            sns_loge("rpmsgSession: read has failed with errno = %d, "
                     "g_rpmsg_err_cnt = %d",
                     errno, _rpmsg_err_cnt.load());
            if (nullptr != encoded_msg) {
              delete encoded_msg;
              encoded_msg = nullptr;
            }
        }
      } else if (poll_fd[1].revents & POLLHUP) {
          sns_loge("rpmsgSession: POLLHUP event is received, AON CLIENT SSR "
                   "occured");
          /* If close has acquired the lock while SSR occurs and
             rpmsg_ssr_handler isn't able to acquire the lock and
             rpmsg_disconnect request is sent, then unnecessarily rpmsg_write
             will happen for 150ms as chnl_fd is still valid. Thus, add a check
             to avoid such rpmsg_write and set _chnl_fd to -1.
          */
          _rpmsg_is_ssr_in_progress = true;
          rpmsg_ssr_handler(POLLHUP);
          _rpmsg_is_ssr_in_progress = false;
      } else {
          sns_loge("rpmsgSession received unregistered pollevent = %d",
                   (int)poll_fd[1].revents);
      }
    } else if (ret_val == 0) {
        sns_loge("rpmsgSession: poll time-out");
    } else {
      sns_loge(
            "rpmsgSession: poll failed with errno = %d, g_rpmsg_err_cnt = %d",
            errno, _rpmsg_err_cnt.load());
    }

    if (0 > ret_val) {
        rpmsg_ssr_handler(ret_val);
    } else {
      if (_rpmsg_err_cnt > 0) {
          _rpmsg_err_cnt = 0;
      }
    }
  }
  sns_logd("rpmsgSession thread exit");
}

void rpmsgSession::rpmsg_response_handler(rpmsg_stream_req_info &curr_resp,
                                          sns_client_glink_resp &resp_msg) {
  sns_logd("rpmsgSession: %s start", __func__);
  if (_connection_closed) {
    sns_logi(
        "rpmsgSession response is coming while connection is being closed");
    return;
  }

  sns_logv("rpmsgSession: acquiring _rpmsg_db_mutex");
  _rpmsg_db_mutex.lock();
  auto it = _callback_map_table.find(curr_resp.sensor_uid);
  if (it == _callback_map_table.end()) {
    _rpmsg_db_mutex.unlock();
    sns_logd("rpmsgSession: %s suid NOT found in _callback_map_table table "
             "suid_low=0x%llx suid_high=0x%llx, this=%px",
             __func__, curr_resp.sensor_uid.low, curr_resp.sensor_uid.high,
             this);
    return;
  }
  respCallBack current_resp_cb = it->second.get_resp_cb();

  if (current_resp_cb && resp_msg.has_error && resp_msg.has_connection_handle) {
    uintptr_t handle = reinterpret_cast<uintptr_t>(this);
    uint32_t client_connect_id = static_cast<uint32_t>(handle & 0x7FFFFFFF);
    sns_logd("rpmsgSession: %s suid found in _callback_map_table table and trigerring resp_cb suid_low=0x%llx suid_high=0x%llx conn_hndl=%u, this=%px, client_connect_id = %u",
            __func__, curr_resp.sensor_uid.low, curr_resp.sensor_uid.high, resp_msg.connection_handle, this, client_connect_id);
    current_resp_cb(resp_msg.error, resp_msg.connection_handle);
    if (_logger) {
      const char* dt = LoggerFactory::getDataType(curr_resp.sensor_uid);
      if (dt != nullptr) {
        sns_logd("%s Setting client id to %d", __func__, resp_msg.connection_handle);
        _logger->updateClientId((uint64_t)resp_msg.connection_handle);
        sns_logd("%s Logging response dt=%s value=%lu", __func__, dt, resp_msg.error);
        int rc = _logger->logResponse(dt, resp_msg.error);
        sns_logd("%s LogResponse rc = %d", __func__, rc);
      }
    }
  }

  if (true == curr_resp.is_disable_req) {
    _callback_map_table.erase(it);
    uint32_t conn_hndl = update_clientid_conn_db(curr_resp.sensor_uid);
    _rpmsg_db_mutex.unlock();
    if (0 != conn_hndl)
      send_disconnect_req(curr_resp.sensor_uid, conn_hndl);
  } else {
    _rpmsg_db_mutex.unlock();
  }
  sns_logd("rpmsgSession: %s Ended this=%px", __func__, this);
}

void rpmsgSession::move_stale_conn_hndls_to_restore_db() {
  for (auto it = _clientid_conn_db.begin(); _clientid_conn_db.end() != it;
       it++) {
    uint32_t ssr_conn_hndl = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count() %
        UINT32_MAX);
    sns_logi(
        "rpmsgSession: storing data for conn_hndl = %u with conn_hndl = %u",
        it->first, ssr_conn_hndl);
    _restore_ssr_conn_db[ssr_conn_hndl] = _clientid_conn_db[it->first];
  }
  _clientid_conn_db.clear();
  _suid_conn_handles_db.clear();
}

uint64_t rpmsgSession::get_sample_timestamp_ns()
{
#ifdef __ANDROID_API__
    return android::elapsedRealtimeNano();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
#endif
}

void rpmsgSession::rpmsg_ssr_handler(int error) {
  sns_logi("rpmsgSession: rpmsg_ssr_handler received error = %d", error);
  if (error == POLLHUP) {
    /* Avoid deadlock between close / destructor thread.join() and SSR
       handling. rpmsg_disconnect can close the channel to avoid scenarios
       where _clientid_db is doing rpmsg_write after SSR has happened. Thus,
       there is a possibility that rpmsg_open might acquire the lock and it
       will keep on holding the lock for max 30sec. Thus, increase retry_cnt
       to accomodate 30sec.
    */
    int retry_cnt = 5;
    bool lock_acquired = false;
    bool rpmsg_open_success = false;

    /* If SSR happens, opened rpmsg channel has to be closed as it holds stale
     * fd */
    rpmsg_disconnect(false);
    _resp_queue_mutex.lock();
    _resp_queue.clear();
    _resp_queue_mutex.unlock();

    while (retry_cnt--) {
      sns_logv("rpmsgSession: trying to acquire _rpmsg_open_close_mutex");
      if (false == _rpmsg_open_close_mutex.try_lock()) {
        /* If destructor / close has acquired the lock and active rpmsg
           clients become 0, then no need to do SSR handling as there is no
           active client.
        */
        if (0 >= rpmsgSession::_rpmsg_clients) {
          rpmsg_db_cleanup();
          _rpmsg_is_ssr_in_progress = false;
          return;
        }
        /* Stall the thread for few ms before trying for the lock again */
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      } else {
        lock_acquired = true;
        break;
      }
    }
    if (0 >= rpmsgSession::_rpmsg_clients) {
      rpmsg_db_cleanup();
      if (true == lock_acquired) {
        _rpmsg_open_close_mutex.unlock();
        sns_logv("rpmsgSession: released _rpmsg_open_close_mutex");
      }
      _rpmsg_is_ssr_in_progress = false;
      return;
    }

    sns_logv("rpmsgSession: Acquiring _rpmsg_db_mutex");
    _rpmsg_db_mutex.lock();

    for (auto &it : _clientid_conn_db) {
      auto &suid_conn_map = it.second;
      for (auto itr : suid_conn_map) {
        itr.second->_reconnecting = true;
      }
    }
    _suid_conn_handles_db.clear();
    _rpmsg_db_mutex.unlock();
    sns_logv("rpmsgSession: Released _rpmsg_db_mutex");

    /* Retry is required as SWM might take more than 30sec to come up after
     * SSR
     */
    retry_cnt = 6;
    if (false == lock_acquired) {
      sns_logv("rpmsgSession: trying to acquire _rpmsg_open_close_mutex");
      _rpmsg_open_close_mutex.lock();
      lock_acquired = true;
    }
    while (retry_cnt--) {
      try {
        sns_logd("rpmsgSession: rpmsg error, trying to reconnect");
        rpmsg_connect(false);
        _rpmsg_is_ssr_in_progress = false;
        rpmsg_open_success = true;
        sns_logi("rpmsgSession: connection re-established");
        break;
      } catch (const exception &e) {
        sns_loge("rpmsgSession: could not reconnect: %s. Trying to connect %d "
                 "more times",
                 e.what(), retry_cnt);
      }
    }

    /* Polling thread needs to be exited in case we are not able to connect to
       SWM after SSR. Otherwise, thread will be blocked indefinitely as thread
       will be polling on chnl_fd -1. Hence, clean up is essential.
    */
    if (false == rpmsg_open_success) {
      _rpmsg_is_ssr_in_progress = false;
      _rpmsg_db_mutex.lock();
      move_stale_conn_hndls_to_restore_db();
      _rpmsg_db_mutex.unlock();
      sns_logv("rpmsgSession: Released _rpmsg_db_mutex");
      _rpmsg_open_close_mutex.unlock();
      sns_logv("rpmsgSession: Released _rpmsg_open_close_mutex");
      restore_ssr_conn_hndl_worker->addTask([] {
        if (::write(_wakeup_pipe[1], "d", 1) < 0) {
          sns_loge("rpmsgSession: write err on wakup pipe errrno = %d no way "
                   "to join readthread",
                   errno);
        }
        _rpmsg_read_threadhdl.join();
        _is_thread_created = false;
        ::close(_wakeup_pipe[0]);
        ::close(_wakeup_pipe[1]);
      });
      return;
    }

    /* Avoid sending conn_hndl request and receiving conn_hndl in the same
     * thread */
    restore_ssr_conn_hndl_worker->addTask([] {
      sns_logv("rpmsgSession: Acquiring _rpmsg_db_mutex");
      _rpmsg_db_mutex.lock();
      fetch_new_conn_hndls_after_ssr();

      for (auto &it : _clientid_conn_db) {
        auto &suid_conn_map = it.second;
        for (auto itr : suid_conn_map) {
          rpmsgSession *conn = itr.second;
          conn->_worker->addTask([conn] {
            conn->_reconnecting = false;
            if (conn->_connection_closed == true) {
              sns_logd(
                  "rpmsgSession: conn = 0x%llx sensor deactivated during ssr",
                  (uint64_t)conn);
              return;
            }
            for (auto iter = conn->_callback_map_table.begin();
                 iter != conn->_callback_map_table.end(); ++iter) {
              errorCallBack err_cb = (iter->second).get_error_cb();
              if (nullptr == err_cb) {
                continue;
              }
              err_cb(ISession::RESET);
            }
          });
        }
      }
      _rpmsg_db_mutex.unlock();
      sns_logv("rpmsgSession: Released _rpmsg_db_mutex");
    });
    _rpmsg_open_close_mutex.unlock();
    sns_logv("rpmsgSession: released _rpmsg_open_close_mutex");
  } else {
    _rpmsg_err_cnt++;
    if ((_rpmsg_err_cnt > RPMSG_ERRORS_HANDLED_ATTEMPTS) && !triggerSSR()) {
      sns_logd("rpmsgSession: triggred ssr g_rpmsg_err_cnt = %d",
               _rpmsg_err_cnt.load());
      _rpmsg_err_cnt = 0;
    }
  }
}

void rpmsgSession::fetch_new_conn_hndls_after_ssr() {
  sns_logv("rpmsgSession: Starting conn_handle remapping after SSR");
  auto temp_clientid_db = _clientid_conn_db;
  auto temp_ssr_conn_db = _restore_ssr_conn_db;
  _clientid_conn_db.clear();
  _restore_ssr_conn_db.clear();

  for (auto it = temp_clientid_db.begin(); temp_clientid_db.end() != it; it++) {
    auto &conn_hndl = it->first;
    auto &suid_session_map = it->second;

    /* After SSR, all the sensors are discovered again. Sensor discovery
       happens within sensor handle after SSR. Thus, new conn hndls can be
       present in the clientid_db having suid request for discovering
       sensors for previously active sensor handle. Setting callabcks for
       sensors whose suid changes dynamically after SSR like gyro_cal,
       new callbacks can be mapped to any of these handles. Beacause of
       this behaviour, unqiue suids can be distributed over different
       conn hndls. Hence, fetch conn hndls per suid after SSR.
    */
    for (auto itr = suid_session_map.begin(); suid_session_map.end() != itr;) {
      rpmsgSession *conn = itr->second;
      conn->_reconnecting = false;
      uint32_t new_conn_hndl = conn->check_available_conn_hndl(itr->first);
      if (0 == new_conn_hndl) {
        new_conn_hndl = conn->fetch_new_conn_hndl(itr->first);
      }
      if (0 == new_conn_hndl) {
        sns_loge("rpmsgSession: failed to get new conn_handle");
        uint32_t ssr_conn_hndl = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
                .count() %
            UINT32_MAX);
        sns_logi("rpmsgSession: storing data with conn_hndl = %u",
                 ssr_conn_hndl);
        _restore_ssr_conn_db[ssr_conn_hndl] = suid_session_map;
        break;
      } else {
        sns_logi("rpmsgSession: mapping conn_hndl %u to new conn_hndl %u for "
                 "suid.low %llx",
                 conn_hndl, new_conn_hndl, itr->first.low);
        _suid_conn_handles_db[itr->first].push_back(new_conn_hndl);
        _clientid_conn_db[new_conn_hndl][itr->first] = itr->second;
        itr = suid_session_map.erase(itr);
      }
    }
  }

  /* Try to restart suids which could not be restarted after previous SSR */
  for (auto it = temp_ssr_conn_db.begin(); temp_ssr_conn_db.end() != it; it++) {
    auto &conn_hndl = it->first;
    sns_logd("rpmsgSesson: trying to recover conn_hndl %u", conn_hndl);
    auto &suid_session_map = it->second;

    for (auto itr = suid_session_map.begin(); suid_session_map.end() != itr;) {
      auto conn = itr->second;
      uint32_t new_conn_hndl = conn->check_available_conn_hndl(itr->first);
      if (0 == new_conn_hndl) {
        new_conn_hndl = conn->fetch_new_conn_hndl(itr->first);
      }
      sns_logi("rpmsgSession: received new conn_hndl = %u", new_conn_hndl);
      if (0 == new_conn_hndl) {
        sns_loge("rpmsgSession: failed to get new conn_hndl for conn_hndl %u "
                 "and suid.low %llx",
                 conn_hndl, itr->first.low);
        _restore_ssr_conn_db[conn_hndl] = suid_session_map;
        break;
      } else {
        _suid_conn_handles_db[itr->first].push_back(new_conn_hndl);
        _clientid_conn_db[new_conn_hndl][itr->first] = itr->second;
        itr = suid_session_map.erase(itr);
      }
    }
  }
  sns_logv("rpmsgSession: Finished fetching new conn_handle after SSR");
}

void rpmsgSession::rpmsg_disconnect(bool thread_exit) {
  sns_logv("rpmsgSession: Entered %s", __func__);
  if (thread_exit) {
    if (rpmsgSession::_rpmsg_clients > 0) {
      sns_logi("rpmsgSession: Not the last conn. Do not close the chnl");
      return;
    }
  }

  if (-1 != _chnl_fd) {
    destroy_rpmsg_endpoint(_chnl_fd);
    if (::close(_chnl_fd) != 0) {
      sns_loge("rpmsgSession: close on _s_chnl_fd = %d failed with errno = %d",
               _chnl_fd.load(), errno);
    }
  }
  sns_logi("rpmsgSession: channel closed on fd = %d", _chnl_fd.load());
  _chnl_fd = -1;
  _chnl_name = "";

  if (thread_exit) {
    rpmsg_db_cleanup();
    if (::write(_wakeup_pipe[1], "d", 1) < 0) {
      sns_loge("rpmsgSession: write err on wakup pipe errrno = %d no way to "
               "join readthread",
               errno);
    } else {
      _rpmsg_read_threadhdl.join();
      _is_thread_created = false;
      restore_ssr_conn_hndl_worker.reset();
      ind_hndl_worker.reset();
      ::close(_wakeup_pipe[0]);
      ::close(_wakeup_pipe[1]);
    }
  }
}

int rpmsgSession::get_suid(suid &out_suid, sns_client_glink_ind &ind_msg) {
  sns_client_event_msg pb_event_msg = ind_msg.event;

  sns_logv("rpmsgSession: Done parsing ind message");
  out_suid.low = pb_event_msg.suid.suid_low;
  out_suid.high = pb_event_msg.suid.suid_high;
  return 0;
}

void rpmsgSession::rpmsg_indication_handler(suid &sensor_uid, void *buf,
                                            size_t buf_len, size_t index) {
  sns_logd("rpmsgSession: conn = 0x%llx received indication", (uint64_t)this);
  if (_connection_closed) {
    sns_logd("rpmsgSession: conn = 0x%llx indication is coming while "
             "connection is being closed",
             (uint64_t)this);
    return;
  }

  uint64_t sample_received_ts = get_sample_timestamp_ns();
  /* Add event callback to a worker thread as clients may send new request in
   * the same thread */
  if (nullptr != _worker) {
    _worker->addTask([this, buf, buf_len, sample_received_ts, sensor_uid, index] {
      // learn datatype from SUID + ATTR events in the indication payload.
      updateDataTypeFromEventPayload(sensor_uid, (uint8_t *)buf + index, buf_len);

      if (_logger) {
        const char* dt = LoggerFactory::getDataType(sensor_uid);
        if (dt != nullptr)
          _logger->logEvent(dt, (uint8_t*)buf + index, buf_len);
      }

      sns_logv("rpmsgSession: acquiring _rpmsg_db_mutex");
      _rpmsg_db_mutex.lock();
      auto it = _callback_map_table.find(sensor_uid);
      if (it == _callback_map_table.end()) {
        _rpmsg_db_mutex.unlock();
        sns_loge("rpmsgSession: No callbacks registered for suid_low=0x%llx "
                 "suid_high=0x%llx",
                 sensor_uid.low, sensor_uid.high);
        return;
      }
      eventCallBack current_event_cb = it->second.get_event_cb();
      if (nullptr == current_event_cb) {
        _rpmsg_db_mutex.unlock();
        sns_loge("rpmsgSession: event cb is not registered");
        return;
      }

      _rpmsg_db_mutex.unlock();
      /* While hitting the callback, client can call new callback in the same
         thread. Thus, keep it out of any mutex for other call flows to work
         seamlessly.
      */
      current_event_cb((uint8_t*)buf + index, buf_len, sample_received_ts);
      delete[] (uint8_t*)buf;
      sns_logv("rpmsgSession: triggered event callback for datatype = 0x%llx",
               sensor_uid.low);
    });
  }
}

uint32_t rpmsgSession::find_free_conn_handle(suid sensor_uid) {
  auto it = _clientid_conn_db.begin();
  if (_clientid_conn_db.end() == it) {
    return 0;
  }

  if (_suid_conn_handles_db.count(sensor_uid) == 0) {
    return it->first;
  }

  auto conn_hndls = _suid_conn_handles_db[sensor_uid];
  for (; it != _clientid_conn_db.end(); it++) {
    uint32_t hndl = it->first;
    if (find(conn_hndls.begin(), conn_hndls.end(), hndl) == conn_hndls.end()) {
      return hndl;
    }
  }
  return 0;
}

void rpmsgSession::update_suid_conn_hndl_db(uint32_t hndl, suid &sensor_uid) {
  if (_suid_conn_handles_db.count(sensor_uid) == 0)
    return;

  try {
    auto &hndls = _suid_conn_handles_db.at(sensor_uid);
    auto it = find(hndls.begin(), hndls.end(), hndl);
    if (it != hndls.end()) {
      hndls.erase(it);
      sns_logd("rpmsgSession: erased hndl = %u from suid_low=0x%llx", hndl,
               sensor_uid.low);
      if (hndls.size() == 0) {
        auto itr = _suid_conn_handles_db.find(sensor_uid);
        if (itr != _suid_conn_handles_db.end())
          _suid_conn_handles_db.erase(itr);
      }
    }
  } catch (const out_of_range &ex) {
    sns_loge("rpmsgSession: conn_hndl %u does not exist for suid_low=0x%llx "
             "suid_high=0x%llx",
             hndl, sensor_uid.low, sensor_uid.high);
  }
  sns_logd("Exiting %s", __func__);
}

void rpmsgSession::send_disconnect_req(const suid &sensor_uid,
                                       uint32_t conn_hndl) {
  sns_logd("rpmsgSession: Sending disconnect req for conn_hndl = %d",
           conn_hndl);
  sns_client_glink_msg pb_glink_msg = sns_client_glink_msg_init_default;
  pb_glink_msg.which_msg = sns_client_glink_msg_disconnect_tag;

  sns_client_glink_disconnect glink_disconnect =
      sns_client_glink_disconnect_init_default;
  glink_disconnect.has_connection_handle = true;
  glink_disconnect.connection_handle = conn_hndl;
  pb_glink_msg.msg.disconnect = glink_disconnect;

  pb_byte_t encoded_buffer[100];
  pb_ostream_t stream =
      pb_ostream_from_buffer(encoded_buffer, sizeof(encoded_buffer));
  if (!pb_encode(&stream, sns_client_glink_msg_fields, &pb_glink_msg)) {
    sns_loge("rpmsgSession: Failed to encode sns_client_glink_msg: %s",
             PB_GET_ERROR(&stream));
    return;
  }
  sns_logd(
      "rpmsgSession: Encoded sns_client_glink_msg successfully (%zu bytes)",
      stream.bytes_written);
  std::string pb_glink_req_encoded(reinterpret_cast<char *>(encoded_buffer),
                                   stream.bytes_written);
  send_rpmsg_req(sensor_uid, pb_glink_req_encoded, false);
  sns_logd("rpmsgSession: Exited %s", __func__);
}

rpmsgSession *rpmsgSession::get_conn(uint32_t &conn_hndl, suid &sensor_uid) {
  rpmsgSession *conn = nullptr;
  try {
    auto &suid_conn_mp = _clientid_conn_db.at(conn_hndl);
    try {
      conn = suid_conn_mp.at(sensor_uid);
    } catch (const out_of_range &ex) {
      sns_loge("rpmsgSession: conn object does not exist for conn_hndl %u and "
               "suid_low=0x%llx suid_high=0x%llx",
               conn_hndl, sensor_uid.low, sensor_uid.high);
      return nullptr;
    }
  } catch (const out_of_range &ex) {
    sns_loge("rpmsgSession: conn_hndl %u does not exist for suid_low=0x%llx "
             "suid_high=0x%llx",
             conn_hndl, sensor_uid.low, sensor_uid.high);
    return nullptr;
  }
  return conn;
}

void rpmsgSession::rpmsg_db_cleanup() {
  _resp_queue_mutex.lock();
  _resp_queue.clear();
  _resp_queue_mutex.unlock();
  sns_logv(
      "rpmsgSession: acquiring _rpmsg_db_mutex. Clearing clientid_conn_db");
  _rpmsg_db_mutex.lock();
  _clientid_conn_db.clear();
  _suid_conn_handles_db.clear();
  _restore_ssr_conn_db.clear();
  _rpmsg_db_mutex.unlock();
  sns_logv("rpmsgSession: released _rpmsg_db_mutex");
}

/* Clears the database when any client goes away */
uint32_t rpmsgSession::update_clientid_conn_db(suid sensor_uid) {
  uint32_t conn_hndl = 0;
  sns_logd("rpmsgSession: Entering %s", __func__);
  for (auto it = _clientid_conn_db.begin(); it != _clientid_conn_db.end();) {
    bool entry_updated = false;
    auto &suid_conn_mp = it->second;
    sns_logd("rpmsgSession: conn_hndl = %u", it->first);
    auto itr = suid_conn_mp.find(sensor_uid);
    if (itr != suid_conn_mp.end() && this == itr->second) {
      suid_conn_mp.erase(itr);
      sns_logd("Erasing entry for conn 0x%llx from _clientid_conn_db",
               (uint64_t)this);
      update_suid_conn_hndl_db(it->first, sensor_uid);
      if (0 == suid_conn_mp.size()) {
        conn_hndl = it->first;
        sns_logd("Erasing entry for conn_hndl %u from _clientid_conn_db",
                 it->first);
        it = _clientid_conn_db.erase(it);
        entry_updated = true;
      }
      break;
    }
    if (false == entry_updated)
      it++;
  }

  /* Check and remove entry for suid, rpmsg handle if it exists */
  for (auto it = _restore_ssr_conn_db.begin();
       it != _restore_ssr_conn_db.end();) {
    bool entry_updated = false;
    auto &suid_conn_mp = it->second;
    sns_logd("rpmsgSession: conn_hndl to be removed from _restore_ssr_conn_db "
             "= %u for suid.low = 0x%llx",
             it->first, sensor_uid.low);
    auto itr = suid_conn_mp.find(sensor_uid);
    if (itr != suid_conn_mp.end() && this == itr->second) {
      suid_conn_mp.erase(itr);
      sns_logd("Erasing entry for conn 0x%llx from _restore_ssr_conn_db",
               (uint64_t)this);
      if (0 == suid_conn_mp.size()) {
        sns_logd("Erasing entry from _restore_ssr_conn_db");
        it = _restore_ssr_conn_db.erase(it);
        entry_updated = true;
      }
    }
    if (false == entry_updated)
      it++;
  }
  sns_logd("rpmsgSession: Exiting %s", __func__);
  return conn_hndl;
}

uint32_t rpmsgSession::fetch_new_conn_hndl(suid sensor_uid) {
  uint32_t conn_handle = 0;
  /* request new conn handle */

  uint8_t buffer[128];
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
  sns_client_glink_msg pb_glink_msg = sns_client_glink_msg_init_zero;
  pb_glink_msg.which_msg = sns_client_glink_msg_connect_tag;
  pb_glink_msg.msg.connect = sns_client_glink_connect_init_zero;

  bool status = pb_encode(&stream, sns_client_glink_msg_fields, &pb_glink_msg);

  if (!status) {
    sns_loge("Encoding failed: %s\n", PB_GET_ERROR(&stream));
  } else {
    sns_logi("Encoded %zu bytes\n", stream.bytes_written);
  }

  std::string pb_glink_msg_encoded(reinterpret_cast<char *>(buffer),
                                   stream.bytes_written);

  _ack_conn_hndl = 0;
  _is_req_success.store(false);
  if (send_rpmsg_req(sensor_uid, pb_glink_msg_encoded, false) != -1) {
    sns_logi("rpmsgSession: Request sent. Waiting for ack event");
    bool timeout = true;
    int max_retry = SNS_RPMSG_ACK_TIMEOUT_MAX_RETRY;
    do {
      if (_is_req_success.load()) {
        _is_req_success.store(false);
        timeout = false;
        conn_handle = _ack_conn_hndl;
        sns_logi("rpmsgSession: assigning conn_hndl = %u", conn_handle);
        _ack_conn_hndl = 0;
        break;
      } else {
        this_thread::sleep_for(chrono::milliseconds(SNS_RPMSG_ACK_TIMEOUT_MS));
      }
      max_retry--;
    } while (max_retry > 0);

    if (timeout) {
      sns_loge("Timed out waiting for conn ack event");
      _is_req_success.store(false);
      _ack_conn_hndl = 0;
      conn_handle = 0;
    }
  }
  return conn_handle;
}

uint32_t rpmsgSession::check_available_conn_hndl(suid sensor_uid) {
  sns_logd("rpmsgSession: %s called", __func__);
  uint32_t conn_handle = 0;

  if (_clientid_conn_db.size() == 0 ||
      ((_suid_conn_handles_db.count(sensor_uid) != 0) &&
       (_suid_conn_handles_db[sensor_uid].size() ==
        _clientid_conn_db.size()))) {
    return 0;
  } else {
    conn_handle = find_free_conn_handle(sensor_uid);
  }
  sns_logd("rpmsgSession: %s ended", __func__);
  return conn_handle;
}

int rpmsgSession::setCallBacks(suid sensor_uid, ISession::respCallBack resp_cb,
                               ISession::errorCallBack error_cb,
                               ISession::eventCallBack event_cb) {
  sns_logd("rpmsgSession: %s start this=0x%llx", __func__, (uint64_t)this);
  uint32_t conn_hndl = 0;

  /* While doing setCallbacks, conn_hndl is fetched.
     At a time, only one conn_hndl should be fetched.
  */
  lock_guard<mutex> lk(_rpmsg_setcb_mutex);
  sns_logv("rpmsgSession: Acquiring _rpmsg_db_mutex");
  _rpmsg_db_mutex.lock();
  auto it = _callback_map_table.find(sensor_uid);
  if (it == _callback_map_table.end()) {
    sns_logi("rpmsgSession: suid(low=0x%llx, high=0x%llx) is new. Register "
             "callbacks",
             sensor_uid.low, sensor_uid.high);
    if (resp_cb == nullptr && error_cb == nullptr && event_cb == nullptr) {
      _rpmsg_db_mutex.unlock();
      sns_loge("rpmsgSession: all callbacks are null for new suid. No need to "
               "register it.");
      return -1;
    }
    conn_hndl = check_available_conn_hndl(sensor_uid);
    if (0 == conn_hndl) {
      _rpmsg_db_mutex.unlock();
      sns_logv("rpmsgSession: released _rpmsg_db_mutex");
      conn_hndl = fetch_new_conn_hndl(sensor_uid);
      if (0 == conn_hndl) {
        sns_loge("rpmsgSession: Invalid conn_hndl = %u", conn_hndl);
        return -1;
      }
      sns_logv("rpmsgSession: acquiring _rpmsg_db_mutex");
      _rpmsg_db_mutex.lock();
    }
    sns_logi("rpmsgSession conn_hndl = %u", conn_hndl);
    qsh_register_cb cb(resp_cb, error_cb, event_cb);
    _callback_map_table.insert(
        std::pair<suid, qsh_register_cb>(sensor_uid, cb));
    _clientid_conn_db[conn_hndl][sensor_uid] = this;
    _suid_conn_handles_db[sensor_uid].push_back(conn_hndl);
    _rpmsg_db_mutex.unlock();
    sns_logv("rpmsgSession: released _rpmsg_db_mutex");
  } else {
    sns_logi(
        "rpmsgSession: suid(low=0x%llx, high=0x%llx) is already registered",
        sensor_uid.low, sensor_uid.high);
    if (resp_cb == nullptr && error_cb == nullptr && event_cb == nullptr) {
      sns_logi("rpmsgSession: all callbacks are null, unregister it");
      _callback_map_table.erase(it);
      uint32_t conn_hndl = update_clientid_conn_db(sensor_uid);
      _rpmsg_db_mutex.unlock();
      if (0 != conn_hndl) {
        send_disconnect_req(sensor_uid, conn_hndl);
      }
      return -1;
    } else {
      sns_logi("rpmsgSession: update callbacks with new one");
      qsh_register_cb cb(resp_cb, error_cb, event_cb);
      it->second = cb;
      _rpmsg_db_mutex.unlock();
    }
  }
  sns_logd("rpmsgSession: %s completed, this=%px ", __func__, this);
  return 0;
}

int rpmsgSession::rpmsg_write(int fd, uint8_t *buf, size_t buf_len) {
  ssize_t ret_val = 0;
  int write_bytes = 0;
  int retry_count = 0;

  sns_logd("rpmsgSession conn = 0x%llx write fd = %d buf_len = %zu, waiting "
           "for _rpmsg_write_mutex",
           (uint64_t)this, fd, buf_len);

  /* One write should finish before other write can proceed */
  lock_guard<mutex> lk(_rpmsg_write_mutex);
  if (_chnl_fd == -1 || true == _rpmsg_is_ssr_in_progress) {
    sns_loge("rpmsgSession conn = 0x%llx write is not permitted as _s_chnl_fd "
             "is -1 or SSR in-progress",
             (uint64_t)this);
    return -1;
  }
#ifdef __ANDROID_API__
  QSH_TRACE_BEGIN("sensors::rpmsg_write");
#endif
  if(_wakelock_inst != nullptr)
  {
    sns_logd("rpmsgSession acquiring wakelock");
    _wakelock_inst->acquire(1);
  } else {
    sns_loge("rpmsgSession wakelock instance is null - continuing without "
             "suspend protection");
  }

  if (_conn_status == rpmsg_conn_status::RPMSG_CONNECTION_STATUS_INIT)
    _conn_status = rpmsg_conn_status::RPMSG_CONNECTION_STATUS_ACTIVE;

  while ((buf_len != 0) && (-1 != _chnl_fd) &&
         ((ret_val = ::write(fd, buf, buf_len)) != 0)) {
    if (-1 == ret_val) {
      if (EAGAIN == errno || (EBUSY == errno)) {
        retry_count++;
        sns_loge("rpmsgSession conn = 0x%llx write EAGAIN has occured",
                 (uint64_t)this);
        if (MAX_RPMSG_WRITE_RETRIES == retry_count) {
          _rpmsg_err_cnt++;
          sns_loge("rpmsgSession conn = 0x%llx write MAX_RPMSG_WRITE_RETRIES = "
                   "%d has finished on fd = %d & g_rpmsg_err_cnt = %d",
                   (uint64_t)this, MAX_RPMSG_WRITE_RETRIES, fd,
                   _rpmsg_err_cnt.load());
          if (_wakelock_inst != nullptr) {
            sns_logd("rpmsgSession releasing wakelock");
            _wakelock_inst->release(1);
          }
#ifdef __ANDROID_API__
          QSH_TRACE_END();
#endif

          if (_conn_status == rpmsg_conn_status::RPMSG_CONNECTION_STATUS_ACTIVE)
            _conn_status = rpmsg_conn_status::RPMSG_CONNECTION_STATUS_INIT;
          return -errno;
        }
        usleep(5 * 1000);
        continue;
      } else {
        _rpmsg_err_cnt++;
        sns_loge("rpmsgSession conn = 0x%llx write has failed with errno = %d "
                 "& g_rpmsg_err_cnt = %d",
                 (uint64_t)this, errno, _rpmsg_err_cnt.load());
        if (_wakelock_inst != nullptr) {
          sns_logd("rpmsgSession releasing wakelock");
          _wakelock_inst->release(1);
        }
#ifdef __ANDROID_API__
        QSH_TRACE_END();
#endif
        if(_conn_status == rpmsg_conn_status::RPMSG_CONNECTION_STATUS_ACTIVE)
          _conn_status = rpmsg_conn_status::RPMSG_CONNECTION_STATUS_INIT;
        return -errno;
      }
    }
    sns_logd("rpmsgSession conn = 0x%llx partial bytes written= %d",
             (uint64_t)this, ret_val);
    buf_len -= ret_val;
    buf += ret_val;
    write_bytes += ret_val;
  }

  sns_logv("rpmsgSession conn = 0x%llx complete bytes written= %d retries %d",
           (uint64_t)this, write_bytes, retry_count);
  if (_wakelock_inst != nullptr) {
    sns_logd("rpmsgSession releasing wakelock");
    _wakelock_inst->release(1);
  }
#ifdef __ANDROID_API__
  QSH_TRACE_END();
#endif
  return ret_val;
}

uint32_t rpmsgSession::find_assigned_conn_handle(suid sensor_uid) {
  for (auto it = _clientid_conn_db.begin(); it != _clientid_conn_db.end();
       it++) {
    auto &suid_conn_map = it->second;
    if (suid_conn_map.count(sensor_uid) != 0 &&
        suid_conn_map.at(sensor_uid) == this) {
      return it->first;
    }
  }
  return 0;
}

int rpmsgSession::send_rpmsg_req(suid sensor_uid, std::string encoded_buffer,
                                 bool is_resp_expected) {
  int ret_val = 0;
  bool is_disable_req = false;

  if (_reconnecting) {
    sns_loge("rpmsgSession: conn = 0x%llx is reconnecting, cannot send request",
             (uint64_t)this);
    return -1;
  }
  sns_logd("rpmsgSession: conn = 0x%llx is sending request", (uint64_t)this);
  if ((encoded_buffer.size()) > SNS_RPMSG_MAX_REQUEST_SIZE) {
    sns_loge("rpmsgSession: conn = 0x%llx request payload size is too large",
             (uint64_t)this);
    throw runtime_error("rpmsgSession request payload size too large");
  }

  /* Convert const uint8_t* to uint8_t* */
  const uint8_t* const_arr = reinterpret_cast<const uint8_t*>(encoded_buffer.c_str());
  uint8_t* arr = const_cast<uint8_t*>(const_arr);
  uint64_t start_ts_nsec = get_sample_timestamp_ns();
  ret_val = rpmsg_write(_chnl_fd, arr, encoded_buffer.length());
  if(ret_val < 0)
  {
    sns_loge("rpmsgSession: conn = 0x%llx rpmsg_write has failed", (uint64_t)this);
    if(is_resp_expected)
    {
      sns_logd("rpmsgSession: Acquiring _resp_queue_mutex");
      _resp_queue_mutex.lock();
      _resp_queue.pop_back();
      _resp_queue_mutex.unlock();
    }
    rpmsg_ssr_handler(ret_val);
    return -1;
  }

  uint64_t stop_ts_nsec = get_sample_timestamp_ns();
  if((stop_ts_nsec - start_ts_nsec) > 2*NSEC_PER_SEC)
  {
    sns_logi("rpmsgSession: conn = 0x%llx time taken to complete rpmsg_write is %lf secs",
              (uint64_t)this, (double)((stop_ts_nsec - start_ts_nsec)/(NSEC_PER_SEC)));
  }
  return 0;
}

int rpmsgSession::sendRequest(suid sensor_uid, std::string encoded_buffer) {
  sns_logd("rpmsgSession: conn = 0x%llx is sending request", (uint64_t)this);
  sns_client_request_msg pb_req_msg;
  rpmsg_stream_req_info req_info;
  bool is_disable_req;
  uint8_t *buf;

  sns_logv("rpmsgSession: acquiring _rpmsg_db_mutex");
  _rpmsg_db_mutex.lock();
  uint32_t conn_hndl = find_assigned_conn_handle(sensor_uid);
  _rpmsg_db_mutex.unlock();
  sns_logv("rpmsgSession: released _rpmsg_db_mutex");
  if (0 == conn_hndl) {
    sns_loge("rpmsgSession: conn_hndl = 0 for suid low 0x%llx", sensor_uid.low);
    return -1;
  }
  std::string request_payload_bytes;
  sns_client_request_msg pb_req_msg_decoded =
      sns_client_request_msg_init_default;
  pb_req_msg_decoded.request.payload.funcs.decode = &capture_bytes_cb;
  pb_req_msg_decoded.request.payload.arg = &request_payload_bytes;
  pb_istream_t stream = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t *>(encoded_buffer.data()),
      static_cast<size_t>(encoded_buffer.size()));
  if (!pb_decode(&stream, sns_client_request_msg_fields, &pb_req_msg_decoded)) {
    sns_loge("rpmsgSession: first level suid decoding failed %s",
             PB_GET_ERROR(&stream));
    return -1;
  }
  sns_logd("rpmsgSession: conn = 0x%llx, session_id = %d, low.suid %llx, msgid "
           "= %d ",
           (uint64_t)this, pb_req_msg_decoded.susp_config.delivery_type,
           pb_req_msg_decoded.suid.suid_low, pb_req_msg_decoded.msg_id);
  is_disable_req =
      (SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ == pb_req_msg_decoded.msg_id)
          ? true
          : false;
  req_info.sensor_uid.low = sensor_uid.low;
  req_info.sensor_uid.high = sensor_uid.high;
  req_info.is_disable_req = is_disable_req;
  req_info.conn_hndl = conn_hndl;

  sns_logd("rpmsgSession: Acquiring _resp_queue_mutex");
  _resp_queue_mutex.lock();
  _resp_queue.push_back(req_info);
  _resp_queue_mutex.unlock();

  pb_byte_t encoded_payload[200];
  pb_ostream_t payload_stream =
      pb_ostream_from_buffer(encoded_payload, sizeof(encoded_payload));
  sns_client_glink_msg pb_glink_msg = sns_client_glink_msg_init_default;

  pb_glink_msg.which_msg = sns_client_glink_msg_req_tag;
  pb_glink_msg.msg.req.has_connection_handle = true;
  pb_glink_msg.msg.req.connection_handle = conn_hndl;
  pb_glink_msg.msg.req.has_request = true;
  pb_glink_msg.msg.req.request = pb_req_msg_decoded;
  pb_glink_msg.msg.req.request.request.payload.funcs.encode = &emit_bytes_cb;
  pb_glink_msg.msg.req.request.request.payload.arg = &request_payload_bytes;
  if (!pb_encode(&payload_stream, sns_client_glink_msg_fields, &pb_glink_msg)) {
    sns_loge("rpmsgSession: pb_glink_msg encoding failed: %s",
             PB_GET_ERROR(&payload_stream));
    _resp_queue_mutex.lock();
    _resp_queue.pop_back();
    _resp_queue_mutex.unlock();
    return -1;
  }
  sns_logd("rpmsgSession: Encoded pb_glink_msg successfully (%zu bytes)",
           payload_stream.bytes_written);

  std::string encoded_glink_buffer(reinterpret_cast<const char*>(encoded_payload), payload_stream.bytes_written);
  int req_rc = send_rpmsg_req(sensor_uid, encoded_glink_buffer, true);

  if (_logger) {
    const char* dt = LoggerFactory::getDataType(sensor_uid);
    if (dt != nullptr) {
      sns_logd("%s Logging request dt=%s", __func__, dt);
      int rc = _logger->logRequest(dt, encoded_buffer.data(), encoded_buffer.size());
      sns_logd("%s LogRequest rc = %d", __func__, rc);
    }
  }

  return req_rc;
}
