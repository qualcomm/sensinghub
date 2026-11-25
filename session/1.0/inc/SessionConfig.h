#pragma once
/** ============================================================================
 * @file
 *
 * @brief  SessionConfig is an opaque, setter-configured object used to pass
 * extensible session creation parameters (e.g. hub id, direct-channel create
 * request) to sessionFactory::getSession(const SessionConfig*) without
 * exposing a struct layout across the client ABI boundary.
 *
 * @copyright
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ===========================================================================*/

#include <string>

namespace com {
namespace quic {
namespace sensinghub {
namespace session {
namespace V1_0 {

class sessionFactory;

/**
 * @class SessionConfig
 * @brief Opaque configuration object populated and consumed exclusively by
 * sessionFactory. Construction, mutation, and protobuf encoding are private
 * and restricted to sessionFactory via friend access, so clients cannot
 * construct a SessionConfig or call its setters/encoder directly - they must
 * go through sessionFactory
 *
 */
class SessionConfig {
public:
  ~SessionConfig() {}

private:
  friend class sessionFactory;

  SessionConfig() {}

  void setDirectChannelRequest(const char* encodedRequest, int requestLen) {
    mDirectChannelRequest.assign(encodedRequest, requestLen);
  }

  /**
   * @brief Hand-encodes this config's fields into the sns_session_config_req
   * wire format without depending on nanopb.
   *
   * @return Encoded protobuf bytes.
   */
  std::string encode(int hub_id) const;

  std::string mDirectChannelRequest;
};

}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
