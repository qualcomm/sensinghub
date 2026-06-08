/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <string>
#include <cinttypes>
#include <cmath>
#include <chrono>
#include <fstream>
#include <string>
#include "sns_client.pb.h"
#include "sns_suid.pb.h"
#include "qshLog.h"
#include "suidLookUp.h"

using namespace std;
using namespace std::chrono;

struct suid_decode_context {
    std::vector<std::vector<suid>> *suids;
    std::vector<std::string> *datatype;
};

suidLookUp::suidLookUp(suidEventCb cb, int hubID)
  : mEventCb(cb)
{
  mSensorUid.low = 12370169555311111083ull;
  mSensorUid.high = 12370169555311111083ull;
  mSetThreadName = true;
  ISession::eventCallBack eventCallBack =[this](const uint8_t* msg , size_t msgLength, uint64_t timeStamp)
                { this->handleQshEvent(msg, msgLength, timeStamp); };

  mSessionFactory = make_unique<sessionFactory>();
  if(nullptr != mSessionFactory) {
    try {
      if(-1 == hubID) {
        mSession = unique_ptr<ISession>(mSessionFactory->getSession());
      }
      else{
        mSession = unique_ptr<ISession>(mSessionFactory->getSession(hubID));
      }
    } catch(const std::exception& e) {
      sns_loge("Exception creating session: %s", e.what());
      mSession = nullptr;
    } catch(...) {
      sns_loge("Unknown exception creating session");
      mSession = nullptr;
    }
    if(nullptr != mSession) {
      int ret = mSession->open();
      if(0 == ret){
        ret = mSession->setCallBacks(mSensorUid, nullptr, nullptr, eventCallBack);
        if(0 != ret){
          sns_loge("failed to set callbacks");
          mSession->close();
          mSession.reset();
          mSessionFactory.reset();
        }
      } else {
        mSession.reset();
      }
    } else {
      sns_loge("failed to create ISession");
      mSessionFactory.reset();
    }
  } else {
    sns_loge("failed to create sessionFactory");
  }
}

suidLookUp::~suidLookUp() {
  if (nullptr != mSession) {
    mSession->close();
  }
}
void suidLookUp::requestSuid(std::string datatype, bool default_only)
{
    pb_byte_t encoded_payload[100];
    sns_logv("requesting suid for %s, ts = %fs", datatype.c_str(),
             duration_cast<duration<float>>(high_resolution_clock::now().
                                            time_since_epoch()).count());

    pb_ostream_t payload_stream = pb_ostream_from_buffer(encoded_payload, sizeof(encoded_payload));
    sns_suid_req suid_req = sns_suid_req_init_default;
    qshPb::pb_buffer_arg buffer_arg = (qshPb::pb_buffer_arg){
      .buf = datatype.c_str(),
      .buf_len = datatype.length() + 1
    };

    /* populate SUID request */
    suid_req.has_register_updates = true;
    suid_req.register_updates = true;
    suid_req.has_default_only = true;
    suid_req.default_only = default_only;
    suid_req.data_type.funcs.encode = &qshPb::encode_bytes_callback;
    suid_req.data_type.arg = &buffer_arg;

    if (!pb_encode(&payload_stream, sns_suid_req_fields, &suid_req)) {
      sns_loge("lookup: sns_suid_req encoding failed: %s", PB_GET_ERROR(&payload_stream));
      return;
    }
    sns_logd("lookup: Encoded %zu bytes successfully", payload_stream.bytes_written);

    /* populate the client request message */
    qshPb::pb_buffer_arg buffer_arg_payload = (qshPb::pb_buffer_arg){
      .buf = encoded_payload,
      .buf_len = payload_stream.bytes_written
    };

    sns_std_suid _suid = {.suid_low = mSensorUid.low, .suid_high = mSensorUid.high};
    sns_client_request_msg request_msg = sns_client_request_msg_init_default;
    request_msg.suid = _suid;
    request_msg.msg_id = SNS_SUID_MSGID_SNS_SUID_REQ;
    request_msg.susp_config.client_proc_type = SNS_STD_CLIENT_PROCESSOR_APSS;
    request_msg.susp_config.delivery_type = SNS_CLIENT_DELIVERY_WAKEUP;
    request_msg.request.payload.funcs.encode = &qshPb::encode_bytes_callback;
    request_msg.request.payload.arg = &buffer_arg_payload;
    request_msg.has_client_tech = true;
    request_msg.client_tech = SNS_TECH_SENSORS;

    pb_byte_t buffer[256];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, sns_client_request_msg_fields, &request_msg)) {
        sns_loge("lookup: Failed to encode sns_client_request: %s\n", PB_GET_ERROR(&stream));
        return;
    }
    sns_logd("lookup: Encoded sns_client_request successfully (%zu bytes)\n", stream.bytes_written);
    std::string encoded_string(reinterpret_cast<char*>(buffer), stream.bytes_written);

    if (nullptr != mSession){
      int ret = mSession->sendRequest(mSensorUid, encoded_string);
      if(0 != ret){
        sns_loge("Error in sending request");
        return;
      }
    }
}

bool suid_decode_event(pb_istream_t *stream, const pb_field_t *field,
                               void **arg)
{
  auto *ctx = static_cast<suid_decode_context *>(*arg);
  if (!ctx || !ctx->suids || !ctx->datatype) {
      sns_loge("lookup: suid_decode_event ctx is null");
      return false;
  }

  bool rv = true;

  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;

  qshPb::pb_buffer_arg data;
  event.payload.funcs.decode = &qshPb::decode_payload;
  event.payload.arg = &data;

  if(!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                      &event))
  {
    sns_loge("lookup: sns_client_event decode failed %s", PB_GET_ERROR(stream));
    return false;
  }

  uint32_t msg_id = event.msg_id;
  sns_logd("event msg_id=%d", msg_id);
  if (msg_id == SNS_SUID_MSGID_SNS_SUID_DISCOVERY_DONE_EVENT){
    sns_logi("Received SUID Discovery Done Event");
    ctx->suids->emplace_back(std::vector<suid>{});
    ctx->datatype->emplace_back("SNS_SUID_MSGID_SNS_SUID_DISCOVERY_DONE_EVENT");
  } else if(msg_id == SNS_SUID_MSGID_SNS_SUID_EVENT) {
    if (!data.buf || data.buf_len == 0) {
        sns_loge("Empty payload in client event");
        return false;
    }

    pb_istream_t sub_stream = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t *>(data.buf),
      data.buf_len
    );

    sns_suid_event suid_event = sns_suid_event_init_default;
    std::vector<sns_std_suid> suid_vector;
    qshPb::pb_buffer_arg datatype_buf;
    suid_event.suid.funcs.decode = &qshPb::decode_suid;
    suid_event.suid.arg = &suid_vector;
    suid_event.data_type.funcs.decode = &qshPb::decode_payload;
    suid_event.data_type.arg = &datatype_buf;

    if(!pb_decode(&sub_stream, sns_suid_event_fields, &suid_event))
    {
      sns_loge("lookup: sns_suid_event decoding failed %s", PB_GET_ERROR(&sub_stream));
      return false;
    }

    const char* datatype =  (char *)datatype_buf.buf;
    sns_logd("suid_event for %s, num_suids=%d, ts=%fs", datatype,
              suid_vector.size(),
              duration_cast<duration<float>>(high_resolution_clock::now().
                                              time_since_epoch()).count());
    /* iterate over all events in the message */
    std::vector<suid> suid_list;
    for (int i = 0; i < suid_vector.size(); i++) {
      sns_std_suid _suid = suid_vector.at(i);
      suid_list.push_back(suid(_suid.suid_low,_suid.suid_high));
    }
    ctx->suids->emplace_back(std::move(suid_list));
    ctx->datatype->emplace_back(std::string(datatype));
  } else {
    sns_loge("invalid event msg_id=%d", msg_id);
    return false;
  }
  return rv;
}

void suidLookUp::handleQshEvent(const uint8_t *data, size_t size, uint64_t timeStamp)
{
    if (mSetThreadName == true) {
        mSetThreadName = false;
        int ret = 0;
        string pthreadName = "suidLookUp_see";
        ret = pthread_setname_np(pthread_self(), pthreadName.c_str());
        if (ret != 0) {
            sns_loge("Failed to set ThreadName: %s\n", pthreadName.c_str());
        }
    }
    /* parse the pb encoded event */
    std::vector<std::vector<suid>> suids_vector;
    std::vector<std::string> datatype_vector;

    suid_decode_context arg_context = {
        .suids = &suids_vector,
        .datatype = &datatype_vector
    };
    pb_istream_t stream = pb_istream_from_buffer(
      (const pb_byte_t *)data,
      size);
    sns_client_event_msg event  = sns_client_event_msg_init_default;
    event.events.funcs.decode = &suid_decode_event; //called for each repeated event
    event.events.arg          = &arg_context;

    if (!pb_decode(&stream, sns_client_event_msg_fields, &event)){
      sns_loge("lookup: sns_client_event decoding failed %s", PB_GET_ERROR(&stream));
    }
    /* send callback for this datatype */
    if(suids_vector.size() == datatype_vector.size() && datatype_vector.size() != 0){
      for(size_t i=0;i<datatype_vector.size();i++)
        if(datatype_vector[i] != "SNS_SUID_MSGID_SNS_SUID_DISCOVERY_DONE_EVENT")
          mEventCb(datatype_vector[i], suids_vector[i]);
    } else {
      sns_loge("lookup: sns_client_event events mismatch");
    }
}

locate::locate()
    : mSuids(std::make_shared<std::vector<suid>>()),
      mLookupDone(false) {}

void locate::onSuidsAvailable(const std::string& datatype, const std::vector<suid>& suids)
{
    // Notifying callback that SUID discovery is done using special value for datatype.
    if ((datatype.compare("SNS_SUID_MSGID_SNS_SUID_DISCOVERY_DONE_EVENT") == 0))
    {
        unique_lock<mutex> lk(mMutex);
        mLookupDone = true;
        mConditionVar.notify_one();
        return;
    }
    if (!mSuids)
    {
        sns_loge("mSuids pointer field is NULL.");
        return;
    }

    sns_logi("%u sensor(s) available for datatype %s",
        (unsigned int)suids.size(), datatype.c_str());

    unique_lock<mutex> lk(mMutex);
    mSuids->clear();
    for (size_t idx = 0; idx < suids.size(); idx++)
    {
        suid sensor_uid = { 0, 0 };
        sensor_uid.low = suids[idx].low;
        sensor_uid.high = suids[idx].high;

        sns_logi("datatype %s suid = [%" PRIx64 " %" PRIx64 "]",
            datatype.c_str(), sensor_uid.high, sensor_uid.low);

        mSuids->push_back(sensor_uid);
    }
}

std::shared_ptr<std::vector<suid>> locate::lookUp(const std::string& dataType, int hubId, std::chrono::milliseconds timeoutMs)
{
    if (!mSuids)
    {
        mSuids = std::make_shared<std::vector<suid>>();
    }

    if (mLookupDone)
    {
        return mSuids;
    }

    suidLookUp lookup(
        [this](const auto& dataType, const vector<suid>& suids)
        {
            this->onSuidsAvailable(dataType, suids);
        }, hubId);

    lookup.requestSuid(dataType);

    sns_logi("waiting for suid lookup");
    // TODO remove timeout once suid_lookup request_suid() is fixed
    const auto then = std::chrono::system_clock::now() + timeoutMs;

    unique_lock<mutex> lk(mMutex);
    while (!mLookupDone) {
        if (mConditionVar.wait_until(lk, then) == std::cv_status::timeout) {
            sns_logi("SUID lookup timeout(%lld ms) for datatype %s.", timeoutMs.count(), dataType.c_str());
            break;
        }
    }

    sns_logi("%s", "end suid lookup");
    return mSuids;
}
