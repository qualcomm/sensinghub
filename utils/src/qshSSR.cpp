/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <string>
#include <cstring>
#include "qshLog.h"
#include "qshSSR.h"
#ifdef __ANDROID_API__
#include <cutils/properties.h>
#endif
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include "qshTarget.h"
using namespace std;

static const char SENSORS_HAL_SSR_SUPPORT[] = "persist.vendor.sensors.debug.hal_trigger_ssr";
static const char SENSORS_HAL_CRASH_SUPPORT[] = "persist.vendor.sensors.debug.hal_trigger_crash";

bool isSSRTrigerSupport()
{
#if __ANDROID_API__
    char ssrProp[PROP_VALUE_MAX];
    property_get(SENSORS_HAL_SSR_SUPPORT, ssrProp, "false");
    sns_logd("ssrProp: %s",ssrProp);
    if (!strncmp("true", ssrProp, 4)) {
        sns_logi("support_ssr_trigger: %s",ssrProp);
        return true;
    }
#endif
    return false;
}

bool isCrashTrigerSupport()
{
#if __ANDROID_API__
    char crashProp[PROP_VALUE_MAX];
    property_get(SENSORS_HAL_CRASH_SUPPORT, crashProp, "false");
    sns_logd("crashProp: %s",crashProp);
    if (!strncmp("true", crashProp, 4)) {
        sns_logi("support_crash_trigger: %s",crashProp);
        return true;
    }
#endif
    return false;
}

/* utility function to trigger ssr*/
/*  all deamons/system_app do not have permissions to open
     sys/kernel/boot_slpi/ssr , right now only hal sensors can do it*/
int triggerSSR() {
    if(isSSRTrigerSupport() == true) {
        int fd = open(qshTarget::getSSRPath().c_str(), O_WRONLY);
        if (fd<0) {
            sns_logd("failed to open sys/kernel/boot_*/ssr");
            return -1;
        }
        int ret = -1;
        if (write(fd, "1", 1) > 0) {
           sns_logi("ssr triggered successfully");
           ret = 0;
        } else {
            sns_loge("failed to write sys/kernel/boot_slpi/ssr");
            perror("Error: ");
        }
        close(fd);
        if (ret == 0) {
            /*allow atleast some time before connecting after ssr*/
            sleep(2);
        }
        return ret;
    } else {
        if (isCrashTrigerSupport() == true) {
            int fd = open("/proc/sysrq-trigger", O_WRONLY | O_CLOEXEC);
            if (fd < 0) {
                sns_loge("failed to open sysrq-trigger");
                return -1;
            }
            if (write(fd, "c", 1) > 0) {
                sns_loge("crash triggered successfully");
                close(fd);
                return 0;
            } else {
                sns_loge("failed to write sysrq-trigger");
                close(fd);
                perror("Error: ");
                return -1;
            }
        } else {
            sns_logd("trigger_ssr or trigger_crash not supported");
            return -1;
        }
    }
}
