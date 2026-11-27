/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "SessionConfig.h"
#include <cstdint>

using namespace std;
using namespace com::quic::sensinghub::session::V1_0;

namespace {

/* Hand-rolled protobuf (proto2) wire encoding for sns_session_config_req */

void encodeVarint(uint64_t value, string& out) {
  while (value >= 0x80) {
    out.push_back(static_cast<char>((value & 0x7F) | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<char>(value));
}

void encodeTag(int fieldNumber, int wireType, string& out) {
  encodeVarint((static_cast<uint64_t>(fieldNumber) << 3) | wireType, out);
}

void encodeInt32Field(int fieldNumber, int32_t value, string& out) {
  encodeTag(fieldNumber, 0 /* varint */, out);
  encodeVarint(static_cast<uint64_t>(static_cast<int64_t>(value)), out);
}

void encodeBytesField(int fieldNumber, const string& value, string& out) {
  encodeTag(fieldNumber, 2 /* length-delimited */, out);
  encodeVarint(value.size(), out);
  out.append(value);
}

}  // namespace

string SessionConfig::encode(int hub_id) const {
  string encoded;
  if (hub_id != -1) {
    encodeInt32Field(1, hub_id, encoded);
  }
  if (!mDirectChannelRequest.empty()) {
    encodeBytesField(2, mDirectChannelRequest, encoded);
  }
  return encoded;
}
