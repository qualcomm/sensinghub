#pragma once
/** ============================================================================
 * @file
 *
 * @brief  Logger interface for per-session request/response/event logging.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * ===========================================================================*/

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>

namespace com {
namespace quic {
namespace sensinghub {
namespace sessionlogger {
namespace V1_0 {

/**
 * @brief Abstract interface for per-session sensor logging.
 *
 * Implementations of this interface are responsible for recording sensor
 * requests, responses, and events for a single session. The interface is
 * intentionally minimal so that no-op and real (DIAG) implementations can
 * be swapped transparently.
 */
class ISessionLogger {
public:
  /**
   * @brief Log an outgoing sensor request.
   *
   * Called immediately before a request is sent to the Sensing Hub.
   *
   * @param [in] dataType    Sensor datatype string (e.g. "accel", "gyro").
   *                          May be "unknown" if the datatype has not yet been
   *                          resolved for this SUID.
   * @param [in] requestBuf  Pointer to the protocol-buffer-encoded request
   *                          payload.
   * @param [in] requestLen  Length of requestBuf in bytes.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure (e.g. underlying logging channel not available)
   */
  virtual int logRequest(const char* dataType, const void* requestBuf, size_t requestLen) = 0;

  /**
   * @brief Log an incoming sensor response.
   *
   * Called when a response is received from the Sensing Hub for a previously
   * sent request.
   *
   * @param [in] dataType       Sensor datatype string (e.g. "accel", "gyro").
   *                             May be "unknown" if not yet resolved.
   * @param [in] responseValue  Numeric result/error code from the response.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure (e.g. underlying logging channel not available)
   */
  virtual int logResponse(const char* dataType, const uint32_t responseValue) = 0;

  /**
   * @brief Log an incoming sensor event (indication).
   *
   * Called when an event indication is received from the Sensing Hub.
   *
   * @param [in] dataType  Sensor datatype string (e.g. "accel", "gyro").
   *                        May be "unknown" if not yet resolved.
   * @param [in] eventBuf  Pointer to the protocol-buffer-encoded event
   *                        payload.
   * @param [in] eventLen  Length of eventBuf in bytes.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure (e.g. underlying logging channel not available)
   */
  virtual int logEvent(const char* dataType, const void* eventBuf, size_t eventLen) = 0;

  /**
   * @brief Update the client ID associated with this session.
   *
   * Called when the Sensing Hub assigns (or re-assigns) a client connection ID
   * to this session. The default implementation is a no-op; concrete loggers
   * that need to tag log records with the client ID should override this.
   *
   * @param [in] clientId  Client connection ID assigned by the Sensing Hub.
   */
  virtual void updateClientId(uint64_t clientId) {}

  virtual ~ISessionLogger() = default;
protected:
};

}  // namespace V1_0
}  // namespace sessionlogger
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
