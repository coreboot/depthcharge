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

#define NEED_VB20_INTERNALS  /* Poking around inside NV storage fields */

#include <libpayload.h>
#include <udc/chipidea.h>

#include "board/smaug/fastboot.h"
#include "board/smaug/input.h"
#include "boot/android_dt.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/flash/block_flash.h"
#include "image/fmap.h"
#include "vboot/firmware_id.h"
#include "vboot/vbnv.h"

struct bdev_info fb_bdev_list[BDEV_COUNT] = {
	[MMC_BDEV] = {"mmc", NULL, NULL},
	[FLASH_BDEV] = {"flash", NULL, NULL},
};

size_t fb_bdev_count = ARRAY_SIZE(fb_bdev_list);

struct part_info fb_part_list[] = {
	PART_GPT("boot", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("kernel-a", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("kernel-b", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 1),
	PART_GPT("recovery", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 2),
	PART_GPT("kernel", "ext4", BDEV_ENTRY(MMC_BDEV),
		 GPT_TYPE(CHROMEOS_KERNEL), 0),
	PART_GPT("system", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 0),
	PART_GPT("vendor", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 1),
	PART_GPT("cache", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 2),
	PART_GPT("data", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 3),
	PART_GPT("userdata", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 3),
	PART_GPT("metadata", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 4),
	PART_GPT("boot", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 5),
	PART_GPT("misc", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS), 6),
	PART_GPT("persistent", "ext4", BDEV_ENTRY(MMC_BDEV), GPT_TYPE(LINUX_FS),
		 7),
	PART_NONGPT("chromeos", NULL, BDEV_ENTRY(MMC_BDEV), 0, 0),
	PART_NONGPT("mbr", NULL, BDEV_ENTRY(MMC_BDEV), 0, 1),
	PART_NONGPT("gpt", NULL, BDEV_ENTRY(MMC_BDEV), 1, 33),
	PART_NONGPT("RO_SECTION", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RO_VPD", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RW_SECTION_A", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	PART_NONGPT("RW_SECTION_B", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 0),
	/*
	 * "bootloader" is a special alias used to flash different firmware
	 * partitions to spi flash. When asked by the host, we report entire spi
	 * flash as the size of this partition. When host asks the device to
	 * flash an image to bootloader partition, device will extract specific
	 * parts from the image i.e. ro, rw-a, rw-b and write them to
	 * appropriate offsets on the spi flash.
	 * This partition name is required to satisfy host-side scripts that
	 * want to update ro, rw-a and rw-b sections of the firmware using a
	 * single name and image.
	 */
	PART_NONGPT("bootloader", NULL, BDEV_ENTRY(FLASH_BDEV), 0, 9),
};

size_t fb_part_count = ARRAY_SIZE(fb_part_list);

static int get_board_var(struct fb_cmd *cmd, fb_getvar_t var)
{
	int ret = 0;
	struct fb_buffer *output = &cmd->output;

	switch(var) {
	case FB_BOOTLOADER_VERSION: {
		/*
		 * It is tricky to report back bootloader version for
		 * us. fastboot expects a single value for bootloader
		 * version. However, we have 3 copies of bootloader image
		 * i.e. RO, RW-A and RW-B.
		 *
		 * It might not be right to provide RO bootloader version
		 * always, especially if user receives OTA update for firmware
		 * or flashes a new firmware with different version without
		 * disabling write-protect.
		 *
		 * This scheme reads the index of firmware tried for last boot
		 * and reports the version of that firmware index. Here, the
		 * assumption is that since the firmware was tried last it is
		 * the one with most recent version. NOTE: Currently, we do not
		 * have any mechanism in OS to set the FW_PREV_BOOT_RESULT.
		 *
		 */
		uint8_t index = vbnv_read(VB2_NV_FW_PREV_TRIED);
		if (index > VBSD_RW_B)
			index = VBSD_RO;

		const char *version = get_fw_id(index);

		if (version == NULL)
			ret = -1;
		else
			fb_add_string(output, "%s", version);
		break;
	}
	case FB_PRODUCT:
		fb_add_string(output, "%s", hardware_name());
		break;
	case FB_BASEBAND_VERSION:
		fb_add_string(output, "%s", "N/A");
		break;
	case FB_VARIANT:
		/*
		 * We do not have any variants for this board. Return empty
		 * string.
		 */
		fb_add_string(output, "", NULL);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int board_should_enter_device_mode(void)
{
	return 1;
}

void fastboot_chipset_init(struct usbdev_ctrl **udc, device_descriptor_t *dd)
{
	dc_usb_initialize();
	*udc = chipidea_init(dd);
}

void fill_fb_info(TegraMmcHost *emmc, SpiFlash *flash)
{
	FlashBlockDev *fbdev = block_flash_register_nor(&flash->ops);
	FmapArea area;
	const char *name;
	size_t flash_sec_size = lib_sysinfo.spi_flash.sector_size;
	int i;

	fb_fill_bdev_list(MMC_BDEV, &emmc->mmc.ctrlr);
	fb_fill_part_list("chromeos", 0, backend_get_bdev_size_blocks("mmc"));

	fb_fill_bdev_list(FLASH_BDEV, &fbdev->ctrlr);

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

/*
 * This routine is used to handle fastboot calls to write image to special
 * "bootloader" partition. It is assumed that this call is made before fastboot
 * protocol handler calls backend write partition handler.
 * This routine takes an image equal to the size of the spi flash and extracts
 * partitions that need to flashed i.e. ro, rw-a and rw-b.
 */
backend_ret_t board_write_partition(const char *name, void *image_addr,
				    uint64_t image_size)
{
	/* Handle writes to bootloader partition. Others use default path. */
	if (strcmp(name, "bootloader"))
		return BE_NOT_HANDLED;

	/* Cannot handle sparse image here. */
	if (is_sparse_image(image_addr))
		return BE_WRITE_ERR;

	static const char * const part_names[] = {
		"RO_SECTION",
		"RW_SECTION_A",
		"RW_SECTION_B",
	};

	int i;
	struct part_info *part;
	uintptr_t offset;
	size_t size;
	size_t flash_size = lib_sysinfo.spi_flash.size;
	size_t flash_sec_size = lib_sysinfo.spi_flash.sector_size;

	for (i = 0; i < ARRAY_SIZE(part_names); i++) {

		part = get_part_info(part_names[i]);

		/* Ensure image size is equal to flash size. */
		if (flash_size != image_size)
			return BE_IMAGE_OVERFLOW_ERR;

		/*
		 * We only write to specific regions in spi flash. Calculate the
		 * offset within the image to write to ro, rw-a and rw-b
		 * sections.
		 */
		offset = (uintptr_t)image_addr + part->base * flash_sec_size;
		size = part->size * flash_sec_size;

		backend_write_partition(part_names[i], (void *)offset, size);
	}

	return BE_SUCCESS;
}

static const struct {
	fb_button_type button;
	const char *string;
	uint8_t input;
} button_table[] = {
	{FB_BUTTON_UP, "Volume up", KEYSET(0, VOL_UP, 0)},
	{FB_BUTTON_DOWN, "Volume down", KEYSET(0, 0, VOL_DOWN)},
	{FB_BUTTON_SELECT, "Power button", KEYSET(PWR_BTN, 0, 0)},
	{FB_BUTTON_CONFIRM, "Power button", KEYSET(PWR_BTN, 0, 0)},
	{FB_BUTTON_CANCEL, "Volume down", KEYSET(0, 0, VOL_DOWN)},
};

static const char *get_button_str(fb_button_type button)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(button_table); i++)
		if (button_table[i].button == button)
			return button_table[i].string;

	die("%d not implemented!\n", button);
	return NULL;
}

static fb_button_type read_input(uint32_t flags, uint64_t timeout_us)
{
	int i;
	uint8_t input;

	while (1) {
		input = read_char(timeout_us);

		if (input == NO_BTN_PRESSED)
			return FB_BUTTON_NONE;

		for (i = 0; i < ARRAY_SIZE(button_table); i++) {
			if ((button_table[i].button & flags) &&
			    (button_table[i].input == input))
				return button_table[i].button;
		}
	}
}

fb_callback_t fb_board_handler = {
	.get_var = get_board_var,
	.enter_device_mode = board_should_enter_device_mode,
	.keyboard_mask = ec_fb_keyboard_mask,
	.print_screen = fb_print_text_on_screen,
	.read_batt_volt = cros_ec_read_batt_volt,
	.battery_cutoff = ec_fb_battery_cutoff,
	.read_input = read_input,
	.get_button_str = get_button_str,
	.double_tap_disable = ec_fb_double_tap_disable,
};
