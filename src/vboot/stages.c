/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#include <libpayload.h>
#include <stdint.h>
#include <vboot_api.h>

#include "base/power_management.h"
#include "base/timestamp.h"
#include "boot/boot.h"
#include "config.h"
#include "image/fmap.h"
#include "image/startrw.h"
#include "image/symbols.h"
#include "vboot/util/acpi.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"
#include "vboot/util/memory.h"

int vboot_init(void)
{
	VbInitParams iparams = {
		.flags = 0
	};

	int dev_switch = flag_fetch(FLAG_DEVSW);
	int rec_switch = flag_fetch(FLAG_RECSW);
	int wp_switch = flag_fetch(FLAG_WPSW);
	int oprom_loaded = flag_fetch(FLAG_OPROM);
	if (dev_switch < 0 || rec_switch < 0 ||
	    wp_switch < 0 || oprom_loaded < 0) {
		// An error message should have already been printed.
		return 1;
	}

	// Decide what flags to pass into VbInit.
	if (dev_switch)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	if (rec_switch)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (wp_switch)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	if (oprom_loaded)
		iparams.flags |= VB_INIT_FLAG_OPROM_LOADED;
	if (CONFIG_OPROM_MATTERS)
		iparams.flags |= VB_INIT_FLAG_OPROM_MATTERS;
	if (CONFIG_RO_NORMAL_SUPPORT)
		iparams.flags |= VB_INIT_FLAG_RO_NORMAL_SUPPORT;
	if (CONFIG_VIRTUAL_DEV_SWITCH)
		iparams.flags |= VB_INIT_FLAG_VIRTUAL_DEV_SWITCH;

	printf("Calling VbInit().\n");
	VbError_t res = VbInit(&cparams, &iparams);
	if (res != VBERROR_SUCCESS) {
		printf("VbInit returned %d, Doing a cold reboot.\n", res);
		cold_reboot();
	}

	if (iparams.out_flags & VB_INIT_OUT_CLEAR_RAM) {
		if (wipe_unused_memory())
			return 1;
	}
	return 0;
};

int vboot_select_firmware(void)
{
	VbSelectFirmwareParams fparams = {
		.verification_block_A = NULL,
		.verification_block_B = NULL,
		.verification_size_A = 0,
		.verification_size_B = 0
	};

	// Set up the fparams structure.
	FmapArea *vblock_a = fmap_find_area(main_fmap, "VBLOCK_A");
	FmapArea *vblock_b = fmap_find_area(main_fmap, "VBLOCK_B");
	if (!vblock_a || !vblock_b) {
		printf("Couldn't find one of the vblocks.\n");
		return 1;
	}
	fparams.verification_block_A = \
		(void *)(vblock_a->offset + main_rom_base);
	fparams.verification_size_A = vblock_a->size;
	fparams.verification_block_B = \
		(void *)(vblock_b->offset + main_rom_base);
	fparams.verification_size_B = vblock_b->size;

	printf("Calling VbSelectFirmware().\n");
	VbError_t res = VbSelectFirmware(&cparams, &fparams);
	if (res != VBERROR_SUCCESS) {
		printf("VbSelectFirmware returned %d, "
		       "Doing a cold reboot.\n", res);
		cold_reboot();
	}

	enum VbSelectFirmware_t select = fparams.selected_firmware;

	// If an RW firmware was selected, start it.
	if (select == VB_SELECT_FIRMWARE_A || select == VB_SELECT_FIRMWARE_B) {
		FmapArea *rw_area;
		if (select == VB_SELECT_FIRMWARE_A)
			rw_area = fmap_find_area(main_fmap, "FW_MAIN_A");
		else
			rw_area = fmap_find_area(main_fmap, "FW_MAIN_B");
		uintptr_t rw_addr = rw_area->offset + main_rom_base;
		if (start_rw_firmware((void *)rw_addr))
			return 1;
	}

	return 0;
}

int vboot_select_and_load_kernel(void)
{
	VbSelectAndLoadKernelParams kparams = {
		.kernel_buffer = (void *)&_kernel_start,
		.kernel_buffer_size = &_kernel_end - &_kernel_start
	};

	printf("Calling VbSelectAndLoadKernel().\n");
	VbError_t res = VbSelectAndLoadKernel(&cparams, &kparams);
	if (res != VBERROR_SUCCESS) {
		printf("VbSelectAndLoadKernel returned %d, "
		       "Doing a cold reboot.\n", res);
		cold_reboot();
	}

	timestamp_add_now(TS_CROSSYSTEM_DATA);

	// Update the crossystem data in the ACPI tables.
	if (acpi_update_data())
		halt();

	if (boot(kparams.kernel_buffer,
		 (void *)(uintptr_t)kparams.bootloader_address,
		 kparams.partition_number, kparams.partition_guid))
		return 1;

	return 0;
}
