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

#include <libpayload-config.h>
#include <libpayload.h>
#include <stdint.h>

#include <vboot_api.h>

#include <ahci.h>
#include <config.h>
#include <fmap.h>
#include <gpio.h>

static Fmap *fmap = (Fmap *)(uintptr_t)CONFIG_FMAP_ADDRESS;
static uintptr_t rom_base;

VbCommonParams cparams = {
	.gbb_data = NULL,
	.gbb_size = 0,
	.shared_data_blob = NULL,
	.shared_data_size = 0,
	.vboot_context = NULL,
	.caller_context = NULL
};

static uint8_t shared_data_blob[VB_SHARED_DATA_REC_SIZE];

static int vboot_init(void)
{
	VbInitParams iparams = {
		.flags = 0
	};
	gpio_t dev_switch, rec_switch, wp_switch;
	gpio_fetch(GPIO_DEVSW, &dev_switch);
	gpio_fetch(GPIO_RECSW, &rec_switch);
	gpio_fetch(GPIO_WPSW, &wp_switch);

	// Decide what flags to pass into VbInit.
	if (dev_switch.value)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	if (rec_switch.value)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (wp_switch.value)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	iparams.flags |= VB_INIT_FLAG_RO_NORMAL_SUPPORT;

	printf("Calling VbInit().\n");
	VbError_t res = VbInit(&cparams, &iparams);
	if (res != VBERROR_SUCCESS)
		return 1;

	if (iparams.out_flags && VB_INIT_OUT_CLEAR_RAM)
		printf("VB_INIT_OUT_CLEAR_RAM set but ignored.\n");
	return 0;
};

static int vboot_select_firmware(void)
{
	VbSelectFirmwareParams fparams = {
		.verification_block_A = NULL,
		.verification_block_B = NULL,
		.verification_size_A = 0,
		.verification_size_B = 0
	};

	// Set up the fparams structure.
	FmapArea *vblock_a = fmap_find_area(fmap, "VBLOCK_A");
	FmapArea *vblock_b = fmap_find_area(fmap, "VBLOCK_B");
	if (!vblock_a || !vblock_b) {
		printf("Couldn't find one of the vblocks.\n");
		return 1;
	}
	fparams.verification_block_A = \
		(void *)(vblock_a->area_offset + rom_base);
	fparams.verification_size_A = vblock_a->area_size;
	fparams.verification_block_B = \
		(void *)(vblock_b->area_offset + rom_base);
	fparams.verification_size_B = vblock_b->area_size;

	printf("Calling VbSelectFirmware().\n");
	VbError_t res = VbSelectFirmware(&cparams, &fparams);
	if (res != VBERROR_SUCCESS)
		return 1;

	printf("Selected firmware: ");
	switch (fparams.selected_firmware) {
	case VB_SELECT_FIRMWARE_RECOVERY:
		printf("recovery\n");
		break;
	case VB_SELECT_FIRMWARE_A:
		printf("a\n");
		break;
	case VB_SELECT_FIRMWARE_B:
		printf("b\n");
		break;
	case VB_SELECT_FIRMWARE_READONLY:
		printf("read only\n");
		break;
	}
	return 0;
}

static int vboot_select_and_load_kernel(void)
{
	VbSelectAndLoadKernelParams kparams = {
		.kernel_buffer = (void *)0x100000,
		.kernel_buffer_size = 0x800000
	};

	printf("Calling VbSelectAndLoadKernel().\n");
	VbError_t res = VbSelectAndLoadKernel(&cparams, &kparams);
	if (res != VBERROR_SUCCESS)
		return 1;

	return 0;
}

int main(void)
{
	// Let the world know we're alive.
	outb(0xaa, 0x80);
	printf("\n\nStarting depthcharge...\n");

	get_cpu_speed();

	ahci_init(PCI_DEV(0, 31, 2));

	if (fmap_check_signature(fmap)) {
		printf("Bad signature on the FMAP.\n");
		halt();
	}

	rom_base = (uintptr_t)(-fmap->fmap_size);

	// Set up the common param structure.
	FmapArea *gbb_area = fmap_find_area(fmap, "GBB");
	if (!gbb_area) {
		printf("Couldn't find the GBB.\n");
		halt();
	}

	cparams.gbb_data =
		(void *)(uintptr_t)(gbb_area->area_offset + rom_base);
	cparams.gbb_size = gbb_area->area_size;

	cparams.shared_data_blob = shared_data_blob;
	cparams.shared_data_size = sizeof(shared_data_blob);

	if (vboot_init())
		halt();
	if (vboot_select_firmware())
		halt();
	if (vboot_select_and_load_kernel())
		halt();
	return 0;
}
