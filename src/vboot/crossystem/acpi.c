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

#include <gbb_header.h>
#include <libpayload.h>
#include <vboot_api.h>
#include <vboot_struct.h>

#include "config.h"
#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

enum {
	VDAT_RW_A = 0,
	VDAT_RW_B = 1,
	VDAT_RECOVERY = 0xFF
};

int crossystem_setup(void)
{
	chromeos_acpi_t *acpi_table = (chromeos_acpi_t *)lib_sysinfo.vdat_addr;
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
	switch (vdat->firmware_index) {
	case VDAT_RW_A:
		main_fw = BINF_RW_A;
		fwid = fmap_rwa_fwid();
		fwid_size = fmap_rwa_fwid_size();
		break;
	case VDAT_RW_B:
		main_fw = BINF_RW_B;
		fwid = fmap_rwb_fwid();
		fwid_size = fmap_rwb_fwid_size();
		break;
	case VDAT_RECOVERY:
		main_fw = BINF_RECOVERY;
		fwid = fmap_ro_fwid();
		fwid_size = fmap_ro_fwid_size();
		break;
	default:
		printf("Unrecognized firmware index %d.\n",
		       vdat->firmware_index);
		return 1;
	}
	acpi_table->main_fw = main_fw;

	// Use the value set by coreboot if we don't want to change it.
	if (CONFIG_EC_SOFTWARE_SYNC) {
		int in_rw = 0;

		if (VbExEcRunningRW(&in_rw)) {
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

	size = MIN(fmap_ro_fwid_size(), sizeof(acpi_table->frid));
	memcpy(acpi_table->frid, fmap_ro_fwid(), size);

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

	return 0;
}

