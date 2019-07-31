// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Depthcharge driver and Verified Boot interface for the
 * Wilco Embedded Controller.  This provides the functions that
 * allow Verified Boot to use the features provided by this EC.
 */

#include <assert.h>
#include <cbfs_core.h>
#include <libpayload.h>
#include <vboot_api.h>
#include <vb2_api.h>
#include <vb2_sha.h>

#include "base/cleanup_funcs.h"
#include "base/container_of.h"
#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/flash.h"
#include "drivers/ec/wilco/image.h"
#include "drivers/flash/fast_spi.h"

static const char *cbfs_file_ecrw_version = "ecrw.version";

static uint32_t get_ec_version_from_cbfs(void)
{
	uint32_t *version;
	size_t size;

	version = cbfs_get_file_content(CBFS_DEFAULT_MEDIA,
					cbfs_file_ecrw_version,
					CBFS_TYPE_RAW, &size);

	if (!version || size > sizeof(*version)) {
		printf("%s: invalid EC version in %s\n", __func__,
		       cbfs_file_ecrw_version);
		return 0;
	}

	return *version;
}

static int vboot_find_flash_device(WilcoEc *ec, enum VbSelectFirmware_t select,
				   WilcoEcFlashDevice *device)
{
	EcFlashInstance instance;
	int ret;

	/* Select primary or failsafe instance */
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		instance = EC_FLASH_INSTANCE_FAILSAFE;
		break;
	case VB_SELECT_FIRMWARE_EC_ACTIVE:
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		instance = EC_FLASH_INSTANCE_PRIMARY;
		break;
	default:
		printf("%s: Unknown firmware selection %d\n",
		       __func__, select);
		return -1;
	}

	/* Find EC flash device on mainboard */
	ret = wilco_ec_flash_find(ec, device, EC_FLASH_LOCATION_MAIN,
				  EC_FLASH_TYPE_EC, instance);
	if (ret < 0) {
		printf("%s: Unable to find EC device!\n", __func__);
		return ret;
	}

	return 0;
}

static vb2_error_t vboot_hash_image(VbootEcOps *vbec,
				    enum VbSelectFirmware_t select,
				    const uint8_t **hash, int *hash_size)
{
	WilcoEc *ec = container_of(vbec, WilcoEc, vboot);
	static struct vb2_sha256_context ctx;
	WilcoEcImageHeader *header;
	uint8_t *image;
	uint32_t ec_offset, image_offset;
	size_t image_size;

	/* Default to empty hash to force update on VBERROR_SUCCESS */
	*hash = ctx.block;
	*hash_size = VB2_SHA256_DIGEST_SIZE;

	/* Start of EC region in flash */
	ec_offset = ec->flash_offset;

	/* Failsafe instance starts in the middle of flash region */
	if (select == VB_SELECT_FIRMWARE_READONLY)
		ec_offset += ec->flash_size / 2;

	/* Force update if header does not validate */
	header = flash_read(ec_offset, sizeof(*header));
	if (header->tag != EC_IMAGE_HEADER_TAG) {
		printf("%s: header tag invalid (%08x != %08x)\n",
		       __func__, header->tag, EC_IMAGE_HEADER_TAG);
		return VBERROR_SUCCESS;
	}
	if (header->version != EC_IMAGE_HEADER_VER) {
		printf("%s: header version invalid (%d != %d)\n",
		       __func__, header->version, EC_IMAGE_HEADER_VER);
		return VBERROR_SUCCESS;
	}

	/* Offset into flash for start of image data */
	image_offset = ec_offset + sizeof(*header);

	/* Calculate size of image data */
	image_size = header->firmware_size * EC_IMAGE_BLOCK_SIZE;

	/* Include trailing data */
	image_size += EC_IMAGE_TRAILER_SIZE;

	/* Include extra trailing data if encryption is enabled */
	if (header->flags & EC_IMAGE_ENC_FLAG)
		image_size += EC_IMAGE_ENC_TRAILER_SIZE;

	/* Read the image from flash */
	image = flash_read(image_offset, image_size);

	/* Generate SHA-256 digest of the header and image */
	vb2_sha256_init(&ctx);
	vb2_sha256_update(&ctx, (uint8_t *)header, sizeof(*header));
	vb2_sha256_update(&ctx, image, image_size);
	vb2_sha256_finalize(&ctx, (uint8_t *)*hash);

	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_update_image(VbootEcOps *vbec,
				      enum VbSelectFirmware_t select,
				      const uint8_t *image, int image_size)
{
	WilcoEc *ec = container_of(vbec, WilcoEc, vboot);
	WilcoEcFlashDevice device;
	uint32_t version = get_ec_version_from_cbfs();
	int ret;

	/* Find the main EC device to flash */
	ret = vboot_find_flash_device(ec, select, &device);
	if (ret == WILCO_EC_RESULT_ACCESS_DENIED)
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	else if (ret < 0)
		return VBERROR_UNKNOWN;

	/* Write image to the previously found EC device */
	if (wilco_ec_flash_image(ec, &device, image, image_size, version) < 0) {
		printf("%s: EC update failed!\n", __func__);
		return VBERROR_UNKNOWN;
	}

	printf("%s: EC update successful.\n", __func__);
	ec->flash_updated = true;

	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_reboot_to_ro(VbootEcOps *vbec)
{
	WilcoEc *ec = container_of(vbec, WilcoEc, vboot);
	if (wilco_ec_reboot(ec) < 0)
		return VBERROR_UNKNOWN;
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_jump_to_rw(VbootEcOps *vbec)
{
	WilcoEc *ec = container_of(vbec, WilcoEc, vboot);
	printf("%s: start\n", __func__);
	if (ec->flash_updated) {
		/* Issue EC reset to boot new image */
		if (wilco_ec_reboot(ec) < 0)
			return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_reboot_switch_rw(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_disable_jump(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_entering_mode(VbootEcOps *vbec,
				       enum VbEcBootMode_t vbm)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_protect(VbootEcOps *vbec,
				 enum VbSelectFirmware_t select)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_running_rw(VbootEcOps *vbec, int *in_rw)
{
	*in_rw = 0;
	return VBERROR_SUCCESS;
};

static vb2_error_t vboot_battery_cutoff(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_check_limit_power(VbootEcOps *vbec, int *limit_power)
{
	*limit_power = 0;
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_enable_power_button(VbootEcOps *vbec, int enable)
{
	WilcoEc *ec = container_of(vbec, WilcoEc, vboot);
	if (wilco_ec_power_button(ec, enable) < 0)
		return VBERROR_UNKNOWN;
	return VBERROR_SUCCESS;
}

static int wilco_ec_cleanup_boot(CleanupFunc *cleanup, CleanupType type)
{
	WilcoEc *ec = cleanup->data;
	return wilco_ec_exit_firmware(ec);
}

enum {
	I8042_CMD_RD_CMD_BYTE = 0x20,
	I8042_CMD_WR_CMD_BYTE = 0x60,
	I8042_CMD_BYTE_XLATE = (1 << 6),
};

static void wilco_ec_keyboard_setup(void)
{
	i8042_probe();

	/* Put keyboard into translated mode for the OS */
	if (!i8042_cmd(I8042_CMD_RD_CMD_BYTE)) {
		int cmd = i8042_read_data_ps2();
		cmd |= I8042_CMD_BYTE_XLATE;
		if (!i8042_cmd(I8042_CMD_WR_CMD_BYTE))
			i8042_write_data(cmd);
	}
}

WilcoEc *new_wilco_ec(uint16_t ec_host_base, uint16_t mec_emi_base,
		      uint32_t flash_offset, uint32_t flash_size)
{
	WilcoEc *ec;

	/* Make sure parameter is valid */
	assert (ec_host_base && mec_emi_base &&
		flash_offset && flash_size);

	ec = xzalloc(sizeof(*ec));
	ec->io_base_packet = mec_emi_base;
	ec->io_base_data = ec_host_base;
	ec->io_base_command = ec_host_base + 4;

	ec->cleanup.cleanup = &wilco_ec_cleanup_boot;
	ec->cleanup.types = CleanupOnHandoff;
	ec->cleanup.data = ec;
	list_insert_after(&ec->cleanup.list_node, &cleanup_funcs);

	/* Location of EC region in flash */
	ec->flash_offset = flash_offset;
	ec->flash_size = flash_size;

	ec->vboot.hash_image = vboot_hash_image;
	ec->vboot.reboot_to_ro = vboot_reboot_to_ro;
	ec->vboot.reboot_switch_rw = vboot_reboot_switch_rw;
	ec->vboot.jump_to_rw = vboot_jump_to_rw;
	ec->vboot.disable_jump = vboot_disable_jump;
	ec->vboot.entering_mode = vboot_entering_mode;
	ec->vboot.update_image = vboot_update_image;
	ec->vboot.protect = vboot_protect;
	ec->vboot.running_rw = vboot_running_rw;
	ec->vboot.battery_cutoff = vboot_battery_cutoff;
	ec->vboot.check_limit_power = vboot_check_limit_power;
	ec->vboot.enable_power_button = vboot_enable_power_button;

	wilco_ec_keyboard_setup();

	printf("Wilco EC [base 0x%04x emi 0x%04x] flash 0x%08x-0x%08x\n",
	       ec_host_base, mec_emi_base, flash_offset,
	       flash_offset + flash_size - 1);

	return ec;
}
