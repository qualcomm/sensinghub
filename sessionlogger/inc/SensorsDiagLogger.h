/** ============================================================================
 * @file
 *
 * @brief  DIAG-backed implementation of the ISessionLogger interface.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * ===========================================================================*/

#pragma once
#include <string>
#include <unistd.h>
#include <inttypes.h>
#include <mutex>
#include <vector>
#include "ISessionLogger.h"

namespace com {
namespace quic {
namespace sensinghub {
namespace sessionlogger {
namespace V1_0 {

/**
 * @brief DIAG backed session logger.
 *
 * Forwards sensor requests, responses, and events to the Qualcomm DIAG
 * subsystem via a dynamically loaded shim library. Each instance is bound
 * to a single session (identified by sessionKey) so that DIAG can
 * correlate log records with the originating session.
 *
 */
class SensorsDiagLogger : public ISessionLogger {
public:
  /**
   * @brief Construct a DIAG logger for a specific session.
   *
   * @param [in] moduleName  Human-readable module identifier (e.g. "APSS").
   * @param [in] sessionKey  Opaque pointer uniquely identifying the calling
   *                          session.
   *
   * @throws std::runtime_error if the DIAG is not available.
   */
  SensorsDiagLogger(std::string moduleName, const void* sessionKey);

  /**
   * @brief Log an outgoing sensor request via DIAG.
   *
   * @param [in] dataType    Sensor datatype string (e.g. "accel").
   * @param [in] requestBuf  Protocol-buffer-encoded request payload.
   * @param [in] requestLen  Length of requestBuf in bytes.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure
   */
  int logRequest(const char* dataType, const void* requestBuf, size_t requestLen) override;

  /**
   * @brief Log an incoming sensor response via DIAG.
   *
   * @param [in] dataType       Sensor datatype string (e.g. "accel").
   * @param [in] responseValue  Numeric result/error code from the response.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure
   */
  int logResponse(const char* dataType, const uint32_t responseValue) override;

  /**
   * @brief Log an incoming sensor event (indication) via DIAG.
   *
   * @param [in] dataType  Sensor datatype string (e.g. "accel").
   * @param [in] eventBuf  Protocol-buffer-encoded event payload.
   * @param [in] eventLen  Length of eventBuf in bytes.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure
   */
  int logEvent(const char* dataType, const void* eventBuf, size_t eventLen) override;

  /**
   * @brief Forward the client connection ID to the DIAG shim.
   *
   * Called when the Sensing Hub assigns a client ID to this session.
   * This is used to tag subsequent log records so they can be
   * correlated with the correct session.
   *
   * @param [in] clientId  Client connection ID assigned by the Sensing Hub.
   */
  void updateClientId(uint64_t clientId) override;

  /**
   * @brief Destructor. Unregisters the session from the DIAG shim.
   */
  ~SensorsDiagLogger() override;

private:
  /** C ABI function pointer type for logging a sensor request via DIAG. */
  using diagReqT   = int (*)(const void*, const char*, size_t, const char*, const char*);
  /** C ABI function pointer type for logging a sensor event (indication) via DIAG. */
  using diagEvtT   = int (*)(const void*, const char*, size_t, const char*, const char*);
  /** C ABI function pointer type for logging a sensor response via DIAG. */
  using diagRspT   = int (*)(const void*, uint32_t, const char*, const char*);
  /** C ABI function pointer type for updating the client connection ID in DIAG. */
  using diagSetIdT = void (*)(const void*, uint64_t);
  /** C ABI function pointer type for clearing per-session DIAG state. */
  using diagClearT = void (*)(const void*);

  /**
   * @brief Holds the dlopen handle and resolved function pointers for the
   *        DIAG shim library. A single process-wide instance is shared
   *        across all SensorsDiagLogger objects.
   */
  struct DiagShim {
    void*      handle = nullptr; /**< dlopen handle for libsnsdiagshim-c.so. */
    diagReqT   req    = nullptr; /**< Pointer to sns_diag_log_request_msg_c. */
    diagEvtT   evt    = nullptr; /**< Pointer to sns_diag_log_event_msg_c. */
    diagRspT   rsp    = nullptr; /**< Pointer to sns_diag_log_response_msg_c. */
    diagSetIdT setId  = nullptr; /**< Pointer to sns_diag_update_client_connect_id_c. */
    diagClearT clear  = nullptr; /**< Pointer to sns_diag_clear_session_c. */
  };

  /** Process-wide DIAG shim instance; loaded at most once via diagShimInit(). */
  static DiagShim _diagShim;

  /**
   * @brief Load the DIAG shim library and resolve all function pointers.
   *
   * No-op if the library has already been loaded. Must be called before
   * any log method is invoked.
   */
  static void diagShimInit();

  /**
   * @brief Return true if all DIAG shim function pointers are resolved.
   *
   * @return true if the shim is fully initialised and ready to use.
   */
  static bool isDiagShimAvailable();

  /**
   * @brief Check whether the DIAG shim is loaded and functional.
   *
   * Attempts to load DIAG library on the first call and caches the result.
   * Subsequent calls return the cached value.
   *
   * @return true if the DIAG is available; false otherwise.
   */
  static bool isAvailable();

  /** Logging module name. For E.g., APSS. */
  std::string _moduleName;

  /** ISession pointer used to bind logger to a single session . */
  const void* _sessionKey = nullptr;
};

}  // namespace V1_0
}  // namespace sessionlogger
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
