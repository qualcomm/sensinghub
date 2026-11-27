/*******************************************************************************
 * @file    SessionClient.cpp
 * @brief   This is an example code and can be used as a reference for using
 *          ISession APIs in order to interact with Sensing Hub.
 *
 * DESCRIPTION
 * Follow main() function to understand the sequence of operations
 * @copyright
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 ---------------------------------------------------------------------------
 *******************************************************************************/


#include <chrono>
#include <condition_variable>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <functional>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#define PB_DEBUG 1
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_cal.pb.h"
#include "sns_client.pb.h"
#include "sns_std.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_std_type.pb.h"
#include "sns_suid.pb.h"
}

#include "ISession.h"
#include "SessionFactory.h"

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;
using com::quic::sensinghub::suid;

// Kept as a global for compatibility with the existing SessionClient tests.
std::vector<suid> suidList;
int gSelectedHubId = -1;

namespace {

constexpr char mSensorType[] = "accel";
constexpr int mSampleRateHz = 10;
constexpr int mBatchPeriodSec = 2;
constexpr int mTestDurationSec = 5;
constexpr int mDefaultHubId = -1;
constexpr bool mMultihubTarget = true;
constexpr auto mWaitTimeout = std::chrono::seconds(10);

struct Buffer {
  Buffer(const void* bufferData = nullptr, size_t bufferSize = 0)
      : data(bufferData), size(bufferSize) {}

  const void* data;
  size_t size;
};

struct SuidDecodeContext {
  std::vector<suid>* suids;
  std::atomic<bool>* discoveryDone;
};

class EventWaiter {
 public:
  /**
   * @brief Wait for an asynchronous session callback with a bounded timeout.
   */
  bool waitFor(const std::function<bool()>& predicate) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, mWaitTimeout, predicate);
  }

  void notify() {
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_one();
  }

 private:
  std::condition_variable cv_;
  std::mutex mutex_;
};

bool encodeBytes(pb_ostream_t* stream, const pb_field_t* field,
                 void* const* arg) {
  const auto* buffer = static_cast<const Buffer*>(*arg);
  return pb_encode_tag_for_field(stream, field) &&
         pb_encode_string(stream, static_cast<const pb_byte_t*>(buffer->data),
                          buffer->size);
}

bool decodeBytes(pb_istream_t* stream, const pb_field_t*, void** arg) {
  auto* buffer = static_cast<Buffer*>(*arg);
  buffer->size = stream->bytes_left;
  buffer->data = stream->state;
  return pb_read(stream, nullptr, stream->bytes_left);
}

bool decodeString(pb_istream_t* stream, const pb_field_t*, void** arg) {
  auto* output = static_cast<std::string*>(*arg);
  output->assign(stream->bytes_left, '\0');
  return output->empty() ||
         pb_read(stream, reinterpret_cast<pb_byte_t*>(&(*output)[0]),
                 output->size());
}

bool decodeSuid(pb_istream_t* stream, const pb_field_t*, void** arg) {
  auto* output = static_cast<std::vector<sns_std_suid>*>(*arg);
  sns_std_suid value = sns_std_suid_init_default;
  if (!pb_decode_noinit(stream, sns_std_suid_fields, &value)) {
    return false;
  }
  output->push_back(value);
  return true;
}

bool decodeSuidEvent(pb_istream_t* stream, const pb_field_t*, void** arg) {
  auto* context = static_cast<SuidDecodeContext*>(*arg);
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  Buffer payload;
  event.payload.funcs.decode = decodeBytes;
  event.payload.arg = &payload;

  if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                 &event)) {
    return false;
  }

  printf("Event received from SUID sensor, msg_id=%" PRIu32 "\n", event.msg_id);

  if (event.msg_id == SNS_SUID_MSGID_SNS_SUID_DISCOVERY_DONE_EVENT) {
    context->discoveryDone->store(true);
    printf("SUID discovery done event\n");
    return true;
  }
  if (event.msg_id != SNS_SUID_MSGID_SNS_SUID_EVENT) {
    return true;
  }

  sns_suid_event suidEvent = sns_suid_event_init_default;
  std::vector<sns_std_suid> values;
  suidEvent.suid.funcs.decode = decodeSuid;
  suidEvent.suid.arg = &values;
  if (!payload.data) return false;
  pb_istream_t payloadStream = pb_istream_from_buffer(
      static_cast<const pb_byte_t*>(payload.data), payload.size);
  if (!pb_decode(&payloadStream, sns_suid_event_fields, &suidEvent)) {
    return false;
  }

  for (const auto& value : values) {
    context->suids->emplace_back(value.suid_low, value.suid_high);
    printf("  SUID low=%" PRIu64 " high=%" PRIu64 "\n",
           value.suid_low, value.suid_high);
  }
  return true;
}

std::string encodeSuidRequest(const suid& discoverySensor) {
  Buffer type{ mSensorType, sizeof(mSensorType) };
  sns_suid_req request = sns_suid_req_init_default;
  request.has_register_updates = true;
  request.register_updates = true;
  request.has_default_only = true;
  request.default_only = true;
  request.data_type.funcs.encode = encodeBytes;
  request.data_type.arg = &type;

  pb_byte_t payload[128];
  pb_ostream_t payloadStream = pb_ostream_from_buffer(payload, sizeof(payload));
  if (!pb_encode(&payloadStream, sns_suid_req_fields, &request)) {
    return {};
  }

  sns_client_request_msg clientRequest = sns_client_request_msg_init_default;
  clientRequest.suid = {discoverySensor.low, discoverySensor.high};
  clientRequest.msg_id = SNS_SUID_MSGID_SNS_SUID_REQ;
  clientRequest.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
  clientRequest.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
  Buffer encodedPayload{payload, payloadStream.bytes_written};
  clientRequest.request.payload.funcs.encode = encodeBytes;
  clientRequest.request.payload.arg = &encodedPayload;

  pb_byte_t output[256];
  pb_ostream_t stream = pb_ostream_from_buffer(output, sizeof(output));
  if (!pb_encode(&stream, sns_client_request_msg_fields, &clientRequest)) {
    return {};
  }
  return std::string(reinterpret_cast<char*>(output), stream.bytes_written);
}

std::string encodeEmptyRequest(const suid& sensor, uint32_t messageId) {
  sns_client_request_msg request = sns_client_request_msg_init_default;
  request.suid = {sensor.low, sensor.high};
  request.msg_id = messageId;
  request.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
  request.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;

  pb_byte_t output[256];
  pb_ostream_t stream = pb_ostream_from_buffer(output, sizeof(output));
  if (!pb_encode(&stream, sns_client_request_msg_fields, &request)) {
    return {};
  }
  return std::string(reinterpret_cast<char*>(output), stream.bytes_written);
}

/**
 * @brief Discover the requested sensor SUIDs on one sensing hub.
 *
 * Each hub has its own ISession. The callback is registered before sending
 * the request because the response can arrive immediately afterward.
 */
bool discoverOnHub(int hubId, const std::string& request,
                   std::vector<suid>* discovered) {
  printf("\nDiscovering %s on hub %d\n", mSensorType, hubId);
  sessionFactory factory;
  ISession* session = factory.getSession(hubId);
  if (!session || session->open() != 0) {
    printf("Unable to open SUID session for hub %d\n", hubId);
    delete session;
    return false;
  }

  EventWaiter waiter;
  std::atomic<bool> discoveryDone{false};
  std::vector<suid> hubSuids;
  sns_suid_sensor suid_sensor_defaults = sns_suid_sensor_init_default;
  const suid suidSensor{suid_sensor_defaults.suid_low,
                             suid_sensor_defaults.suid_high};

  ISession::eventCallBack callback = [&](const uint8_t* data, size_t size,
                                          uint64_t) {
    sns_client_event_msg message = sns_client_event_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    SuidDecodeContext context{&hubSuids, &discoveryDone};
    message.events.funcs.decode = decodeSuidEvent;
    message.events.arg = &context;
    if (!pb_decode(&stream, sns_client_event_msg_fields, &message)) {
      printf("Failed to decode SUID event for hub %d\n", hubId);
      return;
    }
    if (discoveryDone) {
      waiter.notify();
    }
  };

  if (session->setCallBacks(suidSensor, nullptr, nullptr, callback) != 0 ||
      session->sendRequest(suidSensor, request) != 0) {
    printf("Failed to register callback or send SUID request on hub %d\n",
           hubId);
    session->close();
    delete session;
    return false;
  }

  const bool completed = waiter.waitFor([&] { return discoveryDone.load(); });
  session->close();
  *discovered = std::move(hubSuids);
  if (!completed && discovered->empty()) {
    printf("Timed out discovering SUIDs on hub %d\n", hubId);
    delete session;
    return false;
  }

  if (!completed) {
    printf("Discovery complete event not received on hub %d; using %zu SUID(s) received\n",
           hubId, discovered->size());
  }
  printf("Hub %d returned %zu matching SUID(s)\n", hubId, discovered->size());
  delete session;
  return true;
}

bool decodeAttributeValue(pb_istream_t* stream, const pb_field_t*, void**) {
  sns_std_attr_value_data value = sns_std_attr_value_data_init_zero;
  std::string text;
  value.str.funcs.decode = decodeString;
  value.str.arg = &text;
  if (!pb_decode(stream, sns_std_attr_value_data_fields, &value)) {
    return false;
  }
  if (!text.empty()) printf("attribute string: %s\n", text.c_str());
  if (value.has_flt) printf("attribute float: %f\n", value.flt);
  if (value.has_sint) printf("attribute integer: %" PRId64 "\n", value.sint);
  if (value.has_boolean) printf("attribute boolean: %d\n", value.boolean);
  return true;
}

bool decodeAttribute(pb_istream_t* stream, const pb_field_t*, void**) {
  sns_std_attr attribute = sns_std_attr_init_default;
  attribute.value.values.funcs.decode = decodeAttributeValue;
  if (!pb_decode(stream, sns_std_attr_fields, &attribute)) {
    return false;
  }
  printf("attribute id=%" PRIu32 "\n", attribute.attr_id);
  return true;
}

struct AttributeDecodeContext {
  bool handled = false;
};

bool decodeAttributeEvent(pb_istream_t* stream, const pb_field_t*, void** arg) {
  auto* context = static_cast<AttributeDecodeContext*>(*arg);
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  Buffer payload;
  event.payload.funcs.decode = decodeBytes;
  event.payload.arg = &payload;
  if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields, &event)) {
    return false;
  }
  if (event.msg_id != SNS_STD_MSGID_SNS_STD_ATTR_EVENT || !payload.data) {
    return true;
  }
  sns_std_attr_event attributes = sns_std_attr_event_init_default;
  attributes.attributes.funcs.decode = decodeAttribute;
  pb_istream_t payloadStream = pb_istream_from_buffer(
      static_cast<const pb_byte_t*>(payload.data), payload.size);
  if (!pb_decode(&payloadStream, sns_std_attr_event_fields, &attributes)) {
    return false;
  }
  context->handled = true;
  return true;
}

/**
 * @brief Query attributes for every SUID discovered on the selected hub.
 */
bool requestAttributes(int hubId) {
  printf("\nRequesting attributes from hub %d\n", hubId);
  sessionFactory factory;
  ISession* session = factory.getSession(hubId);
  if (!session || session->open() != 0) {
    delete session;
    return false;
  }

  EventWaiter waiter;
  std::mutex countMutex;
  size_t received = 0;
  const size_t expected = suidList.size();
  ISession::eventCallBack callback = [&](const uint8_t* data, size_t size,
                                          uint64_t) {
    sns_client_event_msg message = sns_client_event_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    AttributeDecodeContext context;
    message.events.funcs.decode = decodeAttributeEvent;
    message.events.arg = &context;
    if (!pb_decode(&stream, sns_client_event_msg_fields, &message) ||
        !context.handled) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(countMutex);
      ++received;
    }
    waiter.notify();
  };

  for (const auto& sensor : suidList) {
    printf("Sending attribute request for SUID low=%" PRIu64 " high=%" PRIu64 "\n",
           sensor.low, sensor.high);
    if (session->setCallBacks(sensor, nullptr, nullptr, callback) != 0 ||
        session->sendRequest(sensor, encodeEmptyRequest(
            sensor, SNS_STD_MSGID_SNS_STD_ATTR_REQ)) != 0) {
      session->close();
      return false;
    }
  }

  const bool completed = waiter.waitFor([&] {
    std::lock_guard<std::mutex> lock(countMutex);
    return received >= expected;
  });
  session->close();
  delete session;
  if (completed) {
    printf("All accel attributes received\n");
  } else {
    printf("Timed out waiting for accel attributes\n");
  }
  return completed;
}

bool decodeFloat(pb_istream_t* stream, const pb_field_t*, void** arg) {
  auto* values = static_cast<std::vector<float>*>(*arg);
  uint32_t bits = 0;
  if (!pb_decode_fixed32(stream, &bits)) return false;
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  values->push_back(value);
  return true;
}

bool decodeSensorEvent(pb_istream_t* stream, const pb_field_t*, void**) {
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  Buffer payload;
  event.payload.funcs.decode = decodeBytes;
  event.payload.arg = &payload;
  if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields, &event)) {
    return false;
  }
  printf("\nsensor event msg_id=%" PRIu32, event.msg_id);
  if (event.msg_id != SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT || !payload.data) {
    return true;
  }
  sns_std_sensor_event sensorEvent = sns_std_sensor_event_init_default;
  std::vector<float> values;
  sensorEvent.data.funcs.decode = decodeFloat;
  sensorEvent.data.arg = &values;
  pb_istream_t payloadStream = pb_istream_from_buffer(
      static_cast<const pb_byte_t*>(payload.data), payload.size);
  if (!pb_decode(&payloadStream, sns_std_sensor_event_fields, &sensorEvent)) {
    return false;
  }
  for (float value : values) printf("  %f", value);
  printf("\n");
  return true;
}

/**
 * @brief Configure the first discovered SUID and register its data callback.
 *
 * Sensor samples are delivered through the event callback registered here.
 */
bool startStreaming(ISession* session) {
  printf("\nStarting accel streaming on hub %d\n", gSelectedHubId);
  ISession::eventCallBack callback = [](const uint8_t* data, size_t size,
                                        uint64_t) {
    sns_client_event_msg message = sns_client_event_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    message.events.funcs.decode = decodeSensorEvent;
    if (!pb_decode(&stream, sns_client_event_msg_fields, &message)) {
      printf("Failed to decode sensor event\n");
    }
  };

  if (suidList.empty()) return false;
  const auto& sensor = suidList.front();
  printf("Configuring SUID low=%" PRIu64 " high=%" PRIu64
         ", rate=%d Hz, batch=%d sec\n", sensor.low, sensor.high,
         mSampleRateHz, mBatchPeriodSec);
  if (session->setCallBacks(sensor, nullptr, nullptr, callback) != 0) {
    return false;
  }
  sns_std_sensor_config config = sns_std_sensor_config_init_default;
  config.sample_rate = mSampleRateHz;
  pb_byte_t configBytes[64];
  pb_ostream_t configStream = pb_ostream_from_buffer(configBytes, sizeof(configBytes));
  if (!pb_encode(&configStream, sns_std_sensor_config_fields, &config)) {
    return false;
  }

  sns_client_request_msg request = sns_client_request_msg_init_default;
  request.suid = {sensor.low, sensor.high};
  request.msg_id = SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG;
  request.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
  request.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
  request.request.batching.batch_period = mBatchPeriodSec * 1000000;
  Buffer payload{configBytes, configStream.bytes_written};
  request.request.payload.funcs.encode = encodeBytes;
  request.request.payload.arg = &payload;

  pb_byte_t requestBytes[128];
  pb_ostream_t requestStream = pb_ostream_from_buffer(requestBytes, sizeof(requestBytes));
  if (!pb_encode(&requestStream, sns_client_request_msg_fields, &request)) {
    return false;
  }
  if (session->sendRequest(sensor, std::string(reinterpret_cast<char*>(requestBytes),
                                                requestStream.bytes_written)) != 0) {
    return false;
  }
  return true;
}

/**
 * @brief Disable the configured sensor stream before closing the session.
 */
bool stopStreaming(ISession* session) {
  printf("\nStopping accel streaming\n");
  if (suidList.empty()) return false;
  const auto& sensor = suidList.front();
  const std::string request = encodeEmptyRequest(
      sensor, SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ);
  if (request.empty() || session->sendRequest(sensor, request) != 0) {
    return false;
  }
  return true;
}

}  // namespace
/**
 * @brief Discover accel on every available hub and select the first match.
 * If the target has a single sensing hub, just use mDefaultHubId
 */
bool getSuid() {
  printf("\nStarting SUID discovery for %s\n", mSensorType);
  suidList.clear();
  std::vector<suid> discovered;
  gSelectedHubId = mDefaultHubId;
  const sns_suid_sensor suid_sensor_defaults = sns_suid_sensor_init_default;
  const suid suidSensor{suid_sensor_defaults.suid_low, suid_sensor_defaults.suid_high};

  const std::string request = encodeSuidRequest(suidSensor);
  if(discoverOnHub(mDefaultHubId, request, &discovered)){
    suidList = std::move(discovered);
  }

  return !suidList.empty();
}

/**
 * @brief Discover accel on every available hub and select the first match.
 * If the target has a single sensing hub, just use mDefaultHubId
 */
bool getSuidFromMultiHub() {
  printf("\nStarting SUID discovery for %s\n", mSensorType);
  suidList.clear();
  gSelectedHubId = mDefaultHubId;
  const sns_suid_sensor suid_sensor_defaults = sns_suid_sensor_init_default;
  const suid suidSensor{suid_sensor_defaults.suid_low,
                        suid_sensor_defaults.suid_high};
  const std::string request = encodeSuidRequest(suidSensor);
  sessionFactory factory;
  const std::vector<int> hubIds = factory.getSensingHubIds();
  for (int hubId : hubIds) {
    std::vector<suid> discovered;
    if (discoverOnHub(hubId, request, &discovered)) {
      if (!discovered.empty()) {
        gSelectedHubId = hubId;
        suidList = std::move(discovered);
        printf("Selected hub %d for %s\n", gSelectedHubId, mSensorType);
        break;
      }
    }
  }
  return !suidList.empty();
}

/**
 * @brief Retrieve attributes for the SUIDs found on the selected hub.
 */
bool getAttributes() {
  printf("\nStarting attribute query\n");
  return !suidList.empty() &&
         requestAttributes(gSelectedHubId);
}

/**
 * @brief Stream sensor data, then disable the stream and close the session.
 *
 * This demonstrates the complete open -> setCallBacks -> sendRequest ->
 * disable -> close lifecycle.
 */
bool getSensorData() {
  printf("\nOpening streaming session for hub %d\n", gSelectedHubId);
  sessionFactory factory;
  ISession* session = factory.getSession(gSelectedHubId);
  if (!session) {
    printf("Failed to create streaming session\n");
    return false;
  }

  if (session->open() != 0) {
    printf("Failed to open streaming session\n");
    delete session;
    return false;
  }

  if (!startStreaming(session)) {
    printf("Failed to start streaming\n");
    session->close();
    delete session;
    return false;
  }

  std::this_thread::sleep_for(std::chrono::seconds(mTestDurationSec));
  const bool stopped = stopStreaming(session);
  session->close();
  delete session;
  printf("Streaming session closed\n");
  return stopped;
}

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
 * Once completed, user should free-up the resources, and delete ISession and
 * sessionFactory instance.
 *
 * To get data from a specific sensor, for example "accel"
 * User will discover accel's suid from the suid sensor
 * Stream accel using the discovered suid
 *
 * In this example, we are showing a general steps to stream a sensor
 * 1. Discover suid
 * 2. Reteieve attributes
 * 3. Start stream
 * 4. Stop stream
 * 5. Close
 *
 * The example uses different ISession for each step. However, same ISession
 * can be used for all the steps.
 *
 * Multihub :
 *
 * Some targets may have multiple sensing-hub instances with different
 * configurations. On these targets, clients must call getSensingHubIds and
 * discover the SUID on each hub. Once found, use the corresponding hubId to
 * stream the sensor. Follow getSuidFromMultiHub() for multihub
 * ===========================================================================================================*/

int main(int, char**) {
  printf("Streaming configuration:\n"
         "  sensor: %s\n"
         "  sample rate: %d Hz\n"
         "  batch period: %d sec\n"
         "  duration: %d sec\n", mSensorType, mSampleRateHz,
         mBatchPeriodSec, mTestDurationSec);
  bool suidFound = false;
  if (!mMultihubTarget) {
    suidFound = getSuid();
  } else {
    suidFound = getSuidFromMultiHub();
  }
  if (!suidFound) {
    printf("%s SUID not found\n", mSensorType);
    return -1;
  }
  printf("SUID discovery succeeded\n");
  if (!getAttributes()) {
    printf("Unable to retrieve attributes for %s datatype\n", mSensorType);
    return -1;
  }
  printf("Attribute query succeeded\n");
  return getSensorData() ? 0 : -1;
}
