/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "qshPb.h"
#include "qshPbSensorUtils.h"
#include "qshLog.h"

extern "C" {
#include "sns_suid.pb.h"
#include "sns_std_type.pb.h"
#include "sns_client.pb.h"
}

#include <chrono>
#include <cstdio>
#include <cstring>  // for std::strlen

namespace qshPb {

    /* -------------------- Encoding -------------------- */

    bool encode_bytes_callback(pb_ostream_t* stream, const pb_field_t* field, void* const* arg) {
        const auto* buffer = static_cast<const pb_buffer_arg*>(*arg);
        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }
        // For nanopb, "bytes" and "string" both use pb_encode_string on raw bytes.
        return pb_encode_string(stream,
                                reinterpret_cast<const pb_byte_t*>(buffer->buf),
                                buffer->buf_len);
    }

    bool encode_cstring_cb(pb_ostream_t* stream, const pb_field_t* field, void* const* arg) {
        const char* str = static_cast<const char*>(*arg);
        if (str == nullptr) {
            str = "";
        }
        const size_t len = std::strlen(str);

        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }
        return pb_encode_string(stream,
                                reinterpret_cast<const pb_byte_t*>(str),
                                len);
    }

    bool encode_nowakeup_msg_ids(pb_ostream_t* stream, const pb_field_t* field, void* const* arg) {
        const auto* msg_ids = static_cast<const std::vector<uint32_t>*>(*arg);
        for (uint32_t id : *msg_ids) {
            if (!pb_encode_tag_for_field(stream, field)) {
            return false;
            }
            if (!pb_encode_fixed32(stream, &id)) {
            return false;
            }
        }
        return true;
    }

    bool encode_string_cb(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
        if (!stream || !field || !arg || !*arg) return false;
        const auto* s = static_cast<const pb_buffer_arg*>(*arg);
        if (!pb_encode_tag_for_field(stream, field)) return false;
        if (!pb_encode_varint(stream, s->buf_len))     return false;
        if (s->buf_len > 0 && !pb_write(stream, reinterpret_cast<const pb_byte_t*>(s->buf), s->buf_len)) 
            return false;
    return true;
}

    /* -------------------- Decoding -------------------- */

    bool decode_string(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg) {
        auto* out_str = static_cast<std::string*>(*arg);

        // Read the remaining bytes in the stream for this field.
        const size_t len = stream->bytes_left;
        if (len == 0) {
            out_str->clear();
            return true;
        }
        std::string tmp;
        tmp.resize(len);

        if (!pb_read(stream, reinterpret_cast<pb_byte_t*>(&tmp[0]), len)) {
            return false;
        }
        *out_str = std::move(tmp);
        return true;
    }

    bool decode_payload(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg) {
        auto* data = static_cast<pb_buffer_arg*>(*arg);
        data->buf_len = stream->bytes_left;
        data->buf = stream->state;
        // Advance the stream by consuming the remaining bytes.
        return pb_read(stream, nullptr, stream->bytes_left);
    }

    bool decode_suid(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg) {
        sns_std_suid uid = sns_std_suid_init_default;
        auto* suid_list = static_cast<std::vector<sns_std_suid>*>(*arg);

        if (!pb_decode_noinit(stream, sns_std_suid_fields, &uid)) {
            return false;
        }

        suid_list->push_back(uid);
        return true;
    }

    bool decode_float_array(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg) {
        auto* vec = static_cast<std::vector<float>*>(*arg);
        float value;
        while (stream->bytes_left > 0) {
            if (!pb_decode_fixed32(stream, &value)) {
            return false;
            }
            vec->push_back(value);
        }
        return true;
    }

    bool decode_repeated_enum(pb_istream_t *stream,
                            const pb_field_t * /*field*/,
                            void **arg) {
        auto *ctx = static_cast<std::vector<uint32_t> *>(*arg);
        if (!ctx) {
            return false;
        }
        while (stream->bytes_left > 0) {
            uint64_t raw = 0;
            if (!pb_decode_varint(stream, &raw)) {
            return false; // Propagate decode error.
            }
            ctx->push_back(static_cast<uint32_t>(raw));
        }
        return true;
    }


    /* -------------------- Sensor PB utilities -------------------- */

    /**
     * @brief Internal callback that decodes a single float subtype value from
     *        an sns_std_attr_value_data submessage.
     *
     * Appends the decoded float to the std::vector<float> passed via arg.
     */
    static bool decode_attribute_value_subtype(pb_istream_t* stream,
                                               const pb_field_t* /*field*/,
                                               void** arg)
    {
      sns_std_attr_value_data attr = sns_std_attr_value_data_init_default;
      std::vector<float>* ctx = static_cast<std::vector<float>*>(*arg);

      if (!pb_decode(stream, sns_std_attr_value_data_fields, &attr)) {
        return false;
      }
      if (attr.has_flt) {
        ctx->push_back(attr.flt);
      }
      return true;
    }

    /**
     * @brief Internal callback that decodes a single sns_std_attr_value_data
     *        entry and appends an attribute to the output vector.
     */
    static bool decode_attribute_value(pb_istream_t* stream,
                                       const pb_field_t* /*field*/,
                                       void** arg)
    {
      sns_std_attr_value_data attr = sns_std_attr_value_data_init_default;
      attributes* attr_val = static_cast<attributes*>(*arg);
      attribute new_attr = {};

      pb_buffer_arg data;
      attr.str.funcs.decode = &decode_payload;
      attr.str.arg = &data;
      attr.subtype.values.funcs.decode = &decode_attribute_value_subtype;
      attr.subtype.values.arg = &new_attr.subtype;

      if (!pb_decode(stream, sns_std_attr_value_data_fields, &attr)) {
        return false;
      }

      if (data.buf_len > 0) {
        const char* str =  (char *)data.buf;
        new_attr.str = std::string(str);
        new_attr.has_str = true;
      } else if (attr.has_flt) {
        new_attr.flt = attr.flt;
        new_attr.has_flt = true;
      } else if (attr.has_sint) {
        new_attr.sint = attr.sint;
        new_attr.has_sint = true;
      } else if (attr.has_boolean) {
        new_attr.boolean = attr.boolean;
        new_attr.has_boolean = true;
      } else if (attr.has_subtype) {
        new_attr.has_subtype = true;
      }

      attr_val->push_back(new_attr);
      return true;
    }

    uint32_t get_msg_id(pb_istream_t stream)
    {
      sns_client_event_msg_sns_client_event event =
          sns_client_event_msg_sns_client_event_init_default;

      if (!pb_decode(&stream, sns_client_event_msg_sns_client_event_fields, &event)) {
        sns_loge("get_msg_id: decode failed");
      } else {
        return event.msg_id;
      }
      return 0;
    }

    bool decode_suids(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg)
    {
      sns_client_event_msg_sns_client_event event =
          sns_client_event_msg_sns_client_event_init_default;
      pb_buffer_arg data;
      suid_list* ctx = static_cast<suid_list*>(*arg);

      event.payload.funcs.decode = &decode_payload;
      event.payload.arg = &data;

      uint32_t msg_id = get_msg_id(*stream);
      sns_logd("decode_suids: event msg_id=%d", msg_id);

      if (msg_id != SNS_SUID_MSGID_SNS_SUID_EVENT) {
        sns_loge("decode_suids: unexpected msg_id=%d", msg_id);
        return true;
      }
      if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields, &event)) {
        return false;
      }

      pb_istream_t sub_stream = pb_istream_from_buffer(
          reinterpret_cast<const pb_byte_t*>(data.buf), data.buf_len);
      sns_suid_event suid_event = sns_suid_event_init_default;
      pb_buffer_arg dt_data;
      std::vector<sns_std_suid> suid_vector;

      suid_event.suid.funcs.decode = &decode_suid;
      suid_event.suid.arg = &suid_vector;
      suid_event.data_type.funcs.decode = &decode_payload;
      suid_event.data_type.arg = &dt_data;

      if (!pb_decode(&sub_stream, sns_suid_event_fields, &suid_event)) {
        sns_loge("decode_suids: failed to decode sns_suid_event");
        return false;
      }

      std::string datatype;
      if (dt_data.buf != nullptr && dt_data.buf_len > 0) {
        datatype.assign(reinterpret_cast<const char*>(dt_data.buf), dt_data.buf_len);
        // Trim any trailing NUL that some encoders append.
        if (!datatype.empty() && datatype.back() == '\0') {
          datatype.pop_back();
        }
      }

      sns_logi("decode_suids: datatype=%s num_suids=%zu", datatype.c_str(), suid_vector.size());
      for (const sns_std_suid& s : suid_vector) {
        sns_logd("decode_suids: suid_low=%" PRIu64 " suid_high=%" PRIu64,
                 s.suid_low, s.suid_high);
        ctx->suids->push_back(suid(s.suid_low, s.suid_high));
      }
      *(ctx->datatype) = datatype;
      return true;
    }

    bool decode_attribute(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg)
    {
      sns_std_attr attr = sns_std_attr_init_default;
      sensor_attributes* attr_list = static_cast<sensor_attributes*>(*arg);
      attributes attr_val;

      attr.value.values.funcs.decode = &decode_attribute_value;
      attr.value.values.arg = &attr_val;

      if (!pb_decode(stream, sns_std_attr_fields, &attr)) {
        sns_loge("decode_attribute: failed: %s", PB_GET_ERROR(stream));
        return false;
      }

      (*attr_list)[attr.attr_id] = attr_val;
      return true;
    }

    bool decode_attributes(pb_istream_t* stream, const pb_field_t* /*field*/, void** arg)
    {
      sns_client_event_msg_sns_client_event event =
          sns_client_event_msg_sns_client_event_init_default;
      pb_buffer_arg data;
      sensor_attributes* ctx = static_cast<sensor_attributes*>(*arg);

      event.payload.funcs.decode = &decode_payload;
      event.payload.arg = &data;

      uint32_t msg_id = get_msg_id(*stream);
      sns_logd("decode_attributes: event msg_id=%d", msg_id);

      if (msg_id != SNS_STD_MSGID_SNS_STD_ATTR_EVENT) {
        sns_loge("decode_attributes: unexpected msg_id=%d", msg_id);
        return true;
      }
      if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields, &event)) {
        return false;
      }

      pb_istream_t sub_stream = pb_istream_from_buffer(
          reinterpret_cast<const pb_byte_t*>(data.buf), data.buf_len);
      sns_std_attr_event attr_event = sns_std_attr_event_init_default;

      attr_event.attributes.funcs.decode = &decode_attribute;
      attr_event.attributes.arg = ctx;

      if (!pb_decode(&sub_stream, sns_std_attr_event_fields, &attr_event)) {
        sns_loge("decode_attributes: failed to decode sns_std_attr_event");
        return false;
      }

      sns_logd("decode_attributes: decoded successfully");
      return true;
    }

    /* -------------------- Utilities -------------------- */

    void printHex(const uint8_t* data, size_t size, const char* title) {
        for (size_t i = 0; i < size; ++i) {
            std::printf("%02X ", data[i]);
            if ((i + 1) % 16 == 0) {
            std::printf("\n");
            }
        }
    }
}

