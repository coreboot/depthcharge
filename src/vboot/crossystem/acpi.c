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
#include <vboot_api.h>
#include <vboot_struct.h>

#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/firmware_id.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"

int crossystem_setup(int firmware_type)
{
	chromeos_acpi_t *acpi_table =
		lib_sysinfo.acpi_gnvs + GNVS_CHROMEOS_ACPI_OFFSET;
	VbSharedDataHeader *vdat = (VbSharedDataHeader *)&acpi_table->vdat;
	VbSharedDataHeader *vb_sd;
	int size, vb_sd_size;

	if (find_common_params((void**)&vb_sd, &vb_sd_size) != 0) {
		printf("Can't find common params.\n");
		return 1;
	}

	if (vb_sd->magic != VB_SHARED_DATA_MAGIC) {
		printf("Bad magic value in vboot shared data header.\n");
		return 1;
	}

	acpi_table->boot_reason = BOOT_REASON_OTHER;

	int main_fw = 0;
	const char *fwid;
	int fwid_size;
	int fw_index = vb_sd->firmware_index;

	fwid = get_fw_id(fw_index);

	if (fwid == NULL) {
		printf("Unrecognized firmware index %d.\n", fw_index);
		return 1;
	}

	fwid_size = get_fw_size(fw_index);

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
	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (vb2ex_ec_running_rw(&in_rw)) {
			printf("Couldn't tell if the EC firmware is RW.\n");
			return 1;
		}
		acpi_table->ec_fw = in_rw ? ACTIVE_ECFW_RW : ACTIVE_ECFW_RO;
	}

	uint16_t chsw = 0;
	if (vb_sd->flags & VBSD_BOOT_FIRMWARE_WP_ENABLED)
		chsw |= CHSW_FIRMWARE_WP;
	if (vb_sd->flags & VBSD_BOOT_REC_SWITCH_ON)
		chsw |= CHSW_RECOVERY_X86;
	if (vb_sd->flags & VBSD_BOOT_DEV_SWITCH_ON)
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
	else if (vb_sd->flags & VBSD_BOOT_DEV_SWITCH_ON)
		acpi_table->main_fw_type = FIRMWARE_TYPE_DEVELOPER;
	else
		acpi_table->main_fw_type = FIRMWARE_TYPE_NORMAL;

	acpi_table->recovery_reason = vb_sd->recovery_reason;

	acpi_table->fmap_base = (uintptr_t)fmap_base();

	size = MIN(fwid_size, strnlen(fwid, ACPI_FWID_SIZE));
	uint8_t *dest = (uint8_t *)(uintptr_t)acpi_table->fwid_ptr;
	memcpy(dest, fwid, size);
	dest[size] = 0;

	// Synchronize VbSharedDataHeader to acpi vdat.
	memcpy(vdat, vb_sd, vb_sd_size);

	return 0;
}

