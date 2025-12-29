#pragma once
/** ============================================================================
 * @file
 *
 * @brief SessionFactory provides a simple and flexible way to create ISession
 *        instances for communicating with Sensing Hub.
 *
 * SessionFactory is a lightweight runtime interface that encapsulates the underlying
 * implementation details and exposes a uniform, OS-agnostic way for clients to:
 *   - Discover supported Sensing Hub IDs.
 *   - Create ISession instances bound to a specific hub.
 *
 * Typical usage:
 *   - Create a sessionFactory instance.
 *   - Optionally call getSensingHubIds() to enumerate supported hubs.
 *   - Call getSession(hub_id) to obtain an ISession for a hub.
 *   - Call ISession::open() and use the session to send requests.
 *   - When done, close the session and destroy the factory.
 *
 * @copyright Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * ===========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include <vector>
#include "ISession.h"

namespace com {
namespace quic {
namespace sensinghub {
namespace session {
namespace V1_0 {

/*==============================================================================
  Type Definitions
  ============================================================================*/

/**
 * @class sessionFactory
 * @brief Runtime factory for creating ISession instances.
 *
 * SessionFactory abstracts the runtime loading of the Sensing Hub client
 * implementation. It provides simple APIs to:
 *   - Query the IDs of available Sensing Hubs on the system.
 *   - Create ISession objects bound to a particular hub ID.
 *
 * Client applications must not instantiate ISession directly. Instead,
 * they should always use sessionFactory::getSession() to obtain a
 * platform-backed ISession instance.
 */
class sessionFactory {
public:
  /**
   * @brief Construct a sessionFactory instance.
   *
   * Creates a factory object that can be used to obtain ISession
   * instances and query supported Sensing Hub IDs. Multiple
   * sessionFactory instances are allowed; they may share the same
   * underlying runtime resources.
   */
  sessionFactory(){};


  /**
   * @brief Destroy the sessionFactory instance.
   *
   * Once the client no longer needs to create sessions or query hub
   * IDs, it should destroy the sessionFactory instance to release any
   * associated resources.
   *
   * @note Destroying the factory does not automatically destroy
   * ISession instances created from it. Clients remain responsible
   * for cleaning up their session objects.
   */
  ~sessionFactory(){};


  /**
   * @brief Creates ISession instance for the specified sensing-hub ID
   *
   * This API loads the ISession implementation at runtime and
   * constructs a ISession object that can be used to communicate with
   * the Sensing Hub.
   *
   * @note The returned ISession instance is initially not in open state.
   * The client must call open() before sending any requests.
   *
   * @param [in] hub_id  Hub ID of the desired sensing-hub
   *                     Default value = -1.
   * @return
   *   - Pointer to ISession object  Success.
   *   - Nullptr                     Failure.
   *
   */
  ISession* getSession(int hub_id = -1);


   /**
   * @brief Retrieve the IDs of supported Sensing Hubs.
   *
   * Queries the underlying implementation for all available Sensing Hub
   * IDs on the system. These IDs can be used as input to getSession().
   *
   * @return
   *   A std::vector<int> containing the hub IDs supported by the
   *   current platform. An empty vector indicates that no Sensing Hubs
   *   were discovered or that discovery is not supported.
   */
  std::vector<int> getSensingHubIds();

private:
  /**
   * @brief Function pointer type for the ISession creation symbol.
   *
   */
  typedef ISession* (*getSession_t)(int);


  /**
   * @brief Function pointer type for the Sensing Hub ID retrieval symbol.
   *
   */
  typedef void* (*getSensingHubIds_t)();


  /**
   * @brief Initialize the underlying Sensing Hub client implementation.
   *
   * This function is responsible for loading and resolving the runtime
   * symbols required to:
   *   - Create ISession instances.
   *   - Retrieve Sensing Hub IDs.
   *
   * @return
   *   -  0 on success (all required symbols were resolved).
   *   - -1 on failure (initialization or symbol resolution failed).
   */
  int loadSymbol();

  static bool mSymbolLoaded;  /*!< Indicates whether the required runtime symbols are available.*/
  static getSession_t mGetSessionSymbol;  /*!< Function pointer used to create ISession instances. */
  static getSensingHubIds_t mGetSensingHubIdsSymbol;  /*!< Function pointer used to retrieve Sensing Hub IDs. */
};

}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
