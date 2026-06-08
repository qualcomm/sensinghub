/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <chrono>
#include <vector>
#include "qshLog.h"
#include "LoggerFactory.h"
#include "NullLogger.h"
#include "SensorsDiagLogger.h"

using suid = com::quic::sensinghub::suid;

namespace com {
namespace quic {
namespace sensinghub {
namespace sessionlogger {
namespace V1_0 {


std::mutex LoggerFactory::sDtMutex;
std::unordered_map<suid, std::string, LoggerFactory::SuidHash, LoggerFactory::SuidEq>
    LoggerFactory::sDtCache = {
        // SUID sensor fixed suid used by SSC for SUID lookup.
        {{0xababababababababULL, 0xababababababababULL}, "suid"}
    };

std::shared_ptr<ISessionLogger> LoggerFactory::getLogger(const std::string& moduleName, const void* sessionKey) {
  try {
    return std::make_shared<SensorsDiagLogger>(moduleName, sessionKey);
  } catch (const std::exception&) {
    sns_logi("LoggerFactory: Diag is not available. Using default no-op implementation instead.");
    static std::shared_ptr<ISessionLogger> s_null_logger = std::make_shared<NullLogger>();
    return s_null_logger;
  }
}

const char* LoggerFactory::getDataType(const suid& sensorUid) {
  std::lock_guard<std::mutex> lk(sDtMutex);
  auto it = sDtCache.find(sensorUid);
  if (it == sDtCache.end()) {
    return "unknown";
  }
  return it->second.c_str();
}

void LoggerFactory::updateDataType(const suid& sensorUid, const std::string& datatype) {
  std::lock_guard<std::mutex> lk(sDtMutex);
  auto it = sDtCache.find(sensorUid);
  if (it == sDtCache.end()) {
    sDtCache.emplace(sensorUid, datatype);
    return;
  }

  if (it->second != datatype) {
    sns_loge("Datatype mismatch for suid(low=%" PRIu64 ",high=%" PRIu64 "): cached='%s' new='%s'",
              sensorUid.low, sensorUid.high, it->second.c_str(), datatype.c_str());
  }
}

}  // namespace V1_0
}  // namespace sessionlogger
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
