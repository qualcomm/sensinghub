/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
 
#pragma once
#include <stdexcept>
#include <algorithm>
#include <cinttypes>
#include <stdint.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <map>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include "ISession.h"
#include  "SessionFactory.h"
#include "qsh_register_cb.h"
#include "qshWorker.h"
#ifdef __ANDROID_API__
#include "utils/SystemClock.h"
#include "qshTrace.h"
#else
#include <time.h>
#endif
#include "sns_client.pb.h"
#include "sns_client_glink.pb.h"
#include "qshLog.h"
#include "qshWakelock.h"
#include "qshPb.h"
#include "ISessionLogger.h"

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;

using namespace com::quic::sensinghub::sessionlogger::V1_0;

using suid = com::quic::sensinghub::suid;

namespace com {
namespace quic {
namespace sensinghub {
namespace session {
namespace V1_0 {
namespace implementation {

#define MAX_GLINK_OPEN_RETRIES  3
#define MAX_GLINK_WRITE_RETRIES 30
#define MAX_GLINK_READ_RETRIES  3
#define POLL_TIMEOUT_IN_MS     -1 // -1 : Infinite timeout
#define WAKEUP_SESSION_ID       0
#define NON_WAKEUP_SESSION_ID   1
#define MAX_NUM_SESSION_ID      2
#define GLINK_CHNL_PREFIX       "/dev/glinkpkt_"
#define GLINK_WAKELOCK_NAME     "SensorsHAL_WAKEUP"

enum qsh_conn_status {
  QSH_CONNECTION_STATUS_INIT = 0,
  QSH_CONNECTION_STATUS_ACTIVE = 1,
};

class glinkSession;
struct cb_sensorUID_hash;
/* map structure -> { connection handle : { sensor suid, glink object } } */
typedef std::unordered_map<uint32_t, std::unordered_map<suid, glinkSession*, cb_sensorUID_hash>> conn_mp_type;

struct stream_req_info
{
  suid sensor_uid;
  uint32_t conn_hndl;
  bool is_disable_req;
};

struct cb_sensorUID_hash
{
  std::size_t operator()(const suid& sensor_uid) const
  {
    std::string data(reinterpret_cast<const char*>(&sensor_uid), sizeof(suid));
    return std::hash<std::string>()(data);
  }
};

class glinkSession : public ISession {
public:
  glinkSession(int no_of_chnls, int hub_id, std::string hub_name);
  ~glinkSession();

  int open();
  void close();
  int setCallBacks(suid sensor_uid, ISession::respCallBack resp_cb, ISession::errorCallBack error_cb, ISession::eventCallBack event_cb);
  int sendRequest(suid sensor_uid, std::string encoded_buffer);

private:
  static std::string _chnl_name;
  static std::atomic<int> _chnl_fd;
  static std::atomic<uint32_t> _ack_conn_hndl;
  static int _wakeup_pipe[2];
  static std::mutex _glink_write_mutex;
  static std::atomic<bool> _is_thread_created;
  static std::thread _glink_read_threadhdl;
  static qshWakelock* _wakelock_inst;
  static std::atomic<bool> _is_req_success;
  static conn_mp_type _clientid_conn_db;
  /* db maintained for suids which could not be restored after SSR */
  static conn_mp_type _restore_ssr_conn_db;
  static std::unordered_map<suid, std::vector<uint32_t>, cb_sensorUID_hash> _suid_conn_handles_db;
  static std::deque<stream_req_info> _resp_queue;
  static std::mutex _resp_queue_mutex;
  static std::mutex _glink_open_close_mutex;
  static std::mutex _glink_db_mutex;
  static std::mutex _glink_setcb_mutex;
  static std::atomic<uint32_t> _glink_err_cnt;
  static std::atomic<uint32_t> _glink_clients;
  static std::vector<std::string> _glink_chnls_list;
  static std::atomic<bool> _glink_is_ssr_in_progress;

  qsh_conn_status _conn_status;
  bool _is_ftrace_enabled;
  std::atomic<bool> _reconnecting;
  std::atomic<bool> _connection_closed;
  std::unordered_map<suid, qsh_register_cb, cb_sensorUID_hash> _callback_map_table;
  /*  worker task  */
  std::unique_ptr<qshWorker> _worker;
  /* SSR use case : Caller sends false, ISession client: True */
  static void glink_connect(bool is_new_client);
  static void glink_disconnect(bool thread_exit);
  static int glink_open();
  static int glink_read(int fd, uint8_t* buf, size_t buf_len);
  static void glink_thread_routine();
  static void glink_ssr_handler(int error);
  static void remove_suid(suid sensor_uid);
  static glinkSession* get_conn(uint32_t& conn_hndl, suid& sensor_uid);
  static int get_suid(suid &out_suid, sns_client_glink_ind& ind_msg);
  static void fetch_new_conn_hndls_after_ssr();
  static void glink_db_cleanup();
  static void move_stale_conn_hndls_to_restore_db();

  void glink_indication_handler(suid& sensor_uid, void* buf, size_t buf_len, size_t index);
  void glink_response_handler(stream_req_info& curr_resp, sns_client_glink_resp& resp_msg);
  int glink_write(int fd, uint8_t* buf, size_t buf_len);
  int trigger_event_cb(unsigned int idl_msg_id, eventCallBack current_event_cb, uint64_t ts);
  int update_suid(unsigned int msg_id, suid &out_suid);
  void destroy_map_table();
  uint32_t find_assigned_conn_handle(suid sensor_uid);
  uint32_t update_clientid_conn_db(suid sensor_uid);
  void update_suid_conn_hndl_db(uint32_t hndl, suid& sensor_uid);
  uint32_t find_free_conn_handle(suid sensor_uid);
  void send_disconnect_req(const suid& sensor_uid, uint32_t conn_hndl);
  int send_glink_req(suid sensor_uid, std::string encoded_buffer, bool is_resp_expected);
  uint32_t fetch_new_conn_hndl(suid sensor_uid);
  uint32_t check_available_conn_hndl(suid sensor_uid);
  void glink_cleanup_conn();
  uint64_t get_sample_timestamp_ns();
  std::shared_ptr<ISessionLogger> _logger;
};

}
}
}
}
}
}

