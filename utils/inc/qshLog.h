/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

namespace sensors_log
{

enum log_level
{
    SILENT = 0,
    ERROR = 1,
    INFO = 2,
    DEBUG = 3,
    VERBOSE = 4,
};

/**
 * @brief sets global log level for current process
 * @param level
 */
void set_level(log_level level);

/**
 * @brief get current log tag
 * @return const char*
 */
const char *get_tag();

/**
 * @brief set log tag
 * @param tag
 */
void set_tag(const char *tag);

/**
 * @brief get current log level value
 * @return log_level
 */
log_level get_level();

/**
 * @brief enable/disable logging over stderr
 * @param enable
 */
void set_stderr_logging(bool val);

/**
 * @brief get current status of stderr logging
 * @return bool
 */
bool get_stderr_logging();

}

void sns_log_message(int level, const char *fmt, ...);

#define sns_logv(fmt, args...) sns_log_message(sensors_log::VERBOSE, "%s:%d, " fmt, __FUNCTION__, __LINE__, ##args)
#define sns_loge(fmt, args...) sns_log_message(sensors_log::ERROR, "%s:%d, " fmt, __FUNCTION__, __LINE__, ##args)
#define sns_logd(fmt, args...) sns_log_message(sensors_log::DEBUG, "%s:%d, " fmt, __FUNCTION__, __LINE__, ##args)
#define sns_logi(fmt, args...) sns_log_message(sensors_log::INFO, "%s:%d, " fmt, __FUNCTION__, __LINE__, ##args)
