/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

extern "C" {
#define PB_DEBUG 1
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
}

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

namespace qshPb {

/**
 * @brief Simple buffer descriptor used by nanopb callbacks.
 */
struct pb_buffer_arg {
  const void* buf;   //!< Pointer to input/output buffer
  size_t      buf_len;   //!< Length of buffer in bytes
};

struct decode_context_data {
  float* values;
  size_t count;
  size_t size;
};

/** -------------------- Encoding callbacks -------------------- */

/**
 * @brief Encode bytes field from a provided buffer.
 *
 * @param stream Nanopb output stream.
 * @param field  Target field metadata.
 * @param arg    Pointer to pb_buffer_arg (const void*).
 * @return true on success, false otherwise.
 */
bool encode_bytes_callback(pb_ostream_t* stream, const pb_field_t* field, void* const* arg);

/**
 * @brief Encode a C-string (UTF-8) for a string field.
 *
 * @param stream Nanopb output stream.
 * @param field  Target field metadata.
 * @param arg    Pointer to const char*.
 * @return true on success, false otherwise.
 */
bool encode_cstring_cb(pb_ostream_t* stream, const pb_field_t* field, void* const* arg);

/**
 * @brief Encode repeated fixed32 message IDs (no-wakeup IDs).
 *
 * @param stream Nanopb output stream.
 * @param field  Target field metadata (repeated fixed32).
 * @param arg    Pointer to const std::vector<uint32_t>.
 * @return true on success, false otherwise.
 */
bool encode_nowakeup_msg_ids(pb_ostream_t* stream, const pb_field_t* field, void* const* arg);

/**
 * @brief Encode repeated strings.
 *
 * @param stream Nanopb output stream.
 * @param field  Target field metadata (repeated fixed32).
 * @param arg    Pointer to const std::vector<uint32_t>.
 * @return true on success, false otherwise.
 */
bool encode_string_cb(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
/** -------------------- Decoding callbacks -------------------- */

/**
 * @brief Decode a raw payload into pb_buffer_arg without copying.
 *
 *        This sets pb_buffer_arg::buf to the stream's internal pointer and
 *        pb_buffer_arg::len to remaining bytes, then advances the stream.
 *
 * @param stream Nanopb input stream.
 * @param field  Source field metadata.
 * @param arg    Pointer to pb_buffer_arg*.
 * @return true on success, false otherwise.
 */
bool decode_payload(pb_istream_t* stream, const pb_field_t* field, void** arg);

/**
 * @brief Decode a string field to a std::string.
 *
 * @param stream Nanopb input stream.
 * @param field  Source field metadata.
 * @param arg    Pointer to std::string*.
 * @return true on success, false otherwise.
 */
bool decode_string(pb_istream_t* stream, const pb_field_t* field, void** arg);

/**
 * @brief Decode sns_std_suid entries into a vector.
 *
 * @param stream Nanopb input stream.
 * @param field  Source field metadata (submessage).
 * @param arg    Pointer to std::vector<sns_std_suid>*.
 * @return true on success, false otherwise.
 */
bool decode_suid(pb_istream_t* stream, const pb_field_t* field, void** arg);

/**
 * @brief Decode repeated float values into a vector.
 *
 * @param stream Nanopb input stream.
 * @param field  Source field metadata (repeated fixed32 or float).
 * @param arg    Pointer to std::vector<float>*.
 * @return true on success, false otherwise.
 */
bool decode_float_array(pb_istream_t* stream, const pb_field_t* field, void** arg);

/**
 * @brief Decode repeated enum values into a vector.
 *
 * @param stream Nanopb input stream.
 * @param field  Source field metadata (repeated fixed32 or float).
 * @param arg    Pointer to std::vector<float>*.
 * @return true on success, false otherwise.
 */
bool decode_repeated_enum(pb_istream_t *stream, const pb_field_t * /*field*/, void **arg);

/** -------------------- Utilities -------------------- */

/**
 * @brief Print a hex dump of the given buffer (16 bytes per row).
 *
 * @param data Pointer to data.
 * @param size Size in bytes.
 * @param title Optional label printed before the dump.
 */
void printHex(const pb_byte_t* data, size_t size, const char* title = "Hex Dump");

}  // namespace qshPb
