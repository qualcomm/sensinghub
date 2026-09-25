/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include "ISession.h"
#include "SessionFactory.h"
#ifdef QMI_OSS_ENABLE
#include "qmi_cci.h"
#else
#include "qmi_client.h"
#endif
#include "sns_client_api_v01.h"
#include <mutex>
#include <map>
#include <deque>
#include <chrono>
#include "qsh_register_cb.h"
#include <unordered_map>
#include "ISessionLogger.h"
#include "qshLog.h"
#include "qshWorker.h"
#include "qshPb.h"

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

class qmiSession : public ISession {
public:
  qmiSession();
  ~qmiSession();

  int open();
  void close();
  int setCallBacks(suid sensor_uid, ISession::respCallBack resp_cb, ISession::errorCallBack error_cb, ISession::eventCallBack event_cb);
  int sendRequest(suid sensor_uid, std::string encoded_buffer);

private:
  void qmi_connect();
  void qmi_wait_for_service();
  void qmi_disconnect();
  void handle_error_cb(qmiSession *conn, ISession::error error_code);
  void handle_resp_cb(sns_client_resp_msg_v01 resp);
  void handle_version_cb(sns_client_version_resp_msg_v01 resp);
  void handle_event_cb(unsigned int msg_id, void *ind_buf, unsigned int ind_buf_len);
  int decode_qmi_buffer(unsigned int msg_id, void *ind_buf, unsigned int ind_buf_len);
  int create_ind_memory();
  void destroy_ind_memory();
  bool is_disable_stream_request(std::string encoded_buffer);
  void get_version();

  struct cb_sensorUID_hash
  {
      std::size_t operator()(const suid& sensor_uid) const
      {
          std::string data(reinterpret_cast<const char*>(&sensor_uid), sizeof(suid));
          return std::hash<std::string>()(data);
      }
  };

  struct stream_req_info
  {
   suid sensor_uid;
    bool is_disable_req;
  };

  sns_client_jumbo_report_ind_msg_v01       *_ind = nullptr;
  qmi_cci_os_signal_type                    _os_params;
  sns_client_resp_msg_v01                   _resp = {};
  sns_client_version_resp_msg_v01           _version_resp = {};
  qmi_client_type                           _qmi_handle;

  bool                                      _service_ready;

  std::atomic<bool>                         _reconnecting = false;
  std::atomic<bool>                         _connection_closed = false;
  std::mutex                                _mutex;
  std::condition_variable                   _cv;
  std::mutex                                _cb_map_table_mutex;
  std::unordered_map<suid, qsh_register_cb, cb_sensorUID_hash>     _callback_map_table;

  std::deque<stream_req_info>               _resp_queue;
  std::mutex                                _resp_queue_mutex;
  const uint32_t                            _max_qmi_error_cnt = 10;
  const int                                 _initial_discovery_tries_count = 4;
  const int                                 _post_discovery_tries_count = 2;
  const std::chrono::seconds                _intial_discovery_timeout = (std::chrono::seconds)2;
  const std::chrono::milliseconds           _post_discovery_timeout = (std::chrono::milliseconds)500;
  const std::chrono::milliseconds           _max_error_callback_timeout = (std::chrono::milliseconds)5000;

  std::unique_ptr<qshWorker>                _worker;
  /*All static methods and variables - below*/
  static void qmi_indication_cb(qmi_client_type user_handle,
                                unsigned int msg_id, void* ind_buf,
                                unsigned int ind_buf_len,
                                void* ind_cb_data);

  static void qmi_notify_cb(qmi_client_type user_handle,
                            qmi_idl_service_object_type service_obj,
                            qmi_client_notify_event_type service_event,
                            void *notify_cb_data);

#ifdef QMI_OSS_ENABLE
  static void qmi_error_cb(qmi_client_type user_handle,
                           qmi_cci_error_type error,
                           void* err_cb_data);
  static void qmi_response_cb(qmi_client_type user_handle,
                                unsigned int msg_id,
                                void* resp_cb,
                                unsigned int resp_cb_len,
                                void* resp_cb_data,
                                qmi_cci_error_type qmi_error);
#else
    static void qmi_error_cb(qmi_client_type user_handle,
                             qmi_client_error_type error,
                             void* err_cb_data);
    static void qmi_response_cb(qmi_client_type user_handle,
                                  unsigned int msg_id,
                                  void* resp_cb,
                                  unsigned int resp_cb_len,
                                  void* resp_cb_data,
                                  qmi_client_error_type qmi_error);
#endif

  static bool                               _service_accessed;
  static uint32_t                           _qmi_err_cnt;
  static std::string                        _client_version;
  static std::string                        _sensor_version;
  int                                       client_connect_id;

  std::shared_ptr<ISessionLogger> _logger;
};

}
}
}
}
}
}
