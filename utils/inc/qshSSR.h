/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

/**
 *  @brief Trigger SSR (Subsystem Restart) for the sensor subsystem.
 *  This function attempts to trigger an SSR by writing to the appropriate
 *  sysfs node.
 *  @return 0 on success, -1 on failure
 */
int triggerSSR();
