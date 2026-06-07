/** ============================================================================
 * @file
 *
 * @brief  Factory for creating per-session loggers and maintaining a
 *         process-wide SUID-to-datatype cache.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * ===========================================================================*/

#pragma once

#include "ISessionLogger.h"
#include "ISession.h"
#include <cinttypes>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using suid = com::quic::sensinghub::suid;

namespace com {
namespace quic {
namespace sensinghub {
namespace sessionlogger {
namespace V1_0 {

/**
 * @brief Factory and shared-state manager for session loggers.
 *
 * Provides two independent services:
 *
 *  1. **Logger creation** – getLogger() returns the most appropriate
 *     ISessionLogger implementation for the current runtime environment
 *     (DIAG-backed or no-op).
 *
 *  2. **SUID datatype cache** – A process-wide, thread-safe map from SUID to
 *     sensor datatype string (the value of SNS_STD_SENSOR_ATTRID_TYPE).
 *     Sessions populate the cache opportunistically as SUID-lookup and
 *     attribute events arrive, and query it when annotating log records.
 */
class LoggerFactory {
public:
  /**
   * @brief Create or retrieve a logger for a new session.
   *
   * Selects the best available logger implementation at runtime:
   *  - Returns a new SensorsDiagLogger instance when the DIAG shim is
   *    loaded and available.
   *  - Returns a shared singleton NullLogger otherwise (no allocation
   *    overhead per session).
   *
   * @param [in] moduleName  Human-readable name for the calling module
   *                          (e.g. "APSS"). Forwarded to the DIAG shim for
   *                          log record tagging.
   * @param [in] sessionKey  Opaque pointer that uniquely identifies the
   *                          calling session (typically `this`). Used by the
   *                          DIAG shim to correlate log records with a
   *                          specific session instance.
   *
   * @return Shared pointer to an ISessionLogger implementation. Never null.
   */
  static std::shared_ptr<ISessionLogger> getLogger(const std::string& moduleName, const void* sessionKey);

  /**
   * @brief Look up the cached datatype string for a SUID.
   *
   * Returns the SNS_STD_SENSOR_ATTRID_TYPE string that was previously
   * stored via updateDataType(). Thread-safe.
   *
   * @param [in] sensorUid  SUID to look up.
   *
   * @return Pointer to the cached datatype C-string, or "unknown" if the
   *         SUID has not yet been resolved. The returned pointer is valid for
   *         the lifetime of the process (backed by the static cache map).
   */
  static const char* getDataType(const suid& sensorUid);

  /**
   * @brief Update the cached datatype string for a SUID.
   *
   * Stores the mapping from sensorUid to datatype in the process-wide
   * cache. If a conflicting (different) datatype is already cached for the
   * same SUID, a warning is logged once and the existing value is kept.
   * Thread-safe.
   *
   * @param [in] sensorUid  SUID whose datatype is being recorded.
   * @param [in] datatype    Datatype string (value of
   *                         SNS_STD_SENSOR_ATTRID_TYPE, e.g. "accel").
   */
  static void updateDataType(const suid& sensorUid, const std::string& datatype);

private:
  /** Hash functor for suid. */
  struct SuidHash {
    std::size_t operator()(const suid& sensorUid) const {
      std::string data(reinterpret_cast<const char*>(&sensorUid), sizeof(suid));
      return std::hash<std::string>()(data);
    }
  };

  /** Equality functor for suid. */
  struct SuidEq {
    bool operator()(const suid& a, const suid& b) const {
      return a.low == b.low && a.high == b.high;
    }
  };

  /** Suid - Sensor DataType cache. */
  static std::unordered_map<suid, std::string, SuidHash, SuidEq> sDtCache;

  /** Private mutex for cache interactions. */
  static std::mutex sDtMutex;
};

}  // namespace V1_0
}  // namespace sessionlogger
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
