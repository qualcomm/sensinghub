/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SessionFactory.h"
#include "ISession.h"
#include "SessionConfig.h"
#include <dlfcn.h>

using namespace std;
using namespace com::quic::sensinghub::session::V1_0 ;
#ifdef SNS_VERSIONED_LIB_ENABLED
#define SENSING_HUB_INTERFACE_LIB_NAME "libQshSession.so.1"
#else
#define SENSING_HUB_INTERFACE_LIB_NAME "libQshSession.so"
#endif

bool sessionFactory::mSymbolLoaded = false;
sessionFactory::getSession_t sessionFactory::mGetSessionSymbol = nullptr;
sessionFactory::getSessionFromConfig_t sessionFactory::mgetSessionFromConfigSymbol = nullptr;
sessionFactory::getSensingHubIds_t sessionFactory::mGetSensingHubIdsSymbol = nullptr;

ISession* sessionFactory::getSession() {
  return getSession(-1);
}

ISession* sessionFactory::getSession(int hub_id) {
  loadSymbol();
  ISession *session = nullptr;
  if (mSymbolLoaded && mGetSessionSymbol) {
    session = mGetSessionSymbol(hub_id);
  }
  return session;
}

ISession *sessionFactory::getSession(int hub_id, const SessionConfig *config) {
  loadSymbol();
  ISession *session = nullptr;
  string encodedConfig;
  if (config) {
    encodedConfig = config->encode(hub_id);
  }
  if (mSymbolLoaded && mgetSessionFromConfigSymbol) {
    session = mgetSessionFromConfigSymbol(encodedConfig.data(),
                                          (int)encodedConfig.size());
  }
  return session;
}

vector<int> sessionFactory::getSensingHubIds() {
  vector<int> supportedHubIds;
  loadSymbol();
  if (mSymbolLoaded && mGetSensingHubIdsSymbol) {
    vector<int>* supportedHubIdsPtr = (vector<int>*)mGetSensingHubIdsSymbol();
    for (int loop_idx = 0; loop_idx < (int)supportedHubIdsPtr->size(); loop_idx++) {
      supportedHubIds.push_back((*supportedHubIdsPtr)[loop_idx]);
    }
    delete supportedHubIdsPtr;
  }
  return supportedHubIds;
}

SessionConfig* sessionFactory::getSessionConfig() {
  return new SessionConfig();
}

void sessionFactory::setDirectChannelRequest(SessionConfig *config,
                                             const char *encodedRequest,
                                             int requestLen) {
  if (config) {
    config->setDirectChannelRequest(encodedRequest, requestLen);
  }
}

int sessionFactory::loadSymbol() {
  if (mSymbolLoaded) {
    return 0;
  }
  void *libHandler = dlopen(SENSING_HUB_INTERFACE_LIB_NAME, RTLD_NOW);
  if (nullptr != libHandler) {
    mGetSessionSymbol = (getSession_t)dlsym(libHandler, "getSession");
    if (!mGetSessionSymbol) {
      printf("error in loading getSession symbol: %s\n", dlerror());
    }
    mgetSessionFromConfigSymbol =
        (getSessionFromConfig_t)dlsym(libHandler, "getSessionFromConfig");
    if (!mgetSessionFromConfigSymbol) {
      printf("error in loading getSessionFromConfig symbol: %s\n", dlerror());
    }
    mGetSensingHubIdsSymbol =
        (getSensingHubIds_t)dlsym(libHandler, "getSensingHubIds");
    if (!mGetSensingHubIdsSymbol) {
      printf("error in loading getSensingHubIds symbol %s\n", dlerror());
    }
    mSymbolLoaded = (mGetSessionSymbol || mGetSensingHubIdsSymbol ||
                     mgetSessionFromConfigSymbol);
  } else {
    printf("error in loading the lib \n");
    return -1;
  }
  return 0;
}
