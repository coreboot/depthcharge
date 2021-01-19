/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __DIAG_HEALTH_INFO_H__
#define __DIAG_HEALTH_INFO_H__

#include "drivers/storage/info.h"

// Append the stringified health_info to string buf and return the pointer of
// the next available address of buf.
char *stringify_health_info(char *buf, const char *end, const HealthInfo *info);

// Append the health info of all devices to string buf and return the pointer of
// the next available address of buf.
char *dump_all_health_info(char *buf, const char *end);

#endif
