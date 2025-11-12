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

#include "sns_std_sensor.pb.h"
#include "sns_std_type.pb.h"
#include "sns_client.pb.h"
#include "sns_suid.pb.h"
#include "sns_std.pb.h"
#include "sns_cal.pb.h"

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
    sns_client_event_msg pb_event_msg;
    /* Parse the pb encoded event buffer*/
    pb_event_msg.ParseFromArray(data, size);

    /* Iterate over all events that are received */
    for (int i = 0; i < pb_event_msg.events_size(); i++) {
      auto& pb_event = pb_event_msg.events(i);
      /* parse SUID event packet */
      if (pb_event.msg_id() == SNS_SUID_MSGID_SNS_SUID_EVENT) {
        sns_suid_event pb_suid_event;
        pb_suid_event.ParseFromString(pb_event.payload());
        const string& datatype = pb_suid_event.data_type();
        printf("\nReceived SUIDs for %s, number of suids received = %d\n", datatype.c_str(), pb_suid_event.suid_size());

        /* create a list of all suids found for this sensorType */
        suidList.clear();
        suidList.resize(pb_suid_event.suid_size());
        for (int j = 0; j < pb_suid_event.suid_size(); j++) {
          printf("\nSUID received - suid_low=%" PRIu64 " suid_high=%" PRIu64 , pb_suid_event.suid(j).suid_low(), pb_suid_event.suid(j).suid_high());
          suidList[j] = suid(pb_suid_event.suid(j).suid_low(), pb_suid_event.suid(j).suid_high());
        }
      }
      else
        printf("\ninvalid event msg_id = %d", pb_event.msg_id());
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
  sns_suid_sensor suid_sensor;
  uid.low = (uint64_t)suid_sensor.suid_low();
  uid.high = (uint64_t)suid_sensor.suid_high();

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
  string pb_req_encoded = "";

  sns_suid_req pb_suid_req;
  pb_suid_req.set_data_type(sensorType);
  pb_suid_req.set_register_updates(true);
  pb_suid_req.set_default_only(true);
  pb_suid_req.SerializeToString(&pb_req_encoded);

  sns_client_request_msg pb_req_msg;
  pb_req_msg.set_msg_id(SNS_SUID_MSGID_SNS_SUID_REQ);
  pb_req_msg.mutable_request()->set_payload(pb_req_encoded);
  pb_req_msg.mutable_suid()->set_suid_high(uid.high);
  pb_req_msg.mutable_suid()->set_suid_low(uid.low);
  pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
  pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);

  string pb_req_msg_encoded;
  pb_req_msg.SerializeToString(&pb_req_msg_encoded);

  /* send proto encoded message to sensing-hub using the suidSession */
  ret = suidSession->sendRequest(uid, pb_req_msg_encoded);
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
    sns_client_event_msg pb_event_msg;
    /* Parse the pb encoded event buffer */
    pb_event_msg.ParseFromArray(data, size);

    /* Iterate over all events that are received */
    for (int i = 0; i < pb_event_msg.events_size(); i++) {
      auto& pb_event = pb_event_msg.events(i);

      /* parse attribute event packet */
      if(pb_event.msg_id() == SNS_STD_MSGID_SNS_STD_ATTR_EVENT){
        printf("\nAttributes for - suid_low=%" PRIu64 " suid_high=%" PRIu64 " are :\n" , pb_event_msg.suid().suid_low(), pb_event_msg.suid().suid_high());
        sns_std_attr_event pb_attr_event;
        pb_attr_event.ParseFromString(pb_event.payload());

        for (int i=0; i < pb_attr_event.attributes_size(); i++) {
          printf("attribute count %d \t and values are: ", i);
          sns_std_attr attr = pb_attr_event.attributes(i);
          printf("attr_id: %d \t " , attr.attr_id());
          sns_std_attr_value attr_value = attr.value();
          int attr_value_count = attr_value.values_size();
          for(int i = 0; i < attr_value_count ; i ++) {
            sns_std_attr_value_data val = attr_value.values(i);
            if(val.has_flt())
              printf("flt: %f\t" , val.flt());
            else if(val.has_sint())
              printf("sint: %" PRId64 , val.sint());
            else if(val.has_boolean())
              printf("boolean %d ", (int)val.boolean());
            else if(val.has_str())
              printf("std: %s " , val.str().c_str());
          }
          printf("\n");
        }
      }
      else
        printf("\ninvalid event msg_id = %d", pb_event.msg_id());
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
         printf("all callbacks are null, no need to register it");

    printf("\nrequesting attributes for - suid_low=%" PRIu64 " suid_high=%" PRIu64 "\n", uid.low, uid.high);

    /* create pb-encoded config request message to be sent for attribute query */
    sns_client_request_msg pb_req_msg;
    pb_req_msg.set_msg_id(SNS_STD_MSGID_SNS_STD_ATTR_REQ);
    pb_req_msg.mutable_request()->clear_payload();
    pb_req_msg.mutable_suid()->set_suid_high(uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(uid.low);
    pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);

    string pb_req_msg_encoded;
    pb_req_msg.SerializeToString(&pb_req_msg_encoded);

    /* send proto encoded message to sensing-hub using the attributeSession */
    ret = attributeSession->sendRequest(uid, pb_req_msg_encoded);
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
    sns_client_event_msg pb_event_msg;
    /* Parse the pb encoded event buffer */
    pb_event_msg.ParseFromArray(data, size);

    /* Iterate over all events that are received */
    for (int i = 0; i < pb_event_msg.events_size(); i++) {
      auto& pb_event = pb_event_msg.events(i);

      /* parse sensor data event packet */
      if(pb_event.msg_id() == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT){
        printf("\nReceived Samples:\t");
        sns_std_sensor_event pb_stream_event;
        pb_stream_event.ParseFromString(pb_event.payload());

        for(int i = 0 ; i < pb_stream_event.data_size() ; i++) {
          printf("[%f],\t", pb_stream_event.data(i));
        }
      }

      /* parse cal event packet */
      else if (pb_event.msg_id() == SNS_CAL_MSGID_SNS_CAL_EVENT) {
        printf("\nCal event packet received");
      }

      /* parse sensor re-configuration event packet */
      else if(pb_event.msg_id() == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT){
        printf("\nReceived re-configuration event");
      }
      else
        printf("\ninvalid event msg_id = %d", pb_event.msg_id());
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

  printf("\n\nStreaming started");

  static const uint64_t USEC_PER_SEC = 1000000ull;
  int batchPeriodMicroSec = batchPeriod * USEC_PER_SEC;

  for (const suid& uid : suidList){
    /* set callbacks for the session for 'uid' */
    int ret = streamingSession->setCallBacks(uid, dataResp, dataError, dataEvent);
    if(0 != ret)
      printf("all callbacks are null, no need to register it");

    printf("\nsending request for - suid_low=%" PRIu64 " suid_high=%" PRIu64 , uid.low, uid.high);

    /* create pb-encoded config request message to be sent for streaming request */
    string pb_req_encoded = "";

    sns_std_sensor_config pb_stream_cfg;
    pb_stream_cfg.set_sample_rate(sampleRate);
    pb_stream_cfg.SerializeToString(&pb_req_encoded);

    sns_client_request_msg pb_req_msg;
    pb_req_msg.mutable_request()->mutable_batching()->set_batch_period(batchPeriodMicroSec);
    pb_req_msg.set_msg_id(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG);
    pb_req_msg.mutable_request()->set_payload(pb_req_encoded);
    pb_req_msg.mutable_suid()->set_suid_high(uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(uid.low);
    pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);

    string pb_req_msg_encoded;
    pb_req_msg.SerializeToString(&pb_req_msg_encoded);

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
    sns_client_request_msg pb_req_msg;

    pb_req_msg.set_msg_id(SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ);
    pb_req_msg.mutable_suid()->set_suid_high(uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(uid.low);
    pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    pb_req_msg.mutable_request()->mutable_batching()->set_flush_period(UINT32_MAX);
    pb_req_msg.mutable_request()->mutable_batching()->set_batch_period(0);
    pb_req_msg.mutable_request()->clear_payload();

    string pb_req_msg_encoded;
    pb_req_msg.SerializeToString(&pb_req_msg_encoded);

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
