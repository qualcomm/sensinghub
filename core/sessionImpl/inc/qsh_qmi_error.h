/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include <map>
#include <stdexcept>
using namespace std;

/* exception type defining errors related to qmi API calls */
struct qsh_qmi_error : public runtime_error
{
  qsh_qmi_error(int error_code, const std::string& what = "") :
        runtime_error(what + ": " + error_code_to_string(error_code)) { }

    static string error_code_to_string(int code)
    {
        static const map<int, string> error_map = {
            { QMI_NO_ERR, "qmi no error" },
            { QMI_INTERNAL_ERR, "qmi internal error" },
            { QMI_TIMEOUT_ERR, "qmi timeout" },
            { QMI_XPORT_BUSY_ERR, "qmi transport busy" },
            { QMI_SERVICE_ERR, "qmi service error" },
        };
        string msg;
        try {
            msg = error_map.at(code);
        } catch (out_of_range& e) {
            msg = "qmi error";
        }
        return msg + " (" + to_string(code) + ")";
    }
};
