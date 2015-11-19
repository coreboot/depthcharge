/*
 * Copyright 2015 Google Inc.
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
#include <udc/dwc2_udc.h>

#include "board/veyron_shark/fastboot.h"
#include "config.h"
#include "drivers/bus/usb/usb.h"
#include "image/fmap.h"
#include "vboot/firmware_id.h"

struct bdev_info fb_bdev_list[BDEV_COUNT] = {
	[MMC_BDEV] = {"mmc", NULL, NULL},
	[FLASH_BDEV] = {"flash", NULL, NULL},
};

size_t fb_bdev_count = ARRAY_SIZE(fb_bdev_list);

struct part_info fb_part_list[] = {
	PART_NONGPT("chromeos", NULL, BDEV_ENTRY(MMC_BDEV), 0, 0),
	PART_NONGPT("mbr", NULL, BDEV_ENTRY(MMC_BDEV), 0, 1),
	PART_NONGPT("gpt", NULL, BDEV_ENTRY(MMC_BDEV), 1, 33),
	PART_NONGPT("bootloader", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RO_SECTION", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RO_VPD", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RW_SECTION_A", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RW_SECTION_B", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_GPT("kernel-a", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("kernel-b", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 1),
	PART_GPT("rootfs-a", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_ROOTFS), 0),
	PART_GPT("rootfs-b", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_ROOTFS), 1),
	PART_GPT("data", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 0),
	PART_GPT("userdata", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 0),
	PART_GPT("system", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 1),
	PART_GPT("metadata", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 2),
	PART_GPT("misc", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 3),
	PART_GPT("cache", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 4),
	PART_GPT("persistent", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 5),
};

size_t fb_part_count = ARRAY_SIZE(fb_part_list);

int get_board_var(struct fb_cmd *cmd, fb_getvar_t var)
{
	int ret = 0;
	struct fb_buffer *output = &cmd->output;

	switch(var) {
	case FB_BOOTLOADER_VERSION: {
		const char *version = get_active_fw_id();

		if (version == NULL)
			ret = -1;
		else
			fb_add_string(output, "%s", version);
		break;
	}
	case FB_PRODUCT:
		fb_add_string(output, "shark", NULL);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

int board_should_enter_device_mode(void)
{
	return 1;
}

void fastboot_chipset_init(struct usbdev_ctrl **udc, device_descriptor_t *dd)
{
	*udc = dwc2_udc_init(dd);
}

void fill_fb_info(BlockDevCtrlr *bdev_ctrlr_arr[BDEV_COUNT])
{
	int i;
	const char *name;
	FmapArea area;
	uint64_t flash_sec_size = lib_sysinfo.spi_flash.sector_size;

	for (i = 0; i < BDEV_COUNT; i++)
		fb_fill_bdev_list(i, bdev_ctrlr_arr[i]);

	fb_fill_part_list("chromeos", 0, backend_get_bdev_size_blocks("mmc"));

	for (i = 0; i < fb_part_count; i++) {
		if (fb_part_list[i].bdev_info != BDEV_ENTRY(FLASH_BDEV))
			continue;

		name = fb_part_list[i].part_name;

		if (fmap_find_area(name, &area)) {
			/*
			 * Special partition bootloader cannot be found on the
			 * spi flash using fmap_find_area. Check if current
			 * flash_bdec partition is bootloader and set its range
			 * as the entire spi flash.
			 */
			if (!strcmp(name, "bootloader"))
				fb_fill_part_list("bootloader", 0,
						  lib_sysinfo.spi_flash.size /
						  flash_sec_size);
			else
				printf("ERROR: Area %s not found\n", name);
			continue;
		}

		assert(ALIGN(area.offset, flash_sec_size) == area.offset);
		assert(ALIGN(area.size, flash_sec_size) == area.size);

		fb_fill_part_list(name, area.offset / flash_sec_size,
				  area.size / flash_sec_size);
	}

}

fb_callback_t fb_board_handler = {
	.get_var = get_board_var,
	.enter_device_mode = board_should_enter_device_mode,
	.print_screen = fb_print_text_on_screen,
};
