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

#include "boot/commandline.h"
#include "boot/zimage.h"
#include "drivers/power_management.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"
#include "vboot/util/memory.h"

#define CMD_LINE_SIZE 4096

static char cmd_line_buf[2 * CMD_LINE_SIZE];

int vboot_init(int dev_switch, int rec_switch,
	       int wp_switch, int oprom_loaded)
{
	VbInitParams iparams = {
		.flags = 0
	};

	// Decide what flags to pass into VbInit.
	if (dev_switch)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	if (rec_switch)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (wp_switch)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	if (oprom_loaded)
		iparams.flags |= VB_INIT_FLAG_OPROM_LOADED;
	iparams.flags |= VB_INIT_FLAG_OPROM_MATTERS;
	iparams.flags |= VB_INIT_FLAG_RO_NORMAL_SUPPORT;

	printf("Calling VbInit().\n");
	VbError_t res = VbInit(&cparams, &iparams);
	if (res != VBERROR_SUCCESS) {
		printf("VbInit returned %d, Doing a cold reboot.\n", res);
		cold_reboot();
	}

	if (iparams.out_flags && VB_INIT_OUT_CLEAR_RAM) {
		if (wipe_unused_memory())
			return 1;
	}
	return 0;
};

int vboot_select_firmware(enum VbSelectFirmware_t *select)
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

	*select = fparams.selected_firmware;

	return 0;
}

int vboot_select_and_load_kernel(void)
{
	static const char cros_secure[] = "cros_secure ";
	static char cmd_line_temp[CMD_LINE_SIZE + sizeof(cros_secure)];

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

	uintptr_t params_addr =
		kparams.bootloader_address - sizeof(struct boot_params);
	struct boot_params *params = (struct boot_params *)params_addr;
	uintptr_t cmd_line_addr = params_addr - CMD_LINE_SIZE;
	strcpy(cmd_line_temp, cros_secure);
	strncat(cmd_line_temp, (char *)cmd_line_addr, CMD_LINE_SIZE);

	if (commandline_subst(cmd_line_temp, 0,
			      kparams.partition_number + 1,
			      kparams.partition_guid,
			      cmd_line_buf,
			      sizeof(cmd_line_buf)))
		return 1;

	if (zboot(params, cmd_line_buf, kparams.kernel_buffer))
		return 1;

	return 0;
}
