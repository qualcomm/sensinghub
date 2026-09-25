/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "qmiSession.h"
#include "sns_client_api_v01.h"
#include "qsh_qmi_error.h"
#include "suidLookUp.h"
#include "qshSSR.h"
#include "qshLog.h"
#include "sns_client.pb.h"
#ifdef __ANDROID_API__
#include "utils/SystemClock.h"
#else
#include <time.h>
#endif
#include <sys/types.h>
#include <sched.h>
#include <inttypes.h>

#include "LoggerFactory.h"
#include "sessionLoggerUtil.h"


using namespace std;
using namespace ::com::quic::sensinghub::session::V1_0::implementation;
using namespace ::com::quic::sensinghub::sessionlogger::V1_0;

#define MAX_SVC_INFO_ARRAY_SIZE 5

bool qmiSession::_service_accessed = false;
uint32_t qmiSession::_qmi_err_cnt = 0;
string qmiSession::_sensor_version = "";
string qmiSession::_client_version = "";

qmiSession::qmiSession() {
  _qmi_handle = nullptr;
  sns_logi("%s" , __func__);

  _logger = LoggerFactory::getLogger("APSS", this);
  sns_logi("%s Logger is set for %llu", __func__, this);
}

int qmiSession::create_ind_memory() {
  _ind = (sns_client_jumbo_report_ind_msg_v01 *)calloc(1, sizeof(sns_client_jumbo_report_ind_msg_v01));
  if(nullptr == _ind){
    sns_loge("Error while creating memory for ind");
    return -1;
  }
  return 0;
}

int qmiSession::open(){
  try{
    _worker = make_unique<qshWorker>();
    int ret = create_ind_memory();
    if(-1 == ret)
      return -1;
    qmi_connect();
    if(_sensor_version.empty() && _client_version.empty())
      get_version();
  }catch(const exception& e){
    sns_loge("%s failed: %s", __func__, e.what());
    _worker.reset();
    destroy_ind_memory();
    return -1;
  }
  return 0;
}

void qmiSession::close(){
  _connection_closed = true;
  qmi_disconnect();
  _worker.reset();
  destroy_ind_memory();
}

void qmiSession::destroy_ind_memory(){
  if(nullptr != _ind) {
    free(_ind);
    _ind = nullptr;
  }
}

qmiSession::~qmiSession(){
  /*delete worker object explicitly to clean-up all pending tasks
    after that, there will be no access to any resource */
  _logger.reset();
  _callback_map_table.clear();
  _resp_queue.clear();
  sns_logi("%s" , __func__);
}

void qmiSession::qmi_connect()
{
  qmi_idl_service_object_type svc_obj =
      SNS_CLIENT_SVC_get_service_object_v01();
#ifdef QMI_OSS_ENABLE
  qmi_cci_error_type qmi_err;
#else
  qmi_client_error_type qmi_err;
#endif
  qmi_service_info svc_info_array[MAX_SVC_INFO_ARRAY_SIZE];
  uint32_t num_services, num_entries = MAX_SVC_INFO_ARRAY_SIZE;

  /*svc_info_array[5] - Initialized to 0 to avoid static analysis errors*/
  for(uint32_t i = 0 ; i < num_entries ; i++)
    memset(&svc_info_array[i], 0, sizeof(svc_info_array[i]));

  sns_logv("waiting for sensors qmi service");
  qmi_wait_for_service();
  sns_logv("connecting to qmi service");
#ifdef QMI_OSS_ENABLE
  qmi_err = qmi_cci_get_service_list(svc_obj, svc_info_array,
      &num_entries, &num_services);
#else
  qmi_err = qmi_client_get_service_list(svc_obj, svc_info_array,
      &num_entries, &num_services);
#endif
  if (QMI_NO_ERR != qmi_err) {
    throw qsh_qmi_error(qmi_err, "qmi_client_get_service_list() failed");
  }

  if (num_entries == 0) {
    throw runtime_error("sensors service has no available instances");
  }

  if (_connection_closed) {
    sns_logi("connection got closed do not open qmi_channel");
    return ;
  }

  /* As only one qmi service is expected for sensors, use the 1st instance */
  qmi_service_info svc_info = svc_info_array[0];

  std::unique_lock<std::mutex> lk(_mutex);
#ifdef QMI_OSS_ENABLE
  qmi_err = qmi_cci_init(&svc_info, svc_obj, qmi_indication_cb,
      (void*)this, &_os_params, &_qmi_handle);
#else
  qmi_err = qmi_client_init(&svc_info, svc_obj, qmi_indication_cb,
      (void*)this, &_os_params, &_qmi_handle);
#endif

  if (qmi_err != QMI_IDL_LIB_NO_ERR) {
    lk.unlock();
    throw qsh_qmi_error(qmi_err, "qmi_client_init() failed");
  }
#ifdef QMI_OSS_ENABLE
  qmi_err = qmi_cci_register_error_cb(_qmi_handle, qmi_error_cb, this);
#else
  qmi_err = qmi_client_register_error_cb(_qmi_handle, qmi_error_cb, this);
#endif

  if (QMI_NO_ERR != qmi_err) {
    lk.unlock();
//    qsh_qmi_debug_timer debug_timer;
#ifdef QMI_OSS_ENABLE
    qmi_cci_release(_qmi_handle);
#else
    qmi_client_release(_qmi_handle);
#endif
    throw qsh_qmi_error(qmi_err, "qmi_client_register_error_cb() failed");
  }
  lk.unlock();
  sns_logv("connected to qsh for %p", (void *)this);
}

void qmiSession::qmi_wait_for_service()
{
  qmi_client_type notifier_handle;
#ifdef QMI_OSS_ENABLE
  qmi_cci_error_type qmi_err;
#else
  qmi_client_error_type qmi_err;
#endif
  qmi_cci_os_signal_type os_params;
  int num_tries = _initial_discovery_tries_count;
#ifdef QMI_OSS_ENABLE
  qmi_err = qmi_cci_notifier_init(SNS_CLIENT_SVC_get_service_object_v01(),
      &os_params, &notifier_handle);
#else
  qmi_err = qmi_client_notifier_init(SNS_CLIENT_SVC_get_service_object_v01(),
      &os_params, &notifier_handle);
#endif
  if (QMI_NO_ERR != qmi_err) {
    throw qsh_qmi_error(qmi_err, "qmi_client_notifier_init() failed");
  }

  /* register a callback and wait until service becomes available */
  _service_ready = false;
#ifdef QMI_OSS_ENABLE
  qmi_err = qmi_cci_register_notify_cb(notifier_handle, qmi_notify_cb,
      this);
#else
  qmi_err = qmi_client_register_notify_cb(notifier_handle, qmi_notify_cb,
      this);
#endif
  if (qmi_err != QMI_NO_ERR) {
//    qsh_qmi_debug_timer debug_timer;
#ifdef QMI_OSS_ENABLE
    qmi_cci_release(notifier_handle);
#else
    qmi_client_release(notifier_handle);
#endif
    throw qsh_qmi_error(qmi_err, "qmi_client_register_notify_cb() failed: %d");
  }

  if (_service_accessed) {
    num_tries = _post_discovery_tries_count;
  }

  std::unique_lock<std::mutex> lk(_mutex);
  bool timeout = false;
  while (num_tries > 0 && (_service_ready != true)) {
    num_tries--;
    if (_service_accessed)
      timeout = !_cv.wait_for(lk, std::chrono::milliseconds(_post_discovery_timeout),
          [this]{ return _service_ready; });
    else
      timeout = !_cv.wait_for(lk, std::chrono::seconds(_intial_discovery_timeout),
          [this]{ return _service_ready; });

    if (timeout) {
      if (num_tries == 0) {
        lk.unlock();
//        qsh_qmi_debug_timer debug_timer;
#ifdef QMI_OSS_ENABLE
        qmi_cci_release(notifier_handle);
#else
        qmi_client_release(notifier_handle);
#endif

        throw runtime_error("FATAL: could not find sensors QMI service");
      }
      sns_loge("timeout while waiting for sensors QMI service: "
          "will try %d more time(s)", num_tries);
    } else {
      _service_accessed = true;
    }
  }
  lk.unlock();
//  qsh_qmi_debug_timer debug_timer;
#ifdef QMI_OSS_ENABLE
  qmi_cci_release(notifier_handle);
#else
  qmi_client_release(notifier_handle);
#endif

}

void qmiSession::qmi_disconnect()
{
  std::unique_lock<std::mutex> lk(_mutex);
  if (_qmi_handle != nullptr) {
//    qsh_qmi_debug_timer debug_timer;
#ifdef QMI_OSS_ENABLE
    qmi_cci_release(_qmi_handle);
#else
    qmi_client_release(_qmi_handle);
#endif
    _qmi_handle = nullptr;
    /*in ssr call back and sensor disabled , so notify qmi_connect to comeout*/
    if (_connection_closed)
      _cv.notify_one();
  }
  /*explicit unlock not required , just added not to miss the logic*/
  lk.unlock();
  sns_logv("disconnected from qsh for %p", (void *)this);
}

void qmiSession::qmi_indication_cb(qmi_client_type user_handle,
                                unsigned int msg_id, void* ind_buf,
                                unsigned int ind_buf_len,
                                void* ind_cb_data
                                )
{
  qmiSession *conn = static_cast<qmiSession *>(ind_cb_data);
  conn->handle_event_cb(msg_id, ind_buf, ind_buf_len);
}

#ifdef QMI_OSS_ENABLE
void qmiSession::qmi_error_cb(qmi_client_type user_handle,
                                      qmi_cci_error_type error,
                                      void* err_cb_data)
{
  qmiSession *conn = static_cast<qmiSession *>(err_cb_data);
  sns_loge("error=%d", error);

  if (conn->_connection_closed) {
    sns_logi("qmi error is coming while connection is being closed");
    return;
  }
#else
void qmiSession::qmi_error_cb(qmi_client_type user_handle,
                                      qmi_client_error_type error,
                                      void* err_cb_data)
{
  qmiSession *conn = static_cast<qmiSession *>(err_cb_data);
  sns_loge("error=%d", error);

  if (conn->_connection_closed) {
    sns_logi("qmi error is coming while connection is being closed");
    return;
  }
#endif
  if (error != QMI_NO_ERR) {
    conn->_reconnecting = true;
    /* handle error asynchronously using worker thread */
    conn->_service_accessed = false;
    conn->_worker->addTask([conn]
    {
      /* need to check it again since this code will
         be executed by worker thread */
      if (conn->_connection_closed) {
        sns_logi("qmi error is coming while connection is being closed");
        return;
      }
      sns_logi("qmi error, qmi_disconnect %p", (void *)conn);
      /* re-establish qmi connection */
      conn->qmi_disconnect();
      try {
        sns_logi("qmi error, trying to reconnect");
        conn->qmi_connect();
      } catch (const exception& e) {
        sns_loge("could not reconnect: %s", e.what());
        conn->handle_error_cb(conn, ISession::SERVICE_DOWN);
      }
      conn->_reconnecting = false;
      sns_logi("qmi connection re-established");
      /* notify user about connection reset */
      if (conn->_connection_closed == true) {
        sns_logi("sensor deactivated during ssr");
        return;
      }
      conn->handle_error_cb(conn, ISession::RESET);
    });
  }
}

void qmiSession::handle_error_cb(qmiSession *conn, ISession::error error_code){
  int total_valid_error_cb_count = 0;
  std::mutex cv_mutex_error_cb;
  std::condition_variable cv_error_cb;
  vector<unique_ptr<qshWorker>> subworkers;
  int subworker_count = 0;

  for(auto itr = conn->_callback_map_table.begin(); itr!= conn->_callback_map_table.end(); ++itr) {
    if(nullptr == itr->second.get_error_cb()) {
      sns_logd("error_cb is not registered. do not callback anything for this suid");
      continue;
    }
    total_valid_error_cb_count++;
  }

  for(auto itr = conn->_callback_map_table.begin(); itr!= conn->_callback_map_table.end(); ++itr) {
    errorCallBack err_cb = (itr->second).get_error_cb();
    if(nullptr == err_cb) {
      sns_logd("error_cb is not registered. do not callback anything for this suid");
      continue;
    }
    subworkers.push_back(make_unique<qshWorker>());
    (subworkers.back())->addTask([&,err_cb]() {
      err_cb(error_code);
      std::unique_lock<std::mutex> lock(cv_mutex_error_cb);
      subworker_count++;
      cv_error_cb.notify_one();
    });
  }
  std::unique_lock<std::mutex> lock(cv_mutex_error_cb);
  bool timeout = false;
  sns_logi("wait for notification - for 5 secs");
  timeout = !cv_error_cb.wait_for(lock, std::chrono::milliseconds(conn->_max_error_callback_timeout),
                                  [&subworker_count,&total_valid_error_cb_count](){
                                      return total_valid_error_cb_count == subworker_count;
                                  });
  if(timeout){
    throw std::runtime_error("Timeout");
  }

  for(size_t i=0;i<subworkers.size();i++) {
    (subworkers[i]).reset();
  }
}

void qmiSession::qmi_notify_cb(qmi_client_type user_handle,
                                       qmi_idl_service_object_type service_obj,
                                       qmi_client_notify_event_type service_event,
                                       void *notify_cb_data)
{
  qmiSession *conn = static_cast<qmiSession *>(notify_cb_data);
  if (conn->_connection_closed) {
    sns_logi("qmi notify request is coming while connection is being closed");
    return;
  }
  unique_lock<mutex> lk(conn->_mutex);
  conn->_service_ready = true;
  conn->_cv.notify_one();
}

int qmiSession::setCallBacks(suid sensor_uid, ISession::respCallBack resp_cb, ISession::errorCallBack error_cb, ISession::eventCallBack event_cb) {
  sns_logd("%s start this=%px" , __func__, this);
  std::lock_guard<mutex> lk(_cb_map_table_mutex);
  auto it = _callback_map_table.find(sensor_uid);
  if (it == _callback_map_table.end()) {
    sns_logi("suid(low=%" PRIu64 ",high=%" PRIu64 ") is new. register callbacks", sensor_uid.low, sensor_uid.high);
    if (resp_cb == nullptr && error_cb == nullptr && event_cb == nullptr) {
      sns_loge("all callbacks are null for new suid. no need to register it");
      return -1;
    }
    qsh_register_cb cb(resp_cb,error_cb,event_cb);
    _callback_map_table.insert(std::pair<suid,qsh_register_cb>(sensor_uid, cb));
  } else {
    sns_logi("suid(low=%" PRIu64 ",high=%" PRIu64 ") is alread registered", sensor_uid.low, sensor_uid.high);
    if (resp_cb == nullptr && error_cb == nullptr && event_cb == nullptr) {
      sns_logi("all callbacks are null, unregister it");
      _callback_map_table.erase(it);
    } else {
      sns_logi("update callbacks with new one");
      qsh_register_cb cb(resp_cb,error_cb,event_cb);
      it->second = cb;
    }
  }
  sns_logd("%s completed, this=%px ", __func__, this);
  return 0;
}

int qmiSession::sendRequest(suid sensor_uid, std::string encoded_buffer) {
  sns_logd("%s start this=%px " , __func__, this);
  if (_reconnecting) {
    sns_loge("qmi connection failed, cannot send data");
    return -1;
  }
  sns_client_req_msg_v01 req_msg;

  if (encoded_buffer.length() > SNS_CLIENT_REQ_LEN_MAX_V01) {
    sns_loge("error: payload size is too large");
    return -1;
  }

  memcpy(req_msg.payload, encoded_buffer.c_str(),
      encoded_buffer.length());
  req_msg.use_jumbo_report_valid = true;
  req_msg.use_jumbo_report = true;
  req_msg.payload_len = encoded_buffer.length();

  stream_req_info req_info;
  req_info.sensor_uid.low = sensor_uid.low;
  req_info.sensor_uid.high = sensor_uid.high;
  req_info.is_disable_req = is_disable_stream_request(encoded_buffer);
  _resp_queue_mutex.lock();
  _resp_queue.push_back(req_info);
  _resp_queue_mutex.unlock();
#ifdef QMI_OSS_ENABLE
  qmi_cci_error_type qmi_err;
#else
  qmi_client_error_type qmi_err;
#endif
  qmi_txn_handle qmi_txn_handle;
  {
#ifdef QMI_OSS_ENABLE
    qmi_err = qmi_cci_send_msg_async(_qmi_handle, SNS_CLIENT_REQ_V01,
          (void*)&req_msg, sizeof(req_msg),
          &_resp,
          sizeof(sns_client_resp_msg_v01),
          qmi_response_cb,
          (void*)this,
          &qmi_txn_handle);
#else
    qmi_err = qmi_client_send_msg_async(_qmi_handle, SNS_CLIENT_REQ_V01,
          (void*)&req_msg, sizeof(req_msg),
          &_resp,
          sizeof(sns_client_resp_msg_v01),
          qmi_response_cb,
          (void*)this,
          &qmi_txn_handle);
#endif
  }

  if (_logger) {
    const char* dt = LoggerFactory::getDataType(sensor_uid);
    sns_logd("%s Logging request dt=%s", __func__, dt);
    int rc = _logger->logRequest(dt, encoded_buffer.data(), encoded_buffer.size());
    sns_logd("%s LogRequest rc = %d", __func__, rc);
  }

  if (qmi_err != QMI_NO_ERR){
    _resp_queue_mutex.lock();
    _resp_queue.pop_back();
    _resp_queue_mutex.unlock();
    _qmi_err_cnt++;
    if ((_qmi_err_cnt > _max_qmi_error_cnt) &&
        !triggerSSR()) {
      sns_logd("triggred ssr _qmi_err_cnt %d", _qmi_err_cnt);
      _qmi_err_cnt = 0;
      /*if QMI_NO_ERR and ssr triggered it is surely ssr_simulate*/
    } else {
      sns_logd("qmi_client_send_msg_async() failed");
    }
    return -1;
  } else {
    /*occassional failure of QMI , recovered with in _max_qmi_error_cnt*/
    if (_qmi_err_cnt)
      _qmi_err_cnt = 0;
  }

  sns_logd("%s completed, this=%px" , __func__, this);
  return 0;
}

void qmiSession::handle_version_cb(sns_client_version_resp_msg_v01 resp){
    sns_client_version_response_msg pb_version_resp = sns_client_version_response_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer(
      (const pb_byte_t *)resp.payload, 
      static_cast<pb_size_t>(resp.payload_len));
    if(!pb_decode(&stream, sns_client_version_response_msg_fields, &pb_version_resp)){
      sns_logi("handle_version_cb: ParseFromArray failed len=%u", resp.payload_len);
      return;
    }
    std::string client_version_str{};
    std::string sensor_version_str{};

    if(pb_version_resp.has_major)
      client_version_str += to_string(pb_version_resp.major) + ".";
    else
      client_version_str += to_string(0) + ".";

    if(pb_version_resp.has_minor)
      client_version_str += to_string(pb_version_resp.minor) + ".";
    else
      client_version_str += to_string(0) + ".";

    if(pb_version_resp.has_patch)
      client_version_str += to_string(pb_version_resp.patch);
    else
      client_version_str += to_string(0);

    if(pb_version_resp.has_sensor_major)
      sensor_version_str += to_string(pb_version_resp.sensor_major) + ".";
    else
      sensor_version_str += to_string(0) + ".";

    if(pb_version_resp.has_sensor_minor)
      sensor_version_str += to_string(pb_version_resp.sensor_minor) + ".";
    else
      sensor_version_str += to_string(0) + ".";

    if(pb_version_resp.has_sensor_patch)
      sensor_version_str += to_string(pb_version_resp.sensor_patch);
    else
      sensor_version_str += to_string(0);

    _client_version = client_version_str;
    _sensor_version = sensor_version_str;

    sns_logi("SensingHub version parsed:: _client_version = %s, _sensor_version = %s",
          _client_version.c_str(), _sensor_version.c_str());
}

void qmiSession::get_version(){
  sns_client_version_req_msg_v01 req_msg;
#ifdef QMI_OSS_ENABLE
  qmi_cci_error_type qmi_err;
#else
  qmi_client_error_type qmi_err;
#endif
  qmi_txn_handle qmi_txn_handle;
  {
#ifdef QMI_OSS_ENABLE
    qmi_err = qmi_cci_send_msg_async(_qmi_handle, SNS_CLIENT_VERSION_REQ_V01,
          (void*)&req_msg, sizeof(req_msg),
          &_resp,
          sizeof(sns_client_resp_msg_v01),
          qmi_response_cb,
          (void*)this,
          &qmi_txn_handle);
#else
    qmi_err = qmi_client_send_msg_async(_qmi_handle, SNS_CLIENT_VERSION_REQ_V01,
          (void*)&req_msg, sizeof(req_msg),
          &_version_resp,
          sizeof(sns_client_version_resp_msg_v01),
          qmi_response_cb,
          (void*)this,
          &qmi_txn_handle);
#endif
  }
  if (qmi_err != QMI_NO_ERR){
    sns_logi("Error in sending version request, qmi_err = %d", qmi_err);
  }
}

bool qmiSession::is_disable_stream_request(std::string encoded_buffer) {
  sns_client_request_msg pb_req_msg = sns_client_request_msg_init_default;
  pb_byte_t* pb_encoded_buffer = reinterpret_cast<pb_byte_t*>(encoded_buffer.data());
  pb_istream_t stream = pb_istream_from_buffer(pb_encoded_buffer, encoded_buffer.length());

  if(!pb_decode(&stream, sns_client_request_msg_fields, &pb_req_msg)) {
    sns_loge("qmiSession: sns_client_request_msg decode failed %s", PB_GET_ERROR(&stream));
    return false;
  }
  uint32_t msg_id = pb_req_msg.msg_id;

  if(SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ == msg_id) {
    return true;
  }else {
    return false;
  }
}
#ifdef QMI_OSS_ENABLE
void qmiSession::qmi_response_cb(qmi_client_type user_handle,
                              unsigned int msg_id,
                              void* resp_cb,
                              unsigned int resp_cb_len,
                              void* resp_cb_data,
                              qmi_cci_error_type qmi_err)
{
  qmiSession *conn = static_cast<qmiSession *>(resp_cb_data);
  if(msg_id == SNS_CLIENT_VERSION_RESP_V01){
    sns_client_version_resp_msg_v01 resp = *((sns_client_version_resp_msg_v01 *)resp_cb);
    conn->handle_version_cb(resp);
  }else{
    sns_client_resp_msg_v01 resp = *((sns_client_resp_msg_v01 *)resp_cb);
    conn->handle_resp_cb(resp);
  }
}
#else
void qmiSession::qmi_response_cb(qmi_client_type user_handle,
                              unsigned int msg_id,
                              void* resp_cb,
                              unsigned int resp_cb_len,
                              void* resp_cb_data,
                              qmi_client_error_type qmi_err)
{
  qmiSession *conn = static_cast<qmiSession *>(resp_cb_data);
  if(msg_id == SNS_CLIENT_VERSION_RESP_V01){
    sns_client_version_resp_msg_v01 resp = *((sns_client_version_resp_msg_v01 *)resp_cb);
    conn->handle_version_cb(resp);
  }else{
    sns_client_resp_msg_v01 resp = *((sns_client_resp_msg_v01 *)resp_cb);
    conn->handle_resp_cb(resp);
  }
}
#endif

void qmiSession::handle_resp_cb(sns_client_resp_msg_v01 resp) {
  sns_logd("%s start this=%px " , __func__, this);
  if (_connection_closed) {
    sns_logi("qmi response is coming while connection is being closed");
    return;
  }
  if(resp.client_id_valid)
    client_connect_id = resp.client_id;
  else
    client_connect_id = -1;
  _resp_queue_mutex.lock();
  if(false == _resp_queue.empty()) {
    auto front = _resp_queue.front();
    suid sensor_uid = front.sensor_uid;
    bool is_disable_resp = front.is_disable_req;
    _resp_queue.pop_front();
    _resp_queue_mutex.unlock();
    _cb_map_table_mutex.lock();
    auto it = _callback_map_table.find(sensor_uid);
    if(it == _callback_map_table.end()){
      sns_logd(" %s suid NOT found in _callback_map_table table suid_low=%" PRIu64 " suid_high=%" PRIu64 " client_connect_id=%d , this=%px" ,
          __func__, sensor_uid.low, sensor_uid.high, client_connect_id, this);
      _cb_map_table_mutex.unlock();
      return;
    }
    respCallBack current_resp_cb = it->second.get_resp_cb();
    if(true == is_disable_resp) {
        _callback_map_table.erase(it);
    }
    if(resp.result_valid && resp.client_id_valid) {
      if (current_resp_cb) {
        sns_logd(" %s suid found in _callback_map_table table and trigerring resp_cb suid_low=%" PRIu64 " suid_high=%" PRIu64 " client_connect_id=%d , this=%px" ,
          __func__, sensor_uid.low, sensor_uid.high, client_connect_id, this);
        current_resp_cb(resp.result, resp.client_id);
      }

      if (_logger) {
        const char* dt = LoggerFactory::getDataType(sensor_uid);
        sns_logd("%s Setting client id to %d", __func__, client_connect_id);
        _logger->updateClientId((uint64_t)client_connect_id);
        sns_logd("%s Logging response dt=%s value=%lu", __func__, dt, resp.result);
        int rc = _logger->logResponse(dt, resp.result);
        sns_logd("%s LogResponse rc = %d", __func__, rc);
      }
    }
    _cb_map_table_mutex.unlock();
  } else {
    _resp_queue_mutex.unlock();
  }

  sns_logd("%s Ended this=%px" , __func__, this);
}

int qmiSession::decode_qmi_buffer(unsigned int msg_id, void *ind_buf, unsigned int ind_buf_len) {
  int32_t qmi_err;
  if(SNS_CLIENT_REPORT_IND_V01 == msg_id){
    qmi_err = qmi_idl_message_decode(SNS_CLIENT_SVC_get_service_object_v01(),
        QMI_IDL_INDICATION, msg_id, ind_buf,
        ind_buf_len, (void*)_ind,
        sizeof(sns_client_report_ind_msg_v01));
    if (QMI_IDL_LIB_NO_ERR != qmi_err) {
      sns_loge("qmi_idl_message_decode() failed. qmi_err=%d SNS_CLIENT_REPORT_IND_V01", qmi_err);
      return -1;
    }
  }
  else if(SNS_CLIENT_JUMBO_REPORT_IND_V01 == msg_id){
    qmi_err = qmi_idl_message_decode(SNS_CLIENT_SVC_get_service_object_v01(),
        QMI_IDL_INDICATION, msg_id, ind_buf,
        ind_buf_len, (void*)_ind,
        sizeof(sns_client_jumbo_report_ind_msg_v01));
    if (QMI_IDL_LIB_NO_ERR != qmi_err) {
      sns_loge("qmi_idl_message_decode() failed. qmi_err=%d SNS_CLIENT_JUMBO_REPORT_IND_V01", qmi_err);
      return -1;
    }
  } else {
    sns_loge("not a valid qmi buffer ");
    return -1;
  }
  return 0;
}

void qmiSession::handle_event_cb(unsigned int msg_id, void *ind_buf, unsigned int ind_buf_len) {
  if (_connection_closed) {
    sns_logi("qmi indication is coming while connection is being closed");
    return;
  }
#ifdef __ANDROID_API__
  uint64_t sample_received_ts = android::elapsedRealtimeNano();
#else
  struct timespec current_sample_received_ts;
  clock_gettime(CLOCK_REALTIME, &current_sample_received_ts);
  uint64_t sample_received_ts = (uint64_t)(current_sample_received_ts.tv_sec*1000000000ull+current_sample_received_ts.tv_nsec);
#endif
  int ret = decode_qmi_buffer(msg_id, ind_buf, ind_buf_len);
  if(ret < 0) {
    sns_loge("error while decode_qmi_buffer ");
    return;
  }

  sns_client_event_msg pb_event_msg = sns_client_event_msg_init_default;
  pb_istream_t stream = pb_istream_from_buffer(_ind->payload, _ind->payload_len);

  pb_event_msg.events.funcs.decode = NULL;
  pb_event_msg.events.arg = NULL;
  if (!pb_decode(&stream, sns_client_event_msg_fields, &pb_event_msg)) {
      sns_loge("qmiSession: sns_client_event_msg decoding failed %s", PB_GET_ERROR(&stream));
      return;
  }
  suid sensor_uid;
  sensor_uid.low = pb_event_msg.suid.suid_low;
  sensor_uid.high = pb_event_msg.suid.suid_high;

  // learn datatype from SUID + ATTR events in the indication payload.
  #ifdef __ANDROID_API__
    updateDataTypeFromEventPayload(sensor_uid, _ind->payload, _ind->payload_len);
  #endif

  if (_logger) {
    const char* dt = LoggerFactory::getDataType(sensor_uid);
    _logger->logEvent(dt, _ind->payload, _ind->payload_len);
  }

  eventCallBack current_event_cb = NULL;
  {
    std::lock_guard<mutex> lk(_cb_map_table_mutex);
    auto it = _callback_map_table.find(sensor_uid);
    if(it != _callback_map_table.end()) {
        current_event_cb = it->second.get_event_cb();
    }

    if(nullptr != current_event_cb) {
      sns_logd(" %s suid found in _callback_map_table table and trigerring event_cb suid_low=%" PRIu64 " suid_high=%" PRIu64 " client_connect_id=%d , this=%px" ,
          __func__, sensor_uid.low, sensor_uid.high, client_connect_id, this);
      current_event_cb( _ind->payload, _ind->payload_len, sample_received_ts);
      memset(_ind, 0 , sizeof(sns_client_jumbo_report_ind_msg_v01));
    }
    else{
        sns_loge("event cb is not registered ");
    }
  }
  return;
}
