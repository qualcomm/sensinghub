/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "suid.h"
#include "sns_client.pb.h"

using suid = com::quic::sensinghub::suid;

namespace com {
namespace quic {
namespace sensinghub {
namespace session {
namespace V1_0 {
namespace implementation {

/**
 * Update datatype cache based on SUID lookup event (SNS_SUID_EVENT) contained in a
 * sns_client_event_msg payload. This is a best-effort helper: decode failures are ignored.
 */
void updateDataTypeFromSuidEvent(const uint8_t* payload, size_t payload_len);

/**
 * Update datatype cache based on ATTR event (SNS_STD_MSGID_SNS_STD_ATTR_EVENT) contained in a
 * sns_client_event_msg payload. This is a best-effort helper: decode failures are ignored.
 */
void updateDataTypeFromAttrEvent(const suid& sensor_uid,
                                 const uint8_t* payload,
                                 size_t payload_len);

/**
 * Dispatch helper: scans the sns_client_event_msg payload and calls updateDataTypeFromSuidEvent
 * (when sensor_uid is the SUID lookup SUID and a SNS_SUID_EVENT is present) and
 * updateDataTypeFromAttrEvent. Call this from the indication path instead of calling the two
 * helpers individually.
 */
void updateDataTypeFromEventPayload(const suid& sensor_uid,
                                    const uint8_t* payload,
                                    size_t payload_len);

}  // namespace implementation
}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
