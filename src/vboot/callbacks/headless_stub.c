// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
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

uint32_t vb2ex_prepare_log_screen(enum vb2_screen screen, uint32_t locale_id,
				  const char *str)
{
	/* TODO(b/151200757): Support headless devices */
	return 0;
}

uint32_t vb2ex_get_locale_count(void)
{
	/* TODO(b/151200757): Support headless devices */
	return 0;
}

const char *vb2ex_get_debug_info(struct vb2_context *ctx)
{
	/* TODO(b/151200757): Support headless devices */
	return "";
}

const char *vb2ex_get_firmware_log(int reset)
{
	/* TODO(b/151200757): Support headless devices */
	return "";
}

vb2_error_t vb2ex_diag_get_storage_health(const char **out)
{
	/* TODO(b/151200757): Support headless devices */
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_diag_get_storage_test_log(const char **out)
{
	/* TODO(b/151200757): Support headless devices */
	return VB2_ERROR_EX_UNIMPLEMENTED;
}

vb2_error_t vb2ex_diag_memory_quick_test(int reset, const char **out)
{
	/* TODO(b/151200757): Support headless devices */
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_diag_memory_full_test(int reset, const char **out)
{
	/* TODO(b/151200757): Support headless devices */
	return VB2_SUCCESS;
}
