#pragma once
/** ============================================================================
 * @file
 *
 * @brief  ISession offers simple and easy interface to the clients to
 * (1) Facilitate interaction with Qualcomm Sensing Hub
 * (2) Abstracts the underlying transport layer mechanism
 *
 * @copyright Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @brief ISession interface class provides methods to interact with QSH framework.
 *
 * @note Client needs to refer sessionFactory to create an instance of ISession
 * @example Clients can refer to the example code present at :
 *         ../../../examples/SessionClient/SessionClient.cpp
 */
class ISession {
public:
/**
 * @brief Error codes if any from ISession
 *
 */
  enum error
  {
    RESET /*!< indicates restart / resetting of QSH subsystem due to fatal events */
  };

/**
 * @brief Type alias for ISession respCallBack, errorCallBack and eventCallBack respectively.
 *
 */
  using respCallBack = std::function<void(const uint32_t respValue, uint64_t clientConnectID)>;
  using errorCallBack = std::function<void(error errorValue)>;
  using eventCallBack = std::function<void(const uint8_t *sensorData, size_t sensorDataSize, uint64_t sensorDataTimeStamp)>;

  /**
   * @brief Initiates the client session created using getSession().
   * Sets up and establishes communication between the client and the QSH framework.
   * The client should call this function only once per session.
   *
   * @return
   *   -   0  Success.
   *   -  -1  Failure.
   */
  virtual int open() = 0;

  /**
   * @brief Closes the session for this instance, terminates the communication
   * between the client and the QSH framework.
   * The client must call this function at the end of the session to release
   * resources and avoid memory leaks.
   *
   */
  virtual void close() = 0;

  /**
   * @brief Set the callbacks for the given suid.
   *        Client needs to call only once per given suid.
   *
   * @param [in] suid      SUID for which callbacks are to be registered.
   * @param [in] respCB    Response callback pointer.
   * @param [in] errorCB   Error callback pointer.
   * @param [in] eventCB   Event callback pointer.
   *
   * @note All parameters are mandatory.
   * - Incase SUID is already registered, callbacks are updated with new ones.
   * - The client may pass nullptr, incase any callback function is not defined / required.
   * - To unset the callbacks for already registered SUID,
   *   the client may pass nullptr for all the three callback functions.
   * - For any unregistered SUID, passing nullptr for all callback functions is considered as an error.
   *
   * @return
   *   - 0  Success.
   *   - -1 Failure, if all callback functions are nullptr for an unregistered SUID.
   */
  virtual int setCallBacks(suid suid, respCallBack respCB, errorCallBack errorCB, eventCallBack eventCB) = 0;

  /**
   * @brief Asynchronously sends a protocol buffer (proto)
   * encoded message to the QSH framework
   *
   * @param [in] suid    Unique SUID of the sensor for which request will be sent.
   * @param [in] message Proto encoded request message, formulated with
   *                     client API for the given sensor.
   *
   * @return
   *    - 0   Success
   *    - -1  Failure
   *         - if client tries to send request over a closed session
   *         - if encoded message size exceeds the permissible size
   *         - if sending message fails due to channel related issue
   */
  virtual int sendRequest(suid suid, std::string message) = 0;

  /**
   * @brief Destructor for ISession.
   * Once the usecase is done, to avoid the memory leaks,
   * clients are expected to delete the ISession instance.
   *
   */
  virtual ~ISession(){};
protected:
};

}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
