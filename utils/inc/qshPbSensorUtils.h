/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

/**
 * @file qshSensorPbUtils.h
 *
 * @brief Sensor-specific protobuf decode utilities built on top of qshPb.
 *
 */

#include "qshPb.h"
#include <map>
#include "suid.h"
#include "sns_suid.pb.h"
#include "sns_std_type.pb.h"
#include "sns_client.pb.h"

namespace qshPb {

/**
 * @brief Convenience alias for the public session SUID type.
 *
 * Allows sensor-PB utility code inside this namespace to refer to
 * suid without a fully-qualified prefix.
 */
using suid = com::quic::sensinghub::suid;

/**
 * @brief Output context passed to the decode_suids() nanopb callback.
 *
 * The callback appends each decoded SUID to suids and stores the
 * sensor datatype string in datatype.
 */
struct suid_list {
  std::vector<suid>* suids;    /**< Destination vector for decoded SUIDs. */
  std::string*       datatype; /**< Destination for the sensor datatype string. */
};

/**
 * @brief Holds a single decoded sns_std_attr_value_data value.
 *
 * Exactly one of the has_* flags will be true after decoding.
 */
struct attribute {
  bool               has_subtype; /**< True if the value is a nested subtype. */
  std::vector<float> subtype;     /**< Decoded float subtype values. */
  bool               has_str;     /**< True if the value is a string. */
  std::string        str;         /**< Decoded string value. */
  bool               has_flt;     /**< True if the value is a float. */
  float              flt;         /**< Decoded float value. */
  bool               has_sint;    /**< True if the value is a signed integer. */
  int64_t            sint;        /**< Decoded signed integer value. */
  bool               has_boolean; /**< True if the value is a boolean. */
  bool               boolean;     /**< Decoded boolean value. */
};

/** @brief A list of attribute values for a single attribute ID. */
using attributes = std::vector<attribute>;

/**
 * @brief Map from attribute ID (sns_std_attr_id) to its decoded values.
 *
 * Populated by decode_attributes().
 */
using sensor_attributes = std::map<int32_t, attributes>;

/**
 * @brief Extract the message ID from a client event stream.
 *
 * Decodes only the msg_id field of the first
 * sns_client_event_msg_sns_client_event in stream.
 *
 * @param stream  A copy of the nanopb input stream positioned at the event.
 * @return The decoded message ID, or 0 on failure.
 */
uint32_t get_msg_id(pb_istream_t stream);

/**
 * @brief Nanopb repeated-field callback that decodes SUID lookup events.
 *
 * Expects the stream to contain sns_client_event_msg_sns_client_event
 * records carrying SNS_SUID_MSGID_SNS_SUID_EVENT payloads.
 * Appends each discovered SUID to suid_list::suids and stores the
 * datatype string in suid_list::datatype.
 *
 * @param stream Nanopb input stream.
 * @param field  Field descriptor (unused).
 * @param arg    Pointer to a suid_list output context.
 * @return true on success, false on decode error.
 */
bool decode_suids(pb_istream_t* stream, const pb_field_t* field, void** arg);

/**
 * @brief Nanopb repeated-field callback that decodes a single sns_std_attr.
 *
 * Appends the decoded attribute and its values to the sensor_attributes
 * map passed via arg.
 *
 * @param stream Nanopb input stream.
 * @param field  Field descriptor (unused).
 * @param arg    Pointer to a sensor_attributes output map.
 * @return true on success, false on decode error.
 */
bool decode_attribute(pb_istream_t* stream, const pb_field_t* field, void** arg);

/**
 * @brief Nanopb repeated-field callback that decodes attribute events.
 *
 * Expects the stream to contain sns_client_event_msg_sns_client_event
 * records carrying SNS_STD_MSGID_SNS_STD_ATTR_EVENT payloads.
 * Populates the sensor_attributes map passed via arg.
 *
 * @param stream Nanopb input stream.
 * @param field  Field descriptor (unused).
 * @param arg    Pointer to a sensor_attributes output map.
 * @return true on success, false on decode error.
 */
bool decode_attributes(pb_istream_t* stream, const pb_field_t* field, void** arg);

}  // namespace qshPb
