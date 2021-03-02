// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google LLC.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <vb2_api.h>

#include "diag/storage_test.h"

vb2_error_t vb2ex_diag_storage_test_control(enum vb2_diag_storage_test ops)
{
	enum BlockDevTestOpsType blockdev_ops;
	switch (ops) {
	case VB2_DIAG_STORAGE_TEST_STOP:
		blockdev_ops = BLOCKDEV_TEST_OPS_TYPE_STOP;
		break;
	case VB2_DIAG_STORAGE_TEST_SHORT:
		blockdev_ops = BLOCKDEV_TEST_OPS_TYPE_SHORT;
		break;
	case VB2_DIAG_STORAGE_TEST_EXTENDED:
		blockdev_ops = BLOCKDEV_TEST_OPS_TYPE_EXTENDED;
		break;
	default:
		return VB2_ERROR_EX_UNIMPLEMENTED;
	}
	return diag_storage_test_control(blockdev_ops);
};
