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

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/firmware_id.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

_Static_assert(VB2_VBSD_SIZE <= ARRAY_SIZE(((chromeos_acpi_t *)0)->vdat),
	       "VbSharedDataHeader does not fit in vdat");

int crossystem_setup(int firmware_type)
{
	struct vb2_context *ctx = vboot_get_context();
	chromeos_acpi_t *acpi_table =
		phys_to_virt(lib_sysinfo.acpi_gnvs) + GNVS_CHROMEOS_ACPI_OFFSET;
	int size;

	/* Write VbSharedDataHeader to ACPI vdat for userspace access. */
	vb2api_export_vbsd(ctx, acpi_table->vdat);

	acpi_table->boot_reason = BOOT_REASON_OTHER;

	int main_fw = 0;
	int fw_index;
	const char *fwid;
	int fwid_size;

	fw_index = get_active_fw_index();
	fwid = get_active_fw_id();
	fwid_size = get_active_fw_size();
	if (fwid == NULL || fwid_size == 0) {
		printf("Unrecognized active firmware index.\n");
		return 1;
	}

	const struct {
		int vbsd_fw_index;
		int main_fw_index;
	} main_fw_arr[] = {
		{ VBSD_RW_A, BINF_RW_A },
		{ VBSD_RW_B, BINF_RW_B },
		{ VBSD_RECOVERY, BINF_RECOVERY },
	};

	int i;
	for (i = 0; i < ARRAY_SIZE(main_fw_arr); i++) {
		if (fw_index == main_fw_arr[i].vbsd_fw_index) {
			main_fw = main_fw_arr[i].main_fw_index;
			break;
		}
	}
	assert(i < ARRAY_SIZE(main_fw_arr));

	acpi_table->main_fw = main_fw;

	// Use the value set by coreboot if we don't want to change it.
	if (CONFIG_EC_VBOOT_SUPPORT) {
		int in_rw = 0;

		if (vb2ex_ec_running_rw(&in_rw)) {
			printf("Couldn't tell if the EC firmware is RW.\n");
			return 1;
		}
		acpi_table->ec_fw = in_rw ? ACTIVE_ECFW_RW : ACTIVE_ECFW_RO;
	}

	uint16_t chsw = 0;
	if (ctx->flags & VB2_CONTEXT_FORCE_RECOVERY_MODE)
		chsw |= CHSW_RECOVERY_X86;
	if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
		chsw |= CHSW_DEVELOPER_SWITCH;
	acpi_table->chsw = chsw;

	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = MIN(sizeof(hwid), sizeof(acpi_table->hwid));
	if (!vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size))
		memcpy(acpi_table->hwid, hwid, hwid_size);

	size = MIN(fwid_size, sizeof(acpi_table->fwid));
	memcpy(acpi_table->fwid, fwid, size);

	size = get_ro_fw_size();

	if (size) {
		size = MIN(size, sizeof(acpi_table->frid));
		memcpy(acpi_table->frid, get_ro_fw_id(), size);
	}

	if (firmware_type != FIRMWARE_TYPE_AUTO_DETECT)
		acpi_table->main_fw_type = firmware_type;
	else if (main_fw == BINF_RECOVERY)
		acpi_table->main_fw_type = FIRMWARE_TYPE_RECOVERY;
	else if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
		acpi_table->main_fw_type = FIRMWARE_TYPE_DEVELOPER;
	else
		acpi_table->main_fw_type = FIRMWARE_TYPE_NORMAL;

	acpi_table->recovery_reason = vb2api_get_recovery_reason(ctx);

	acpi_table->fmap_base = (uintptr_t)fmap_base();

	size = MIN(fwid_size, strnlen(fwid, ACPI_FWID_SIZE));
	uint8_t *dest = (uint8_t *)(uintptr_t)acpi_table->fwid_ptr;
	memcpy(dest, fwid, size);
	dest[size] = 0;

	return 0;
}

