/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Google Inc.
 */

#ifndef __DIAG_STORAGE_TEST_H__
#define __DIAG_STORAGE_TEST_H__

#include "diag/common.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"

/*
 * @return DIAG_TEST_SUCCESS on success, or other DiagTestResult enums on
 * failure.
 */
DiagTestResult diag_dump_storage_test_log(char *buf, const char *end);
DiagTestResult diag_storage_test_control(enum BlockDevTestOpsType ops);

#endif
