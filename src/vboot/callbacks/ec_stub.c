/*
 * Copyright 2012 Google Inc.
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
#include <vboot_api.h>
#include <vb2_api.h>

static void no_ec_soft_sync(void) __attribute__((noreturn));
static void no_ec_soft_sync(void)
{
	printf("EC software sync not supported.\n");
	halt();
}

int vb2ex_ec_trusted(void)
{
	printf("The EC which doesn't exist isn't untrusted.\n");
	return 1;
}

vb2_error_t vb2ex_ec_running_rw(int *in_rw)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_jump_to_rw(void)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_disable_jump(void)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_hash_image(enum vb2_firmware_selection select,
				const uint8_t **hash, int *hash_size)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_get_expected_image_hash(enum vb2_firmware_selection select,
					     const uint8_t **hash,
					     int *hash_size)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_update_image(enum vb2_firmware_selection select)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_protect(enum vb2_firmware_selection select)
{
	no_ec_soft_sync();
}

vb2_error_t vb2ex_ec_vboot_done(struct vb2_context *ctx)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExCheckAuxfw(enum vb2_auxfw_update_severity *severity)
{
	*severity = VB2_AUXFW_NO_UPDATE;
	return VB2_SUCCESS;
}

vb2_error_t VbExUpdateAuxfw(void)
{
	return VB2_SUCCESS;
}
