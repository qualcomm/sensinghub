/*******************************************************************************
  @file    SessionClient.cpp
  @brief   This is an example code and can be used as a reference for using
           ISession APIs in order to interact with Sensing Hub.

  DESCRIPTION
  The sensor streams with the default configuration.
  Users may modify the configuration according to their requirements.

 ---------------------------------------------------------------------------
   Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
   SPDX-License-Identifier: BSD-3-Clause-Clear
 ---------------------------------------------------------------------------
 *******************************************************************************/

#include <iostream>
#include <cinttypes>
#include <unistd.h>
#include <vector>
#include <condition_variable>
#include <mutex>

extern "C" {

#define PB_DEBUG 1
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_std_sensor.pb.h"
#include "sns_std_type.pb.h"
#include "sns_client.pb.h"
#include "sns_suid.pb.h"
#include "sns_std.pb.h"
#include "sns_cal.pb.h"
}



#include "ISession.h"
#include "SessionFactory.h"

using namespace std;
using namespace ::com::quic::sensinghub::session::V1_0;

/*
 * default values for the streaming configuration:
 *    sensorType   - name of sensor to be streamed
 *    sampleRate   - sampling rate in Hz
 *    batchPeriod  - batching period in sec
 *    testDuration - duration in sec for which sensor will stream
 */
#define sensorType "accel"
#define sampleRate 10
#define batchPeriod 2
#define testDuration 10

/* vector to store suids of the required sensorType */
vector<suid> suidList;

struct pb_buffer_arg {
  const void* buf;   //!< Pointer to input/output buffer
  size_t      buf_len;   //!< Length of buffer in bytes
};

struct suid_decode_context {
    std::vector<suid> *suids;
    std::string *datatype;
};

/*=============================================================================
     HELPER FUNCTIONS :
=============================================================================*/

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

void printHex(const uint8_t* data, size_t size) {
    printf("Hex Dump of Encoded Payload:\n");
    for (size_t i = 0; i < size; ++i) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

uint32_t
get_msg_id(pb_istream_t *stream)
{
  sns_client_event_msg_sns_client_event event =
    sns_client_event_msg_sns_client_event_init_default;

  if(!pb_decode(stream, sns_client_event_msg_sns_client_event_fields, &event)) {
    printf("lookup: sns_client_event_msg decode failed %s", PB_GET_ERROR(stream));
  } else {
    return event.msg_id;
  }
  return 0;
}

/*=============================================================================
     FUNCTION : decode_suid_event
=============================================================================*/
/*!
@brief
  This function repeated decodes suid event.
*/

bool decode_suid_event(pb_istream_t *stream, const pb_field_t *field,
                               void **arg)
{
  bool rv = true;
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  pb_buffer_arg event_data;
  suid_decode_context *ctx = static_cast<suid_decode_context *>(*arg);

  event.payload.funcs.decode = &decode_payload;
  event.payload.arg = &event_data;

  pb_istream_t stream_cpy = *stream;
  uint32_t msg_id = get_msg_id(&stream_cpy);
  printf("event msg_id=%d", msg_id);

  if (msg_id != SNS_SUID_MSGID_SNS_SUID_EVENT) {
    printf("invalid event msg_id=%d", msg_id);
    return false;
  }
  if(!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                      &event))
  {
    printf("lookup: sns_client_event decode failed %s", PB_GET_ERROR(stream));
    return false;
  }
  else
  {
    pb_istream_t sub_stream = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t *>(event_data.buf),
      event_data.buf_len);
    sns_suid_event suid_event = sns_suid_event_init_default;
    pb_buffer_arg data;
    std::vector<sns_std_suid> suid_vector;

    suid_event.suid.funcs.decode = &decode_suid;
    suid_event.suid.arg = &suid_vector;
    suid_event.data_type.funcs.decode = &decode_payload;
    suid_event.data_type.arg = &data;

    if(!pb_decode(&sub_stream, sns_suid_event_fields, &suid_event))
    {
      printf("lookup: sns_suid_event decoding failed %s", PB_GET_ERROR(&sub_stream));
      return false;
    }else{
      ((pb_byte_t*)data.buf)[data.buf_len ? data.buf_len - 1 : 0] = '\0';
      const char* datatype = (const char *)data.buf;
            printf("\nReceived SUIDs for %s, number of suids received = %zu\n", datatype, suid_vector.size());

      /* iterate over all events in the message */
      for (size_t i = 0; i < suid_vector.size(); i++) {
        sns_std_suid _suid = suid_vector.at(i);
        printf("\nSUID received - suid_low=%" PRIu64 " suid_high=%" PRIu64 , _suid.suid_low, _suid.suid_high);
        ctx->suids->push_back(suid(_suid.suid_low,_suid.suid_high));
      }
      *(ctx->datatype) = std::string(datatype);
    }
  }
  return rv;
}

/*=============================================================================
     FUNCTION : getSuid
=============================================================================*/
/*!
@brief
  This function retrieves the list of all suids for the required sensorType.
*/
bool getSuid() {
  condition_variable eventCV;
  mutex eventMutex;

  /*  -----------------------------------------------------------------------------------
               define event callback, response callback, error callback pointers
      -----------------------------------------------------------------------------------  */
  ISession::eventCallBack suidEvent = [&](const uint8_t* data , size_t size, uint64_t timeStamp){
    printf("got suid event \n");
    sns_client_event_msg pb_event_msg = sns_client_event_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *) data, static_cast<pb_size_t>(size));

    std::vector<suid> suid_vector;
    std::string datatype;
    suid_decode_context arg_context = {
        .suids = &suid_vector,
        .datatype = &datatype
    };

    pb_event_msg.events.funcs.decode = &decode_suid_event; //called for each repeated event
    pb_event_msg.events.arg          = &arg_context;

    /* Parse the pb encoded event buffer*/
    if (!pb_decode(&stream, sns_client_event_msg_fields, &pb_event_msg)){
      printf("suid_event: sns_client_event encoding failed %s", PB_GET_ERROR(&stream));
    } else {
      if (suid_vector.empty()) {
        printf("Warning: No SUIDs received for sensor type\n");
      } else {
        /* Iterate over all events that are received */
        suidList.clear();
        suidList.resize(suid_vector.size());
        for (size_t j = 0; j < suid_vector.size(); j++) {
          suidList[j] = suid_vector[j];
        }
      }
    }
    /* notify the waiting thread when all events are parsed */
    unique_lock<mutex> lock(eventMutex);
    eventCV.notify_one();
  };


  ISession::respCallBack suidResp = [](uint32_t resp_value, uint64_t client_connect_id){
            cout<<"\nSUID discovery response received.";
  };

  ISession::errorCallBack suidError = [](ISession::error errorValue){
            /* User may add their own error handling mechanism incase any error is received */
  };

/*  -----------------------------------------------------------------------------------
                         send suid discovery request
    -----------------------------------------------------------------------------------  */

  /* using the suid of SUID Sensor for suid discovery request */
  suid uid;
  /* Use well-known SUID sensor defaults from sns_suid.proto */
  sns_suid_sensor suid_sensor = sns_suid_sensor_init_default;
  uid.low = suid_sensor.suid_low;
  uid.high = suid_sensor.suid_high;


  /* create a new ISession for suid discovery */
  unique_ptr<sessionFactory> factory = make_unique<sessionFactory>();
  if(nullptr == factory){
    printf("failed to create factory instance");
    return false;
  }

  unique_ptr<ISession> suidSession = unique_ptr<ISession>(factory->getSession());
  if(nullptr == suidSession){
    printf("failed to create session for suid discovery");
    return false;
  }

  /* open the suidSession */
  int ret = suidSession->open();
  if(-1 == ret){
    printf("failed to open ISession for suid discovery");
    return false;
  }

  /* set callbacks for the suidSession for 'uid' */
  ret = suidSession->setCallBacks(uid, suidResp, suidError, suidEvent);
  if(0 != ret)
     printf("all callbacks are null, no need to register it");

  /*
   * Create SUID request message
   * (Please refer sns_client.proto and sns_suid.proto for more details)
   * */

    const char* data_type = sensorType;

  pb_byte_t encoded_payload[100];
  pb_ostream_t payload_stream = pb_ostream_from_buffer(encoded_payload, sizeof(encoded_payload));

  sns_suid_req suid_req = sns_suid_req_init_default;
  pb_buffer_arg buffer_arg = (pb_buffer_arg){
    .buf = data_type,
    .buf_len = strlen(data_type) + 1
  };
  suid_req.has_register_updates = true;
  suid_req.register_updates = true;
  suid_req.has_default_only = true;
  suid_req.default_only = true;
  suid_req.data_type.funcs.encode = &encode_bytes_callback;
  suid_req.data_type.arg = &buffer_arg;

  if (!pb_encode(&payload_stream, sns_suid_req_fields, &suid_req)) {
    printf("suid_req encoding failed: %s\n", PB_GET_ERROR(&payload_stream));
    return 1;
  }
  printf("suid_req Encoded successfully (%zu bytes)\n", payload_stream.bytes_written);
  printHex(encoded_payload, payload_stream.bytes_written);

  sns_std_suid _suid = {.suid_low = uid.low, .suid_high = uid.high};
  sns_client_request_msg request_msg = sns_client_request_msg_init_default;

  request_msg.suid = _suid;
  request_msg.msg_id = SNS_SUID_MSGID_SNS_SUID_REQ;
  request_msg.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
  request_msg.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
  request_msg.susp_config.nowakeup_msg_ids.funcs.encode = NULL;  // No repeated values
  pb_buffer_arg buffer_arg_payload = (pb_buffer_arg){
    .buf = encoded_payload,
    .buf_len = payload_stream.bytes_written
  };
  request_msg.request.payload.funcs.encode = &encode_bytes_callback;
  request_msg.request.payload.arg = &buffer_arg_payload;

  pb_byte_t buffer[256];
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
  if (!pb_encode(&stream, sns_client_request_msg_fields, &request_msg)) {
      printf("client_request_msg failed to encode %s\n", PB_GET_ERROR(&stream));
      return 1;
  }
  printf("client_request_msg Encoded successfully (%zu bytes)\n", stream.bytes_written);
  printHex(buffer, stream.bytes_written);
  std::string encoded_string(reinterpret_cast<char*>(buffer), stream.bytes_written);

  /* send proto encoded message to sensing-hub using the suidSession */
  ret = suidSession->sendRequest(uid, encoded_string);
  if(0 != ret){
    printf("Error in sending suid discovery request");
    return false;
  }

  /* wait until all suids are received */
  unique_lock<mutex> eventLock(eventMutex);
  eventCV.wait(eventLock);

  /* close the session once suids are received */
  suidSession->close();
  printf("\nSensor suid list created\n");
  return true;
 }

/*=============================================================================
     FUNCTION :  decode_attr_value_data
=============================================================================*/
/*!
@brief
  This function decodes repeated std attribute values.
*/
bool decode_attr_value_data(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    sns_std_attr_value_data data = sns_std_attr_value_data_init_zero;

    std::string decoded_str;
    data.str.funcs.decode = &decode_string;
    data.str.arg = &decoded_str;
    data.subtype.values.funcs.decode = &decode_attr_value_data; // reuse this same function
    data.subtype.values.arg = NULL;

    if (!pb_decode(stream, sns_std_attr_value_data_fields, &data)) {
        return false;
    }
    if (!decoded_str.empty()) {
      printf("str: %s \n" , decoded_str.c_str());
    }
    if (data.has_flt) {
      printf("flt: %f\t\n" , data.flt);
    }
    if (data.has_sint) {
            printf("sint: %" PRId64 "\n" , data.sint);

    }
    if (data.has_boolean) {
        printf("boolean %d \n", (int)data.boolean);
    }
    printf("has subtype? : %d\n", data.has_subtype);
    return true;
}

/*=============================================================================
     FUNCTION :  handle_attribute_event
=============================================================================*/
/*!
@brief
  This function decodes repeated std attribute event.
*/
bool handle_attribute_event(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    sns_std_attr std_attr = sns_std_attr_init_default;

    std_attr.value.values.funcs.decode = &decode_attr_value_data; // repeated field
    std_attr.value.values.arg = NULL;

    if (!pb_decode(stream, sns_std_attr_fields, &std_attr)) {
        std::cerr << "Failed to decode sns_std_attr: " << PB_GET_ERROR(stream) << std::endl;
        return false;
    }

    printf("atrrtibute values are for attr_id: %d \t \n" , std_attr.attr_id);
    return true;
}

/*=============================================================================
     FUNCTION :  decode_attribute_event
=============================================================================*/
/*!
@brief
  This function decodes repeated attribute event.
*/
bool decode_attribute_event(pb_istream_t *stream, const pb_field_t *field,
                               void **arg)
{
  bool rv = true;
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  pb_buffer_arg data;

  event.payload.funcs.decode = &decode_payload;
  event.payload.arg = &data;

  if(!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                      &event))
  {
    printf("sns_client_event_msg_sns_client_event failed\n");
    return false;
  }
  else
  {
    uint32_t msg_id = event.msg_id;
    pb_istream_t sub_stream = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t *>(data.buf),
      data.buf_len);
    if (msg_id ==  SNS_STD_MSGID_SNS_STD_ATTR_EVENT) {
      sns_std_attr_event pb_attr_event = sns_std_attr_event_init_default;

      pb_attr_event.attributes.funcs.decode = &handle_attribute_event; //repeated field
      pb_attr_event.attributes.arg = nullptr;

      if(!pb_decode(&sub_stream, sns_std_attr_event_fields, &pb_attr_event))
      {
          printf("Error decoding attrib event: \n");
          return false;
      }
    }
  }
  return rv;
}

/*=============================================================================
     FUNCTION :  getAttributes
=============================================================================*/
/*!
@brief
  This function fetches the attributes of all suids for required sensorType.
*/
bool getAttributes(){
  condition_variable eventCV;
  mutex eventMutex;

  /*  -----------------------------------------------------------------------------------
               define event callback, response callback, error callback pointers
      -----------------------------------------------------------------------------------  */

  ISession::eventCallBack attributeEvent = [&](const uint8_t* data , size_t size, uint64_t timeStamp){
    printf("got attribute event \n");
    sns_client_event_msg pb_event_msg = sns_client_event_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *) data, static_cast<pb_size_t>(size));

    pb_event_msg.events.funcs.decode = &decode_attribute_event; // called for each event
    pb_event_msg.events.arg = NULL;

    if (!pb_decode(&stream, sns_client_event_msg_fields, &pb_event_msg)){
        printf("retrieve_attributes decoding failed \n");
    }

   /* notify the waiting thread when all events are parsed */
    unique_lock<mutex> lock(eventMutex);
    eventCV.notify_one();
  };

  ISession::respCallBack attributeResp = [](uint32_t resp_value, uint64_t client_connect_id){
          cout<<"\nAttribute query response received.";
  };

  ISession::errorCallBack attributeError = [](ISession::error errorValue){
            /* User may add their own error handling mechanism incase any error is received */
  };

/*  -----------------------------------------------------------------------------------
                             send attribute query request
    -----------------------------------------------------------------------------------  */

  /* create a new ISession for attribute query */
  unique_ptr<sessionFactory> factory = make_unique<sessionFactory>();
  if(nullptr == factory){
    printf("failed to create factory instance");
    return false;
  }

  unique_ptr<ISession> attributeSession = unique_ptr<ISession>(factory->getSession());
  if(nullptr == attributeSession){
    printf("failed to create session for attribute query");
    return false;
  }

  /* open the attributeSession session */
  int ret = attributeSession->open();
  if(-1 == ret){
    printf("failed to open ISession for attribute query");
    return false;
  }

  /*
   * For each suid in the list,
   *    - set callbacks
   *    - send pb-encoded request for attributes
   *    - the received attributes may be stored (here, they are simply being printed)
   * */
  for (const suid& uid : suidList) {
    /* set callbacks for the session for 'uid' */
    int ret = attributeSession->setCallBacks(uid, attributeResp, attributeError, attributeEvent);
    if(0 != ret)
         printf("all callbacks are null, no need to register it\n");
    printf("\nrequesting attributes for - suid_low=%" PRIu64 " suid_high=%" PRIu64 "\n", uid.low, uid.high);

    /* create pb-encoded config request message to be sent for attribute query */
    sns_client_request_msg request_msg = sns_client_request_msg_init_default;
    sns_std_suid _suid = {.suid_low = uid.low, .suid_high = uid.high};
    request_msg.suid = _suid;
    request_msg.msg_id = SNS_STD_MSGID_SNS_STD_ATTR_REQ;
    request_msg.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
    request_msg.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
    request_msg.request.payload.funcs.encode = NULL;

    pb_byte_t buffer[256];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, sns_client_request_msg_fields, &request_msg)) {
        printf("ATTR_REQ Failed to encode: %s\n", PB_GET_ERROR(&stream));
        return false;
    }
    printf("ATTR_REQ Encoded successfully (%zu bytes)\n", stream.bytes_written);
    std::string encoded_string(reinterpret_cast<char*>(buffer), stream.bytes_written);

    /* send proto encoded message to sensing-hub using the attributeSession */
    ret = attributeSession->sendRequest(uid, encoded_string);
    if(0 != ret){
        printf("Error in sending attribute query request");
        return false;
    }
  }

  /* wait until all requested attributes are received */
  unique_lock<mutex> eventLock(eventMutex);
  eventCV.wait(eventLock);

  /* close the session once all attributes are received */
  attributeSession->close();
  printf("\nAttributes for all suids received\n");
  return true;
}

/*=============================================================================
     FUNCTION :  decode_client_event_msg
=============================================================================*/
/*!
@brief
  This function decodes client event.
*/
bool decode_client_event(pb_istream_t *stream, const pb_field_t *field,
                               void **arg)
{
  bool rv = true;
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  pb_buffer_arg data;

  event.payload.funcs.decode = &decode_payload;
  event.payload.arg = &data;

  if(!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                      &event))
  {
    return false;
  }
  else
  {
    uint32_t msg_id = event.msg_id;
    printf("\nReceived Samples:\t event msg_id=%d, ts=%llu\n", msg_id,
                 (unsigned long long) event.timestamp);
    if (msg_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT) {
      sns_std_sensor_event pb_sensor_event = sns_std_sensor_event_init_default;
      std::vector<float> decoded_data;

      pb_istream_t sub_stream = pb_istream_from_buffer(
        reinterpret_cast<const pb_byte_t *>(data.buf),
        data.buf_len);
      pb_sensor_event.data.funcs.decode = &decode_float_array;
      pb_sensor_event.data.arg = &decoded_data;

      if(!pb_decode(&sub_stream, sns_std_sensor_event_fields, &pb_sensor_event))
      {
        printf("Error decoding event: ");
        return false;
      } else {
        for(size_t i = 0 ; i < decoded_data.size() ; i++) {
          printf("[%f],\t", decoded_data[i]);
        }
      }
    }
    /* parse cal event packet */
    else if (msg_id == SNS_CAL_MSGID_SNS_CAL_EVENT) {
      printf("\nCal event packet received");
    }
    /* parse sensor re-configuration event packet */
    else if(msg_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT){
      printf("\nReceived re-configuration event");
    }
    else
      printf("\ninvalid event msg_id = %d", msg_id);
  }
  return rv;
}

/*=============================================================================
     FUNCTION : startStreaming
=============================================================================*/
/*!
@brief
  This function starts streaming the sensor with the specified configurations.

  @param[in]   streamingSession       pointer to session dedicated for streaming activity
*/
bool startStreaming(ISession* streamingSession){

/*  -----------------------------------------------------------------------------------
             define event callback, response callback, error callback pointers
    -----------------------------------------------------------------------------------  */

  ISession::eventCallBack dataEvent = [&](const uint8_t* data , size_t size, uint64_t timeStamp){
    sns_client_event_msg pb_event_msg = sns_client_event_msg_init_default;

    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *) data, static_cast<pb_size_t>(size));
    pb_event_msg.events.funcs.decode = &decode_client_event;
    pb_event_msg.events.arg = NULL;
    if (!pb_decode(&stream, sns_client_event_msg_fields, &pb_event_msg)){
      printf("encoding failed \n");
    }
  };

  ISession::respCallBack dataResp = [](uint32_t resp_value, uint64_t client_connect_id){
          cout<<"\nData request response received.";
  };

  ISession::errorCallBack dataError = [](ISession::error errorValue){
            /* User may add their own error handling mechanism incase any error is received */
  };

/*  -----------------------------------------------------------------------------------
                               send streaming request
    -----------------------------------------------------------------------------------  */

  /*
   * For each suid in the list,
   *    - set callbacks
   *    - send pb-encoded request for data
   *    - the received events may be stored (here, they are simply being printed)
   * */
  printf("\nStreaming started\n");

  static const uint64_t USEC_PER_SEC = 1000000ull;
  int batchPeriodMicroSec = batchPeriod * USEC_PER_SEC;

  for (const suid& uid : suidList) {
    /* set callbacks for the session for 'uid' */
    int ret = streamingSession->setCallBacks(uid, dataResp, dataError, dataEvent);
    if(0 != ret)
      printf("all callbacks are null, no need to register it");

    /* create pb-encoded config request message to be sent for streaming request */
    sns_std_sensor_config pb_stream_cfg = sns_std_sensor_config_init_default;
    pb_stream_cfg.sample_rate = sampleRate;

    pb_byte_t sub_buffer[50];
    pb_ostream_t sub_stream = pb_ostream_from_buffer(sub_buffer, sizeof(sub_buffer));
    printf("sending SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG\n");

    sns_client_request_msg pb_req_msg = sns_client_request_msg_init_default;
    pb_buffer_arg buffer_arg_payload;
    if (pb_encode(&sub_stream, sns_std_sensor_config_fields, &pb_stream_cfg)) {
      printf("Encoded sensor_config successfully (%zu bytes)\n", sub_stream.bytes_written);
      buffer_arg_payload = (pb_buffer_arg){
        .buf = sub_buffer,
        .buf_len = sub_stream.bytes_written
      };
      pb_req_msg.request.payload.funcs.encode = &encode_bytes_callback;
      pb_req_msg.request.payload.arg = &buffer_arg_payload;
      pb_req_msg.msg_id = SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG;
    }else{
      printf("Failed to encode sns_client_request_msg: %s\n", PB_GET_ERROR(&sub_stream));
      continue;
    }
    sns_std_suid _suid = {.suid_low = uid.low, .suid_high = uid.high};
    pb_req_msg.suid = _suid;
    pb_req_msg.request.batching.batch_period = batchPeriodMicroSec;
    pb_req_msg.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
    pb_req_msg.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
    pb_req_msg.client_tech = SNS_TECH_SENSORS;

    pb_byte_t buffer[100];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, sns_client_request_msg_fields, &pb_req_msg)) {
        printf("Failed to encode sns_client_request_msg: %s\n", PB_GET_ERROR(&stream));
        return false;
    }
    printf("Encoded sns_client_request_msg successfully (%zu bytes)\n", stream.bytes_written);
    std::string pb_req_msg_encoded(reinterpret_cast<char*>(buffer), stream.bytes_written);

    /* send proto encoded message to sensing-hub using the streamingSession */
    ret = streamingSession->sendRequest(uid, pb_req_msg_encoded);
    if(0 != ret){
        printf("Error in sending streaming request");
        return false;
    }
  }
  return true;
}


/*=============================================================================
     FUNCTION : stopStreaming
=============================================================================*/
/*!
@brief
  This function sends disable request and eventually stops streaming the sensor.

  @param[in]   streamingSession       pointer to session dedicated for streaming activity
*/
bool stopStreaming(ISession* streamingSession){
  /*
   * For each suid in the list,
   *    - send pb-encoded disable request
   * */
  for (const suid& uid : suidList){
    /* create pb-encoded config request message to be sent for disable request */
    sns_client_request_msg pb_req_msg = sns_client_request_msg_init_default;
    pb_byte_t buffer[256];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    pb_req_msg.msg_id = SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ;
    sns_std_suid _suid = {.suid_low = uid.low, .suid_high = uid.high};
    pb_req_msg.suid = _suid;
    pb_req_msg.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
    pb_req_msg.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
    pb_req_msg.request.batching.flush_period = UINT32_MAX;
    pb_req_msg.request.batching.batch_period = 0;
    pb_req_msg.request.payload.funcs.encode = NULL;
    pb_req_msg.client_tech = SNS_TECH_SENSORS;

    if (!pb_encode(&stream, sns_client_request_msg_fields, &pb_req_msg)) {
        printf("Failed to encode DISABLE_REQ: %s\n", PB_GET_ERROR(&stream));
        return false;
    }
    printf("Encoded DISABLE_REQ successfully (%zu bytes)\n", stream.bytes_written);
    std::string pb_req_msg_encoded(reinterpret_cast<char*>(buffer), stream.bytes_written);

    /* send disable request to sensing-hub */
    int ret = streamingSession->sendRequest(uid, pb_req_msg_encoded);
    if(0 != ret){
        printf("Error in sending disable request");
        return false;
    }
  }
  printf("\n\nStopped streaming activity\n");
  return true;
}


/*=============================================================================
     FUNCTION : getSensorData
=============================================================================*/
/*!
@brief
  This function starts the streaming activity for 'testDuration' time,
  and then closes the session.
*/
bool getSensorData(){
  /* create a new ISession for streaming activity */
  unique_ptr<sessionFactory> factory = make_unique<sessionFactory>();
  if(nullptr == factory){
    printf("failed to create factory instance");
    return false;
  }

  ISession* streamingSession = factory->getSession();
  if(nullptr == streamingSession){
    printf("failed to create streaming session");
    return false;
  }

  /* open the streamingSession session */
  int ret = streamingSession->open();
  if(-1 == ret){
    printf("failed to open ISession for streaming");
    return false;
  }

  /* start sensor streaming activity */
  bool start = startStreaming(streamingSession);
  if(!start)
     return false;

  /* let the sensor stream for 'testDuration' seconds, after that stop streaming activity */
  sleep(testDuration);

  /* stop sensor streaming activity */
  bool stop = stopStreaming(streamingSession);
  if(!stop)
     return false;

  /* close and delete the streamingSession */
  streamingSession->close();
  delete streamingSession;
  return true;
}

/*=============================================================================
     FUNCTION : MAIN
=============================================================================*/

int main(int argc, char *argv[]){
  /* =========================================================================================================
   *
   * User should create an ISession using sessionFactory class instance.
   *
   * The typical call flow of ISession APIs for interacting with Sensing Hub:
   *
   * 1. Open newly created Isession.
   * 2. For the successfully opened ISession,
   *      2.1 set the callBacks for the required suid
   *      2.2 send request for the required suid
   * 3. Close ISession.
   *
   * Once completed, user should free-up the resources, and delete ISession and sessionFactory instance.
   *
   *
   * In this example, we are using different ISession for suid discovery, attribute query & streaming activity.
   * However, same ISession can be used for all the activities.
   * ===========================================================================================================*/

  printf("Streaming configuration is as follows :\n");
  printf("\tSensor name : %s", sensorType);
  printf("\tSample rate : %d Hz", sampleRate);
  printf("\tBatch period : %d sec", batchPeriod);
  printf("\tTest duration : %d sec", testDuration);
  printf("\n\n");

  /* retrieve suids for the specified sensor */
  bool suidCheck = getSuid();
  if(!suidCheck){
    printf("\nCould not receive suids for the sensor, sensor streaming can't be done");
    return -1;
  }

  /* retrieve attributes for the specified sensor */
  bool attributeCheck = getAttributes();
  if(!attributeCheck){
    printf("\nCould not receive attributes for the sensor, sensor streaming can't be done");
    return -1;
  }

  /* stream the sensor and get the data events */
  bool dataCheck = getSensorData();
  if(!dataCheck){
    printf("\nError in receiving data events, sensor streaming didn't complete successfully");
    return -1;
  }

  return 0;
}
