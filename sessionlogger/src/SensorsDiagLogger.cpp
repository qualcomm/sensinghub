/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <sstream>
#include <dlfcn.h>
#include "SensorsDiagLogger.h"

namespace com {
namespace quic {
namespace sensinghub {
namespace sessionlogger {
namespace V1_0 {

SensorsDiagLogger::DiagShim SensorsDiagLogger::_diagShim;

void SensorsDiagLogger::diagShimInit() {
  if (_diagShim.handle) {
    return;
  }
  // Attempt to load the diag shim
  _diagShim.handle = dlopen("libsnsdiagshim-c.so", RTLD_NOW);
  if (!_diagShim.handle) {
    return;
  }
  _diagShim.req   = reinterpret_cast<diagReqT>(dlsym(_diagShim.handle, "sns_diag_log_request_msg_c"));
  _diagShim.evt   = reinterpret_cast<diagEvtT>(dlsym(_diagShim.handle, "sns_diag_log_event_msg_c"));
  _diagShim.rsp   = reinterpret_cast<diagRspT>(dlsym(_diagShim.handle, "sns_diag_log_response_msg_c"));
  _diagShim.setId = reinterpret_cast<diagSetIdT>(dlsym(_diagShim.handle, "sns_diag_update_client_connect_id_c"));
  _diagShim.clear = reinterpret_cast<diagClearT>(dlsym(_diagShim.handle, "sns_diag_clear_session_c"));
}

bool SensorsDiagLogger::isDiagShimAvailable() {
  return (_diagShim.req && _diagShim.evt && _diagShim.rsp && _diagShim.setId && _diagShim.clear);
}

SensorsDiagLogger::SensorsDiagLogger(std::string moduleName, const void* sessionKey)
    : _moduleName(std::move(moduleName)), _sessionKey(sessionKey) {
  if (!isAvailable()) {
    throw std::runtime_error("SensorsDiagLogger: DIAG is not available");
  }
}

bool SensorsDiagLogger::isAvailable() {
  static std::mutex sInitMutex;
  std::lock_guard<std::mutex> lock(sInitMutex);

  // Attempt to load libdiag.so once; cache the result.
  static bool sLibdiagChecked = false;
  static bool sLibdiagAvailable = false;
  if (!sLibdiagChecked) {
    sLibdiagChecked = true;
    void* libdiagHandle = dlopen("libdiag.so", RTLD_NOW | RTLD_NOLOAD);
    if (!libdiagHandle) {
      // Not already loaded — try a fresh load.
      libdiagHandle = dlopen("libdiag.so", RTLD_NOW);
    }
    sLibdiagAvailable = (libdiagHandle != nullptr);
  }

  if (sLibdiagAvailable)
    diagShimInit();

  return sLibdiagAvailable && isDiagShimAvailable();
}

int SensorsDiagLogger::logRequest(const char* dataType, const void* requestBuf, size_t requestLen) {
  if (_diagShim.req) {
    int rc = _diagShim.req(_sessionKey,
                        reinterpret_cast<const char*>(requestBuf),
                        requestLen,
                        dataType,
                        _moduleName.c_str());
    if (rc == 0) {
      return 0;
    }
  }

  return 1;
}

int SensorsDiagLogger::logResponse(const char* dataType, const uint32_t responseValue) {
  if (_diagShim.rsp) {
    int rc = _diagShim.rsp(_sessionKey, responseValue, dataType, _moduleName.c_str());
    if (rc == 0) {
      return 0;
    }
  }

  return 1;
}

int SensorsDiagLogger::logEvent(const char* dataType, const void* eventBuf, size_t eventLen) {
  if (_diagShim.evt) {
    int rc = _diagShim.evt(_sessionKey,
                        reinterpret_cast<const char*>(eventBuf),
                        eventLen,
                        dataType,
                        _moduleName.c_str());
    if (rc == 0) {
      return 0;
    }
  }

  return 1;
}

void SensorsDiagLogger::updateClientId(uint64_t clientConnectId) {
  if (_diagShim.setId) {
    _diagShim.setId(_sessionKey, clientConnectId);
  }
}

SensorsDiagLogger::~SensorsDiagLogger() {
  // cleanup of diag-shim per-session state (if shim is present)
  if (_diagShim.clear) {
    _diagShim.clear(_sessionKey);
  }
}

}  // namespace V1_0
}  // namespace sessionlogger
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
