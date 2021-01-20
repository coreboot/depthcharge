/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Google Inc.
 */

#ifndef __DIAG_STORAGE_TEST_H__
#define __DIAG_STORAGE_TEST_H__

#include <vb2_api.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"

vb2_error_t diag_dump_storage_test_log(char *buf, const char *end);
vb2_error_t diag_storage_test_control(enum BlockDevTestOpsType ops);

#endif
