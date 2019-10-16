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

int VbExTrustEC(int devidx)
{
	printf("The EC which doesn't exist isn't untrusted.\n");
	return 1;
}

vb2_error_t VbExEcRunningRW(int devidx, int *in_rw)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcJumpToRW(int devidx)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcDisableJump(int devidx)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcHashImage(int devidx, enum VbSelectFirmware_t select,
			    const uint8_t **hash, int *hash_size)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcGetExpectedImage(int devidx, enum VbSelectFirmware_t select,
				   const uint8_t **image, int *image_size)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcGetExpectedImageHash(int devidx,
				       enum VbSelectFirmware_t select,
				       const uint8_t **hash, int *hash_size)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcUpdateImage(int devidx, enum VbSelectFirmware_t select,
			      const uint8_t *image, int image_size)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcProtect(int devidx, enum VbSelectFirmware_t select)
{
	no_ec_soft_sync();
}

vb2_error_t VbExEcVbootDone(int in_recovery)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExEcBatteryCutOff(void) {
	printf("EC battery cut-off not supported, ignored.\n");
	return VB2_SUCCESS;
}

vb2_error_t VbExCheckAuxFw(VbAuxFwUpdateSeverity_t *severity)
{
	*severity = VB_AUX_FW_NO_UPDATE;
	return VB2_SUCCESS;
}

vb2_error_t VbExUpdateAuxFw(void)
{
	return VB2_SUCCESS;
}
