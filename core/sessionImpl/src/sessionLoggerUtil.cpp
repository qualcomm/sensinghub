/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <string>
#include <vector>

#include "sns_suid.pb.h"
#include "sns_std_sensor.pb.h"
#include "pb_decode.h"

#include "qshPbSensorUtils.h"
#include "LoggerFactory.h"
#include "sessionLoggerUtil.h"

namespace com {
namespace quic {
namespace sensinghub {
namespace session {
namespace V1_0 {
namespace implementation {

void updateDataTypeFromSuidEvent(const uint8_t* payload, size_t payload_len) {
  std::vector<suid> suid_vector;
  std::string datatype;

  qshPb::suid_list arg_context = {
      .suids = &suid_vector,
      .datatype = &datatype
  };

  pb_istream_t stream = pb_istream_from_buffer(payload, payload_len);
  sns_client_event_msg pb_event_msg = sns_client_event_msg_init_default;
  pb_event_msg.events.funcs.decode = &qshPb::decode_suids;
  pb_event_msg.events.arg = &arg_context;

  if (!pb_decode(&stream, sns_client_event_msg_fields, &pb_event_msg)) {
    return;
  }

  if (datatype.empty() || suid_vector.empty()) {
    return;
  }

  if (!datatype.empty() && datatype.back() == '\0') {
    datatype.pop_back();
  }

  for (const auto& uid : suid_vector) {
    ::com::quic::sensinghub::sessionlogger::V1_0::LoggerFactory::updateDataType(uid, datatype);
  }
}

void updateDataTypeFromAttrEvent(const ::com::quic::sensinghub::suid& sensor_uid,
                                 const uint8_t* payload,
                                 size_t payload_len) {
  // if datatype is already known for this suid, skip decoding.
  if (std::string(::com::quic::sensinghub::sessionlogger::V1_0::LoggerFactory::getDataType(sensor_uid)) != "unknown") {
    return;
  }

  sns_client_event_msg pb_event_msg = sns_client_event_msg_init_default;
  pb_istream_t stream = pb_istream_from_buffer(payload, payload_len);

  struct Ctx {
    const ::com::quic::sensinghub::suid* uid;
  } ctx{&sensor_uid};

  // Decode each sns_client_event, but only parse payload when msg_id is ATTR.
  pb_event_msg.events.funcs.decode = [](pb_istream_t* istream, const pb_field_t*, void** arg) -> bool {
    auto* c = reinterpret_cast<Ctx*>(*arg);

    sns_client_event_msg_sns_client_event event = sns_client_event_msg_sns_client_event_init_default;
    qshPb::pb_buffer_arg data{};
    event.payload.funcs.decode = &qshPb::decode_payload;
    event.payload.arg = &data;

    if (!pb_decode(istream, sns_client_event_msg_sns_client_event_fields, &event)) {
      return false;
    }

    if (event.msg_id != SNS_STD_MSGID_SNS_STD_ATTR_EVENT) {
      return true;  // ignore non-ATTR
    }

    pb_istream_t sub_stream =
        pb_istream_from_buffer(reinterpret_cast<const pb_byte_t*>(data.buf), data.buf_len);

    qshPb::sensor_attributes attr_vals;
    sns_std_attr_event attr_event = sns_std_attr_event_init_default;
    attr_event.attributes.funcs.decode = &qshPb::decode_attribute;
    attr_event.attributes.arg = &attr_vals;

    if (!pb_decode(&sub_stream, sns_std_attr_event_fields, &attr_event)) {
      return false;
    }

    auto it = attr_vals.find(SNS_STD_SENSOR_ATTRID_TYPE);
    if (it != attr_vals.end() && !it->second.empty() && it->second[0].has_str) {
      ::com::quic::sensinghub::sessionlogger::V1_0::LoggerFactory::updateDataType(*c->uid, it->second[0].str);
    }
    return true;
  };
  pb_event_msg.events.arg = &ctx;

  // Best-effort: ignore decode failures (don't break event path).
  (void)pb_decode(&stream, sns_client_event_msg_fields, &pb_event_msg);
}

void updateDataTypeFromEventPayload(const ::com::quic::sensinghub::suid& sensor_uid,
                                    const uint8_t* payload,
                                    size_t payload_len) {
  static const ::com::quic::sensinghub::suid LOOKUP_SUID = {0xababababababababULL, 0xababababababababULL};

  if (sensor_uid.low == LOOKUP_SUID.low && sensor_uid.high == LOOKUP_SUID.high) {
    // Scan the events list and only attempt SUID decode if the message actually contains SNS_SUID_EVENT.
    bool has_suid_event = false;
    {
      pb_istream_t scan_stream = pb_istream_from_buffer(payload, payload_len);
      sns_client_event_msg scan_msg = sns_client_event_msg_init_default;

      struct ScanCtx {
        bool* has_suid_event;
      } scan_ctx{&has_suid_event};

      scan_msg.events.funcs.decode = [](pb_istream_t* istream, const pb_field_t*, void** arg) -> bool {
        auto* ctx = reinterpret_cast<ScanCtx*>(*arg);
        sns_client_event_msg_sns_client_event ev = sns_client_event_msg_sns_client_event_init_default;
        if (!pb_decode(istream, sns_client_event_msg_sns_client_event_fields, &ev)) {
          return false;
        }
        if (ev.msg_id == SNS_SUID_MSGID_SNS_SUID_EVENT) {
          *(ctx->has_suid_event) = true;
        }
        return true;
      };
      scan_msg.events.arg = &scan_ctx;

      // Best-effort scan; ignore failures.
      (void)pb_decode(&scan_stream, sns_client_event_msg_fields, &scan_msg);
    }

    if (has_suid_event) {
      updateDataTypeFromSuidEvent(payload, payload_len);
    }
  }

  updateDataTypeFromAttrEvent(sensor_uid, payload, payload_len);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
