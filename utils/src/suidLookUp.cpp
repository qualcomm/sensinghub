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
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "sns_client.pb.h"
#include "sns_suid.pb.h"
#include "qshLog.h"
#include "suidLookUp.h"

using namespace std;
using namespace google::protobuf::io;
using namespace std::chrono;

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
void suidLookUp::requestSuid(std::string datatype, bool defaultOnly)
{
    sns_client_request_msg pbReqMessage;
    sns_suid_req pbSuidReq;
    string pbSuidReqEncoded;
    sns_logv("requesting suid for %s, ts = %fs", datatype.c_str(),
             duration_cast<duration<float>>(high_resolution_clock::now().
                                            time_since_epoch()).count());

    /* populate SUID request */
    pbSuidReq.set_data_type(datatype);
    pbSuidReq.set_register_updates(true);
    pbSuidReq.set_default_only(defaultOnly);
    pbSuidReq.SerializeToString(&pbSuidReqEncoded);

    /* populate the client request message */
    pbReqMessage.set_msg_id(SNS_SUID_MSGID_SNS_SUID_REQ);
    pbReqMessage.mutable_request()->set_payload(pbSuidReqEncoded);
    pbReqMessage.mutable_suid()->set_suid_high(mSensorUid.high);
    pbReqMessage.mutable_suid()->set_suid_low(mSensorUid.low);
    pbReqMessage.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pbReqMessage.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    pbReqMessage.set_client_tech(SNS_TECH_SENSORS);
    string pbReqMsgEncoded;
    pbReqMessage.SerializeToString(&pbReqMsgEncoded);
    if (nullptr != mSession){
      int ret = mSession->sendRequest(mSensorUid, pbReqMsgEncoded);
      if(0 != ret){
        sns_loge("Error in sending request");
        return;
      }
    }
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
    sns_client_event_msg pbEventMessage;
    pbEventMessage.ParseFromArray(data, size);
    /* iterate over all events in the message */
    for (int i = 0; i < pbEventMessage.events_size(); i++) {
        auto& pbEvent = pbEventMessage.events(i);
        if (pbEvent.msg_id() != SNS_SUID_MSGID_SNS_SUID_EVENT) {
            sns_loge("invalid event msg_id=%d", pbEvent.msg_id());
            continue;
        }
        sns_suid_event pbSuidEvent;
        pbSuidEvent.ParseFromString(pbEvent.payload());
        const string& datatype =  pbSuidEvent.data_type();

        sns_logv("suid_event for %s, num_suids=%d, ts=%fs", datatype.c_str(),
                 pbSuidEvent.suid_size(),
                 duration_cast<duration<float>>(high_resolution_clock::now().
                                                time_since_epoch()).count());

        /* create a list of  all suids found for this datatype */
        vector<suid> suids(pbSuidEvent.suid_size());
        for (int j=0; j < pbSuidEvent.suid_size(); j++) {
            suids[j] = suid(pbSuidEvent.suid(j).suid_low(),
                                  pbSuidEvent.suid(j).suid_high());
        }
        /* send callback for this datatype */
        mEventCb(datatype, suids);
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
