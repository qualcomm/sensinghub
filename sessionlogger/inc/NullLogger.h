/** ============================================================================
 * @file
 *
 * @brief  No-op implementation of the ISessionLogger interface.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * ===========================================================================*/

#pragma once
#include "ISessionLogger.h"

namespace com {
namespace quic {
namespace sensinghub {
namespace sessionlogger {
namespace V1_0 {

/**
 * @brief No-op logger used when no real logging backend is available.
 *
 * All methods silently succeed and perform no work. A single shared instance
 * is returned by LoggerFactory::getLogger() when default logger is not available,
 * avoiding per-session allocation overhead.
 */
class NullLogger : public ISessionLogger {
public:
  /** @brief No-op: discards the request without logging. @return 0 always. */
  int logRequest(const char*, const void*, size_t) override { return 0; }

  /** @brief No-op: discards the response without logging. @return 0 always. */
  int logResponse(const char*, const uint32_t) override { return 0; }

  /** @brief No-op: discards the event without logging. @return 0 always. */
  int logEvent(const char*, const void*, size_t) override { return 0; }

  /** @brief No-op: client ID is not tracked by this implementation. */
  void updateClientId(uint64_t) override {}

  ~NullLogger() override = default;
};

}  // namespace V1_0
}  // namespace sessionlogger
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
