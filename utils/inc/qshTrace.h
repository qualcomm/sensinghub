/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#define ATRACE_TAG ATRACE_TAG_ALWAYS
#include <cutils/trace.h>

static bool qsh_trace_enabled = false;

static inline void QSH_TRACE_ENABLE(bool enable)
{
    qsh_trace_enabled = enable;
}

#define QSH_TRACE_INT64(name, value) ({\
    if (CC_UNLIKELY(qsh_trace_enabled)) { \
        ATRACE_INT64(name, value); \
    } \
})

#define QSH_TRACE_BEGIN(name) ({\
    if (CC_UNLIKELY(qsh_trace_enabled)) { \
        ATRACE_BEGIN(name); \
    } \
})

#define QSH_TRACE_END() ({\
    if (CC_UNLIKELY(qsh_trace_enabled)) { \
        ATRACE_END(); \
    } \
})

