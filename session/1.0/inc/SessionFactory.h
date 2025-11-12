#pragma once
/** ============================================================================
 * @file
 *
 * @brief SessionFactory provides simple and flexible way for creating ISession
 * instances.
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
 * @brief SessionFactory is a runtime interface that provides simple and flexible
 * way for creating instances of ISession, functioning as factory class.
 * Users can create an ISession only through sessionFactory instance.
 *
 */
class sessionFactory {
public:
  /**
   * @brief Constructor for sessionFactory, creates singleton object
   *
   */
  sessionFactory(){};

  /**
   * @brief Destructor for sessionFactory.
   * Once the usecase is done, to avoid the memory leaks,
   * clients are expected to delete the sessionFactory instance.
   *
   */
  ~sessionFactory(){};

  /**
   * @brief Creates ISession instance for the specified sensing-hub ID
   * @param [in] hub_id  Hub ID of the desired sensing-hub
   *                     Default value = -1.
   * @return
   *   - Pointer to ISession object  Success.
   *   - Nullptr                     Failure.
   *
   */
  ISession* getSession(int hub_id = -1);

  /**
   * @brief Retrieve Hub IDs of the supported Sensing Hubs.
   * @return vector containing all supported Hub IDs.
   *
   */
  std::vector<int> getSensingHubIds();

private:
  /**
   * @brief Pointer to ISession object.
   *
   */
  typedef ISession* (*getSession_t)(int);

  /**
   * @brief Pointer to vector containing Sensing Hub IDs.
   *
   */
  typedef void* (*getSensingHubIds_t)();

  /**
   * @brief Dynamically loads the proprietary library.
   *
   * @return
   *    -  0  Success.
   *    - -1  Failure.
   */
  int loadSymbol();

  static bool mSymbolLoaded;  /*!< true if loadSymbol() returns 0, else false*/
  static getSession_t mGetSessionSymbol;  /*!< stores pointer to ISession */
  static getSensingHubIds_t mGetSensingHubIdsSymbol;  /*!< stores pointer to vector containing Hub IDs */
};

}  // namespace V1_0
}  // namespace session
}  // namespace sensinghub
}  // namespace quic
}  // namespace com
