/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#define MAX_GLINK_ERROR_STR_LEN 30

struct glink_error : public std::runtime_error
{
    glink_error(int error_code, const std::string& what = "") :
        std::runtime_error(error_code_to_string(error_code, what)) { }

    static std::string error_code_to_string(int code, std::string what)
    {
      char errorno_msg[MAX_GLINK_ERROR_STR_LEN];
      char *p;
      p = strerror_r(code, errorno_msg, MAX_GLINK_ERROR_STR_LEN);
      std::string msg(p);
      return "qsh_glink_ERROR " + what + " : " + msg + " (" + std::to_string(code) + ")";
    }
};

