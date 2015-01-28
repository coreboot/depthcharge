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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#include <libpayload.h>
#include <udc/chipidea.h>

#include "board/rush_ryu/fastboot.h"
#include "config.h"
#include "drivers/bus/usb/usb.h"

struct bdev_info fb_bdev_list[BDEV_COUNT] = {
	[MMC_BDEV] = {"mmc", NULL, NULL},
	/* [FLASH_BDEV] = {"flash", NULL, NULL}, */
};

size_t fb_bdev_count = ARRAY_SIZE(fb_bdev_list);

struct part_info fb_part_list[] = {
	PART_GPT("boot", "ext4", BDEV_NAME(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("kernel-a", "ext4", BDEV_NAME(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("kernel-b", "ext4", BDEV_NAME(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 1),
	PART_GPT("kernel", "ext4", BDEV_NAME(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("system", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS), 0),
	PART_GPT("vendor", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS), 1),
	PART_GPT("cache", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS), 2),
	PART_GPT("data", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS), 3),
	PART_GPT("metadata", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 4),
	PART_GPT("boot", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS), 5),
	PART_GPT("recovery", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 6),
	PART_GPT("misc", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS), 7),
	PART_GPT("bootloader", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 8),
	PART_GPT("persistent", "ext4", BDEV_NAME(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 9),
	PART_NONGPT("gpt", "ext4", BDEV_NAME(MMC_BDEV), 1, 33),
};

size_t fb_part_count = ARRAY_SIZE(fb_part_list);

int get_board_var(fb_getvar_t var, const char *input, size_t input_len,
		  char *str, size_t str_len)
{
	int ret = 0;

	switch(var) {
	case FB_BOOTLOADER_VERSION:
		snprintf(str, str_len, "coreboot-%s", lib_sysinfo.cb_version);
		break;
	case FB_PRODUCT:
		snprintf(str, str_len, "google,ryu-rev%d",
			 lib_sysinfo.board_id);
		break;
	case FB_DWNLD_SIZE:
		/* Max download size set to half of heap size */
		snprintf(str, str_len, "0x%x", CONFIG_HEAP_SIZE/2);
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
	dc_usb_initialize();
	*udc = chipidea_init(dd);
}
