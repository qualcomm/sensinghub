/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#define LOG_TAG "sensors.qti"

#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <remote.h>
#include <AEEStdErr.h>
#include <dirent.h>
#include <signal.h>
#ifdef __ANDROID_API__
#include <utils/Log.h>
#include <stdlib.h>
#endif
#ifndef __ANDROID_API__
#include <sys/types.h>
#endif

#ifndef ADSP_LIBHIDL_NAME
#ifdef SNS_VERSIONED_LIB_ENABLED
#define ADSP_LIBHIDL_NAME "libhidlbase.so.1"
#else
#define ADSP_LIBHIDL_NAME "libhidlbase.so"
#endif
#endif

#ifdef SNS_VERSIONED_LIB_ENABLED
#define LIB_ADSP_DEFAULT_LISTENER "libadsp_default_listener.so.1"
#else
#define LIB_ADSP_DEFAULT_LISTENER "libadsp_default_listener.so"
#endif

#define FASTRPC_NODE_PATH         "/sys/kernel/fastrpc"
#define RPC_REMOTE_HADLE_CONTROL  "remote_handle_control"
#define RPC_REMOTE_SYSTEM_REQUEST "remote_system_request"
#define RPC_ADSP_DEFAULT_LISTENER_START  "adsp_default_listener_start"

typedef int (*adsp_default_listener_start_t)(int argc, char *argv[]);
typedef int (*remote_handle_control_t)(uint32_t req, void* data, uint32_t len);
#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
typedef int (*remote_system_request_t)(system_req_payload *req);
#endif

int requestFastRPCWakelock(void *adsphandler) {
  int nErr = 0;
  remote_handle_control_t handle_control;
  struct remote_rpc_control_wakelock data;

  data.enable = 1;

  if (NULL != (handle_control = (remote_handle_control_t)dlsym(adsphandler, RPC_REMOTE_HADLE_CONTROL))) {
    nErr = handle_control(DSPRPC_CONTROL_WAKELOCK, (void*)&data, sizeof(data));
    if (nErr == AEE_EUNSUPPORTEDAPI) {
#ifdef __ANDROID_API__
      ALOGE("fastrpc wakelock request is not supported");
#endif
      /* this feature may not be supported by all targets
         treat this case as normal since we still can call listener_start */
      nErr = AEE_SUCCESS;
    } else if (nErr) {
#ifdef __ANDROID_API__
      ALOGE("failed to enable fastrpc wake-lock control, %x", nErr);
#endif
    }
  } else {
#ifdef __ANDROID_API__
    ALOGE("unable to find remote_handle_control, %s", dlerror());
#endif
    /* there should be no case where remote_handle_control doesn't exist */
    nErr = AEE_EFAILED;
  }
  return nErr;
}

#define MAX_DOMAIN_RETRIES 200  /* 200 * 25ms = 5 seconds */

#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
static inline bool isDynamicDomainSystem() {
  DIR *dir = opendir(FASTRPC_NODE_PATH);
  if(dir) {
    closedir(dir);
    return true;
  }
  return false;
}

int isDomainAvailable(void){
  void* adsphandler = NULL;
  remote_system_request_t remote_system_request = NULL;
  int retry_count = 0;
  int domain_retry_count = 0;
  if(NULL != (adsphandler = dlopen(LIB_ADSP_DEFAULT_LISTENER, RTLD_NOW))){
    if(NULL != (remote_system_request =
      (remote_system_request_t)dlsym(adsphandler, RPC_REMOTE_SYSTEM_REQUEST)) && isDynamicDomainSystem()) {
      system_req_payload req = {};
      fastrpc_domain *domain = NULL;
      fastrpc_domains_info *sys = &req.sys;
      int nErr = 0;
      int num_domains = 0;

      req.id = FASTRPC_GET_DOMAINS;
  query_num_domains:
      if (retry_count++ > MAX_DOMAIN_RETRIES) {
#ifdef __ANDROID_API__
        ALOGE("isDomainAvailable: max retries exceeded waiting for domains");
#endif
        if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
          ALOGE("dlclose failed");
#endif
        }
        return -1;
      }
      nErr = remote_system_request(&req);
      if (nErr == AEE_EUNSUPPORTED) {
        /*
        * Dynamic domain discovery not supported, dlclose the current adsphandler.
        * and go to default_listener_start directly.
        */
#ifdef __ANDROID_API__
        ALOGE("Failed to get domains num");
#endif
        if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
          ALOGE("dlclose failed");
#endif
        }
        return 0;
      }
      num_domains = sys->num_domains;
      if (!num_domains) {
#ifdef __ANDROID_API__
        ALOGE("No domains available to attach. Retry query after 25 ms...");
#endif
        usleep(25000);
        goto query_num_domains;
      }
      /* Allocate memory for domain info structure */
      sys->domains = nullptr;
      sys->domains = (fastrpc_domain*)calloc(num_domains, sizeof(fastrpc_domain));
      if(nullptr == sys->domains) {
#ifdef __ANDROID_API__
        ALOGE("Failed to allocate memory for domains");
#endif
        if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
          ALOGE("dlclose failed");
#endif
        }
        return -1;
      }
      sys->max_domains = num_domains;
      sys->flags = DOMAINS_LIST_FLAGS_SET_TYPE(sys->flags, FASTRPC_LPASS);
  query_domains:
      nErr = remote_system_request(&req);
      if (nErr) {
        /*
        * If failed to get domain info, free the previous allocated domain array
        * and requery the num domains available and try to get the domain info again
        */
#ifdef __ANDROID_API__
        ALOGE("Failed to get domains info");
#endif
        sys->num_domains = 0;
        free(sys->domains);
        domain_retry_count = 0;
        goto query_num_domains;
      }
      domain = sys->domains;
      if (domain->type == FASTRPC_LPASS && domain->status) {
        free(sys->domains);
        if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
          ALOGE("dlclose failed");
#endif
        }
        return 0;
      } else {
        if (domain_retry_count++ > MAX_DOMAIN_RETRIES) {
#ifdef __ANDROID_API__
          ALOGE("isDomainAvailable: max retries exceeded waiting for domain to come up");
#endif
          free(sys->domains);
          if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
            ALOGE("dlclose failed");
#endif
          }
          return -1;
        }
        memset(sys->domains, 0, sizeof(fastrpc_domain) * num_domains);
#ifdef __ANDROID_API__
        ALOGE("Domain type %d is not up, retry query after 25ms...", FASTRPC_LPASS);
#endif
        usleep(25000);
        goto query_domains;
      }
    } else {
#ifdef __ANDROID_API__
      ALOGE("no remote_system_request symbol, error %s", dlerror());
#endif
      if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
        ALOGE("dlclose failed");
#endif
      }
    }
  }else{
#ifdef __ANDROID_API__
    ALOGE("fail to open %s due to %s", LIB_ADSP_DEFAULT_LISTENER, dlerror());
#endif
  }
  return -1;
}
#endif //SNS_FRPC_DYNAMIC_PD_SUPPORTED

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig) {
  (void)sig;
  g_running = 0;
}

int main(int argc, char *argv[]) {

  int nErr = 0;
  void *adsphandler = NULL, *libhidlbaseHandler = NULL;
  adsp_default_listener_start_t listener_start;

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
  isDomainAvailable();
#endif

#ifdef __ANDROID_API__
  libhidlbaseHandler = dlopen(ADSP_LIBHIDL_NAME, RTLD_NOW);
  if (libhidlbaseHandler == NULL)
  {
    ALOGE("libhidlbase dlopen failed");
    return 0;
  }
#endif

#ifdef __ANDROID_API__
  if (argc > 1) {
    ALOGI("sensors.qti daemon starting for %s", argv[1]);
  } else {
    ALOGI("sensors.qti daemon starting");
  }
#endif
  while (g_running) {
    if(NULL != (adsphandler = dlopen(LIB_ADSP_DEFAULT_LISTENER, RTLD_NOW))) {
      if(NULL != (listener_start =
        (adsp_default_listener_start_t)dlsym(adsphandler, RPC_ADSP_DEFAULT_LISTENER_START))) {
        nErr = requestFastRPCWakelock(adsphandler);
        if(nErr){
#ifdef __ANDROID_API__
          ALOGE("request_fastrpc_wakelock failed with, %x", nErr);
#endif
        }
        nErr = listener_start(argc, argv);
#ifdef __ANDROID_API__
        ALOGE("listener_start exit with %x", nErr);
#endif
      } else {
#ifdef __ANDROID_API__
        ALOGE("failed to find %s symbol: %s", RPC_ADSP_DEFAULT_LISTENER_START, dlerror());
#endif
      }
      if(0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
        ALOGE("dlclose failed");
#endif
      }
    }else {
#ifdef __ANDROID_API__
      ALOGE("fail to open %s due to %s", LIB_ADSP_DEFAULT_LISTENER, dlerror());
#endif
    }
    usleep(25000);
  }
  if(NULL != libhidlbaseHandler && 0 != dlclose(libhidlbaseHandler)) {
#ifdef __ANDROID_API__
    ALOGE("libhidlbase dlclose failed");
#endif
  }
#ifdef __ANDROID_API__
  ALOGI("sensors.qti daemon exiting %x", nErr);
#endif

  return nErr;
}