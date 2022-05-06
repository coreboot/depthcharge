// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC.
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

#include <libpayload.h>
#include <vb2_api.h>

#include "diag/health_info.h"
#include "diag/memory.h"
#include "diag/storage_test.h"
#include "vboot/ui.h"

#define DEFAULT_DIAGNOSTIC_OUTPUT_SIZE (64 * KiB)

vb2_error_t vb2ex_diag_get_storage_health(const char **out)
{
	static char *buf;
	if (!buf)
		buf = malloc(DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
	*out = buf;
	if (!buf)
		return VB2_ERROR_UI_MEMORY_ALLOC;

	dump_all_health_info(buf, buf + DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_diag_get_storage_test_log(const char **out)
{
	static char *buf;
	if (!buf)
		buf = malloc(DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
	*out = buf;
	if (!buf)
		return VB2_ERROR_UI_MEMORY_ALLOC;

	return diag_dump_storage_test_log(buf,
					  buf + DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
}

vb2_error_t vb2ex_diag_memory_quick_test(int reset, const char **out)
{
	*out = NULL;
	if (reset)
		VB2_TRY(memory_test_init(MEMORY_TEST_MODE_QUICK));
	return memory_test_run(out);
}

vb2_error_t vb2ex_diag_memory_full_test(int reset, const char **out)
{
	*out = NULL;
	if (reset)
		VB2_TRY(memory_test_init(MEMORY_TEST_MODE_FULL));
	return memory_test_run(out);
}
