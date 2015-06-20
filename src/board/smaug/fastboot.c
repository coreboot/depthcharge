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

#include "board/smaug/fastboot.h"
#include "boot/android_dt.h"
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
		fb_add_string(output, "%s", hardware_name());
		break;
	case FB_BASEBAND_VERSION:
		fb_add_string(output, "%s", "N/A");
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

void fill_fb_info(BlockDevCtrlr *bdev_ctrlr_arr[BDEV_COUNT])
{
	int i;

	for (i = 0; i < BDEV_COUNT; i++)
		fb_fill_bdev_list(i, bdev_ctrlr_arr[i]);
	fb_fill_part_list("chromeos", 0, backend_get_bdev_size_blocks("mmc"));

	FmapArea area;
	const char *name;
	size_t flash_sec_size = lib_sysinfo.spi_flash.sector_size;

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

int board_user_confirmation(void)
{
	int ret = 0;

	while (1) {
		uint32_t ch = getchar();

		if (ch == '\r') {
			ret = 1;
			break;
		}
		if (ch == ' ') {
			ret = 0;
			break;
		}
	}
	return ret;
}

/*
 * This routine is used to handle fastboot calls to write image to special
 * "bootloader" partition. It is assumed that this call is made before fastboot
 * protocol handler calls backend write partition handler.
 * This routine takes an image equal to the size of the spi flash and extracts
 * partitions that need to flashed i.e. ro, rw-a and rw-b.
 */
backend_ret_t board_write_partition(const char *name, void *image_addr,
				    size_t image_size)
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
