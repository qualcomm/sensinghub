/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "SessionFactory.h"
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
sessionFactory::getSensingHubIds_t sessionFactory::mGetSensingHubIdsSymbol = nullptr;

ISession* sessionFactory::getSession(int hub_id) {
  int status = 0;
  if(true != mSymbolLoaded) {
    status = loadSymbol();
  }
  if(0 == status) {
    return mGetSessionSymbol(hub_id);
  } else {
    return nullptr;
  }
}

vector<int> sessionFactory::getSensingHubIds() {
  vector<int> supportedHubIds;
  int status = 0;
  if(true != mSymbolLoaded) {
    status = loadSymbol();
  }
  if(0 == status && nullptr != mGetSensingHubIdsSymbol) {
    vector<int>* supportedHubIdsPtr =(vector<int>*)mGetSensingHubIdsSymbol();
    for(int loop_idx = 0; loop_idx < supportedHubIdsPtr->size(); loop_idx++) {
    supportedHubIds.push_back((*supportedHubIdsPtr)[loop_idx]);
    }
  }
  return supportedHubIds;
}

int sessionFactory::loadSymbol() {
  void* libHandler = dlopen(SENSING_HUB_INTERFACE_LIB_NAME, RTLD_NOW);
  if(nullptr != libHandler) {
    mGetSessionSymbol = (getSession_t)dlsym(libHandler, "getSession");
    mGetSensingHubIdsSymbol = (getSensingHubIds_t)dlsym(libHandler, "getSensingHubIds");
    if(nullptr != mGetSessionSymbol && nullptr != mGetSensingHubIdsSymbol) {
      mSymbolLoaded = true;
      return 0;
    } else {
      printf("error in loading the symbols \n");
      return -1;
    }
  }
  printf("error in loading the lib \n");
  return -1;
}
