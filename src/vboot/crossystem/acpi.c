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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <gbb_header.h>
#include <libpayload.h>
#include <vboot_api.h>
#include <vboot_struct.h>

#include "config.h"
#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/firmware_id.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"
#include "vboot/util/vboot_handoff.h"

int crossystem_setup(void)
{
	chromeos_acpi_t *acpi_table = (chromeos_acpi_t *)lib_sysinfo.vdat_addr;
	VbSharedDataHeader *vboot_handoff_shared_data;
	VbSharedDataHeader *vdat = (VbSharedDataHeader *)&acpi_table->vdat;
	int size;

	if (vdat->magic != VB_SHARED_DATA_MAGIC) {
		printf("Bad magic value in vboot shared data header.\n");
		return 1;
	}

	acpi_table->boot_reason = BOOT_REASON_OTHER;

	int main_fw;
	const char *fwid;
	int fwid_size;
	int fw_index = vdat->firmware_index;

	fwid = get_fw_id(fw_index);

	if (fwid == NULL) {
		printf("Unrecognized firmware index %d.\n", fw_index);
		return 1;
	}

	fwid_size = get_fw_size(fw_index);

	const struct {
		int vdat_fw_index;
		int main_fw_index;
	} main_fw_arr[] = {
		{ VDAT_RW_A, BINF_RW_A },
		{ VDAT_RW_B, BINF_RW_B },
		{ VDAT_RECOVERY, BINF_RECOVERY },
	};

	int i;
	for (i = 0; i < ARRAY_SIZE(main_fw_arr); i++) {
		if (fw_index == main_fw_arr[i].vdat_fw_index) {
			main_fw = main_fw_arr[i].main_fw_index;
			break;
		}
	}
	assert(i < ARRAY_SIZE(main_fw_arr));

	acpi_table->main_fw = main_fw;

	// Use the value set by coreboot if we don't want to change it.
	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (VbExEcRunningRW(0, &in_rw)) {
			printf("Couldn't tell if the EC firmware is RW.\n");
			return 1;
		}
		acpi_table->ec_fw = in_rw ? ACTIVE_ECFW_RW : ACTIVE_ECFW_RO;
	}

	uint16_t chsw = 0;
	if (flag_fetch(FLAG_WPSW))
		chsw |= CHSW_FIRMWARE_WP_DIS;
	if (flag_fetch(FLAG_RECSW))
		chsw |= CHSW_RECOVERY_X86;
	if (flag_fetch(FLAG_DEVSW))
		chsw |= CHSW_DEVELOPER_SWITCH;
	acpi_table->chsw = chsw;

	GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
	if (memcmp(gbb->signature, GBB_SIGNATURE, GBB_SIGNATURE_SIZE)) {
		printf("Bad signature on GBB.\n");
		return 1;
	}
	char *hwid = (char *)((uintptr_t)cparams.gbb_data + gbb->hwid_offset);
	size = MIN(gbb->hwid_size, sizeof(acpi_table->hwid));
	memcpy(acpi_table->hwid, hwid, size);

	size = MIN(fwid_size, sizeof(acpi_table->fwid));
	memcpy(acpi_table->fwid, fwid, size);

	size = get_ro_fw_size();

	if (size) {
		size = MIN(size, sizeof(acpi_table->frid));
		memcpy(acpi_table->frid, get_ro_fw_id(), size);
	}

	if (main_fw == BINF_RECOVERY)
		acpi_table->main_fw_type = FIRMWARE_TYPE_RECOVERY;
	else if (vdat->flags & VBSD_BOOT_DEV_SWITCH_ON)
		acpi_table->main_fw_type = FIRMWARE_TYPE_DEVELOPER;
	else
		acpi_table->main_fw_type = FIRMWARE_TYPE_NORMAL;

	acpi_table->recovery_reason = vdat->recovery_reason;

	acpi_table->fmap_base = (uintptr_t)fmap_base();

	size = MIN(fwid_size, strnlen(fwid, ACPI_FWID_SIZE));
	uint8_t *dest = (uint8_t *)(uintptr_t)acpi_table->fwid_ptr;
	memcpy(dest, fwid, size);
	dest[size] = 0;

	// Synchronize the value in vboot_handoff back to acpi vdat.
	if (find_common_params((void**)(&vboot_handoff_shared_data), &size) == 0)
		memcpy(vdat, vboot_handoff_shared_data, size);
	else {
		printf("Can't find common params.\n");
		return 1;
	}

	return 0;
}

