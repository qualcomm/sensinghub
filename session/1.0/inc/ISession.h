#pragma once
/** ============================================================================
 * @file
 *
 * @brief  Interface to interact with Sensing Hub
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * ===========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include <string>
#include <functional>
#include <memory>
#include "suid.h"

namespace com {
namespace quic {
namespace sensinghub {
namespace session {
namespace V1_0 {

using namespace ::com::quic::sensinghub;

/*==============================================================================
  Type Definitions
  ============================================================================*/

/**
 * @class ISession
 * @brief ISession is transport-neutral, OS-agnostic, extensible interface
 *        for communicating to Sensing Hub. It exposes APIs for clients to:
 *         (1) Establish a session to Sensing Hub.
 *         (2) Register callbacks per sensor SUID.
 *         (3) Send proto-encoded requests and receive responses/events asynchronously.
 *
 * @example Clients can refer to the example code : examples/SessionClient/SessionClient.cpp
 */
class ISession {
public:

 /**
  * @brief Error codes, if any, from ISession
  *
  */
  enum error
  {
    RESET,        /*!< Indicates a reset of the Sensing Hub subsystem.
                   * - No further events will be recieved on this session
                   * - The session remains in open state
                   * - Client may resend the sensor requests to resume event reception
                   */
    SERVICE_DOWN  /*!< Indicates unavailability of the Sensing Hub subsystem.
                   * - No further events will be recieved on this session
                   * - The session remains in closed state
                   * - The Client is expected to call open() again to attempt reconnection
                   */
  };


  /**
   * @brief This callback is invoked when a response is received for the registered sensor SUID.
   *
   *   Response is an acknowledgement that the client request was sent successfully by the
   *   underlying transport layer.
   *   @param[in] respValue        Response status associated with the request.
   *                               0        - Success
   *                               non-zero - Failure
   *   @param[in] clientConnectID  Connection identifier assigned by Sensing Hub for
   *                               this client session.
   */
  using respCallBack = std::function<void(const uint32_t respValue, uint64_t clientConnectID)>;


  /**
   * @brief This callback is invoked when an error occurs in Sensing Hub.
   *
   *   @param[in] errorValue    ISession error detected
   */
  using errorCallBack = std::function<void(error errorValue)>;


  /**
   * @brief This callback is invoked when an event is received for the registered sensor SUID
   *
   *   Event is the sensor data requested by the client.
   *   @param[in] sensorData              pointer to protocol-buffer-encoded data stream
   *   @param[in] sensorDataSize          size of the sensorData
   *   @param[in] sensorDataTimeStamp     timestamp at which sensorData is generated
   */
  using eventCallBack = std::function<void(const uint8_t *sensorData, size_t sensorDataSize, uint64_t sensorDataTimeStamp)>;


  /**
   * @brief Open the session and establish a connection to Sensing Hub.
   *
   * Initiates the client session created using getSession().
   * Sets up and establishes communication channel between the client and the Sensing Hub framework.
   *
   * This should be called exactly once per ISession instance before
   * any other operations such as setCallBacks() or sendRequest().
   *
   * @return
   *   -   0  Success (session is now open and ready for use).
   *   -  -1  Failure (session remains closed).
   */
  virtual int open() = 0;


  /**
   * @brief Closes the session for this instance and release associated resources
   *
   * Terminates the communication channel between the client and the
   * Sensing Hub framework and frees any resources held by this session.
   *
   * After close() is called:
   * - No further requests should be sent using this ISession instance.
   * - Registered callbacks will no longer receive events.
   *
   *  The client is expected to call close() once it is done with the
   *  session to avoid leaks and dangling resources.
   */
  virtual void close() = 0;


  /**
   * @brief Set the callbacks for specified sensor SUID.
   *        Client needs to call only once per given suid.
   *
   * Associates response, error, and event callbacks with the given SUID for this session.
   * These callbacks are invoked for messages received from Sensing Hub that correspond to the specified SUID.
   *
   * @param [in] suid      Unique SUID of the sensor for which callbacks are set.
   * @param [in] respCB    respCallBack pointer (may be nullptr).
   * @param [in] errorCB   errorCallBack pointer (may be nullptr)
   * @param [in] eventCB   eventCallBack pointer (may be nullptr).
   *
   * @note All parameters are mandatory.
   * - Incase SUID is already registered, callbacks are updated with new ones.
   * - The client may pass nullptr, incase any callback function is not defined / required.
   * - For an already registered SUID, passing nullptr for all callbacks
   *   effectively unregisters that SUID.
   * - For a new/unregistered SUID, passing nullptr for all callback
   *   functions is considered as an error.
   *
   * @return
   *   - 0  Success.
   *   - -1 Failure, if all callback functions are nullptr for an unregistered SUID.
   */
  virtual int setCallBacks(suid suid, respCallBack respCB, errorCallBack errorCB, eventCallBack eventCB) = 0;


  /**
   * @brief Send an asynchronous request for a given sensor.
   *
   * Sends a protocol-buffer-encoded request message to the Sensing Hub for the
   * specified SUID.
   *
   * @param [in] suid    Unique SUID of the target sensor.
   * @param [in] message Proto encoded request message, formulated with
   *                     client API for the given sensor.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure
   *        - Session is not open or already closed.
   *        - Encoded message size exceeds the allowed limit.
   *        - Underlying transport/channel error while sending.
   */
  virtual int sendRequest(suid suid, std::string message) = 0;


  /**
   * @brief Destructor for ISession.
   *
   * Clients are expected to delete the ISession instance once the use
   * case is complete to avoid memory leaks.
   */
  virtual ~ISession(){};
protected:
};

}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
