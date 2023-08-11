// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "base/cleanup_funcs.h"
#include "base/container_of.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/commands_api.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"

/* Timeout waiting for EC hash calculation completion */
static const int CROS_EC_HASH_TIMEOUT_MS = 2000;

/* Time to delay between polling status of EC hash calculation */
static const int CROS_EC_HASH_CHECK_DELAY_MS = 10;

static vb2_error_t vboot_running_rw(VbootEcOps *vbec, int *in_rw)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	struct ec_response_get_version r;

	if (ec_cmd_get_version(me, &r) < 0)
		return VB2_ERROR_UNKNOWN;

	switch (r.current_image) {
	case EC_IMAGE_RO:
		*in_rw = 0;
		break;
	case EC_IMAGE_RW:
		*in_rw = 1;
		break;
	default:
		printf("Unrecognized EC image type %d.\n", r.current_image);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static uint32_t get_vboot_hash_offset(enum vb2_firmware_selection select)
{
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		return EC_VBOOT_HASH_OFFSET_RO;
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		return EC_VBOOT_HASH_OFFSET_UPDATE;
	default:
		return EC_VBOOT_HASH_OFFSET_ACTIVE;
	}
}

static enum ec_flash_region vboot_to_ec_region(
	enum vb2_firmware_selection select)
{
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		return EC_FLASH_REGION_WP_RO;
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		return EC_FLASH_REGION_UPDATE;
	default:
		return EC_FLASH_REGION_ACTIVE;
	}
}

static vb2_error_t vboot_hash_image(VbootEcOps *vbec,
				    enum vb2_firmware_selection select,
				    const uint8_t **hash, int *hash_size)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	static struct ec_response_vboot_hash resp;
	struct ec_params_vboot_hash p = { 0 };
	uint64_t start;
	int recalc_requested = 0;
	uint32_t hash_offset;

	hash_offset = get_vboot_hash_offset(select);

	start = timer_us(0);
	do {
		/* Get hash if available. */
		p.cmd = EC_VBOOT_HASH_GET;
		p.offset = hash_offset;
		if (ec_cmd_vboot_hash(me, &p, &resp) < 0)
			return VB2_ERROR_UNKNOWN;

		switch (resp.status) {
		case EC_VBOOT_HASH_STATUS_NONE:
			/* We have no valid hash - let's request a recalc
			 * if we haven't done so yet. */
			if (recalc_requested != 0)
				break;
			printf("%s: No valid hash (status=%d size=%d). "
			      "Compute one...\n", __func__, resp.status,
			      resp.size);

			p.cmd = EC_VBOOT_HASH_START;
			p.hash_type = EC_VBOOT_HASH_TYPE_SHA256;
			p.nonce_size = 0;

			if (ec_cmd_vboot_hash(me, &p, &resp) < 0)
				return VB2_ERROR_UNKNOWN;

			recalc_requested = 1;
			/* Expect status to be busy (and don't break while)
			 * since we just sent a recalc request. */
			resp.status = EC_VBOOT_HASH_STATUS_BUSY;
			break;
		case EC_VBOOT_HASH_STATUS_BUSY:
			/* Hash is still calculating. */
			mdelay(CROS_EC_HASH_CHECK_DELAY_MS);
			break;
		case EC_VBOOT_HASH_STATUS_DONE:
		default:
			/* We have a valid hash. */
			break;
		}
	} while (resp.status == EC_VBOOT_HASH_STATUS_BUSY &&
		 timer_us(start) < CROS_EC_HASH_TIMEOUT_MS * 1000);

	if (resp.status != EC_VBOOT_HASH_STATUS_DONE) {
		printf("%s: Hash status not done: %d\n", __func__,
		      resp.status);
		return VB2_ERROR_UNKNOWN;
	}
	if (resp.hash_type != EC_VBOOT_HASH_TYPE_SHA256) {
		printf("EC hash was the wrong type.\n");
		return VB2_ERROR_UNKNOWN;
	}

	*hash = resp.hash_digest;
	*hash_size = resp.digest_size;

	return VB2_SUCCESS;
}

static vb2_error_t vboot_reboot_to_ro(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (cros_ec_reboot_param(me, EC_REBOOT_COLD,
				 EC_REBOOT_FLAG_ON_AP_SHUTDOWN) < 0) {
		printf("Failed to schedule EC reboot to RO.\n");
		return VB2_ERROR_UNKNOWN;
	}

	power_off();
}

static vb2_error_t vboot_reboot_ap_off(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (cros_ec_reboot_param(me, EC_REBOOT_COLD_AP_OFF,
				 EC_REBOOT_FLAG_ON_AP_SHUTDOWN) < 0) {
		printf("Failed to schedule EC reboot to AP_OFF.\n");
		return VB2_ERROR_UNKNOWN;
	}

	power_off();
}

static vb2_error_t vboot_jump_to_rw(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (cros_ec_reboot_param(me, EC_REBOOT_JUMP_RW, 0) < 0) {
		printf("Failed to make the EC jump to RW.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static vb2_error_t vboot_reboot_switch_rw(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (cros_ec_reboot_param(me, EC_REBOOT_COLD,
				 EC_REBOOT_FLAG_SWITCH_RW_SLOT) < 0) {
		printf("Failed to reboot the EC to switch RW slot.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static vb2_error_t vboot_disable_jump(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (cros_ec_reboot_param(me, EC_REBOOT_DISABLE_JUMP, 0) < 0) {
		printf("Failed to make the EC disable jumping.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static int ec_flash_protect(CrosEc *me, uint32_t set_mask, uint32_t set_flags,
			    struct ec_response_flash_protect *resp)
{
	struct ec_params_flash_protect params;

	params.mask = set_mask;
	params.flags = set_flags;

	if (ec_cmd_flash_protect_v1(me, &params, resp) != sizeof(*resp))
		return -1;

	return 0;
}

/**
 * Obtain position and size of a flash region
 *
 * @param region	Flash region to query
 * @param offset	Returns offset of flash region in EC flash
 * @param size		Returns size of flash region
 * @return 0 if ok, -1 on error
 */
static int ec_flash_offset(CrosEc *me, enum ec_flash_region region,
			   uint32_t *offset, uint32_t *size)
{
	struct ec_params_flash_region_info p;
	struct ec_response_flash_region_info r;
	int ret;

	p.region = region;
	ret = ec_cmd_flash_region_info_v1(me, &p, &r);
	if (ret != sizeof(r))
		return -1;

	if (offset)
		*offset = r.offset;
	if (size)
		*size = r.size;

	return 0;
}

static int ec_flash_erase(CrosEc *me, uint32_t offset, uint32_t size)
{
	struct ec_params_flash_erase p;

	p.offset = offset;
	p.size = size;
	return ec_cmd_flash_erase(me, &p);
}

/**
 * Write a single block to the flash
 *
 * Write a block of data to the EC flash. The size must not exceed the flash
 * write block size which you can obtain from cros_ec_flash_write_burst_size().
 *
 * The offset starts at 0. You can obtain the region information from
 * cros_ec_flash_offset() to find out where to write for a particular region.
 *
 * Attempting to write to the region where the EC is currently running from
 * will result in an error.
 *
 * @param data		Pointer to data buffer to write
 * @param offset	Offset within flash to write to.
 * @param size		Number of bytes to write
 * @return 0 if ok, -1 on error
 */
static int ec_flash_write_block(CrosEc *me, const uint8_t *data,
				uint32_t offset, uint32_t size)
{
	uint8_t *buf;
	struct ec_params_flash_write *p;
	uint32_t bufsize = sizeof(*p) + size;
	int rv;

	assert(data);

	/* Make sure request fits in the allowed packet size */
	if (bufsize > me->max_param_size)
		return -1;

	buf = xmalloc(bufsize);

	p = (struct ec_params_flash_write *)buf;
	p->offset = offset;
	p->size = size;
	memcpy(p + 1, data, size);

	rv = ec_command(me, EC_CMD_FLASH_WRITE,
			0, buf, bufsize, NULL, 0) >= 0 ? 0 : -1;

	free(buf);

	return rv;
}

/**
 * Return optimal flash write burst size
 */
static int ec_flash_write_burst_size(CrosEc *me)
{
	struct ec_response_flash_info info;
	uint32_t pdata_max_size = me->max_param_size -
		sizeof(struct ec_params_flash_write);

	/*
	 * Determine whether we can use version 1 of the command with more
	 * data, or only version 0.
	 */
	if (!cros_ec_cmd_version_supported(EC_CMD_FLASH_WRITE,
					   EC_VER_FLASH_WRITE))
		return EC_FLASH_WRITE_VER0_SIZE;

	/*
	 * Determine step size.  This must be a multiple of the write block
	 * size, and must also fit into the host parameter buffer.
	 */
	if (ec_cmd_flash_info(me, &info) != sizeof(info))
		return 0;

	return (pdata_max_size / info.write_block_size) *
		info.write_block_size;
}

/**
 * Write data to the flash
 *
 * Write an arbitrary amount of data to the EC flash, by repeatedly writing
 * small blocks.
 *
 * The offset starts at 0. You can obtain the region information from
 * cros_ec_flash_offset() to find out where to write for a particular region.
 *
 * Attempting to write to the region where the EC is currently running from
 * will result in an error.
 *
 * @param data		Pointer to data buffer to write
 * @param offset	Offset within flash to write to.
 * @param size		Number of bytes to write
 * @return 0 if ok, -1 on error
 */
static int ec_flash_write(CrosEc *me, const uint8_t *data, uint32_t offset,
			uint32_t size)
{
	uint32_t burst = ec_flash_write_burst_size(me);
	uint32_t end, off;
	int ret;

	if (!burst)
		return -1;

	end = offset + size;
	for (off = offset; off < end; off += burst, data += burst) {
		uint32_t todo = MIN(end - off, burst);
		/* If SPI flash needs to add padding to make a legitimate write
		 * block, do so on EC. */
		ret = ec_flash_write_block(me, data, off, todo);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * Run verification on a slot
 *
 * @param me     CrosEc instance
 * @param region Region to run verification on
 * @return 0 if success or not applicable. Non-zero if verification failed.
 */
static int ec_efs_verify(CrosEc *me, enum ec_flash_region region)
{
	struct ec_params_efs_verify p;
	int rv;
	p.region = region;

	rv = ec_cmd_efs_verify(me, &p);
	if (rv >= 0) {
		printf("EFS: Verification success\n");
		return 0;
	}

	printf("EFS: Verification failed for %d\n", rv);
	return rv;
}

static vb2_error_t vboot_set_region_protection(
	CrosEc *me, enum vb2_firmware_selection select, int enable)
{
	struct ec_response_flash_protect resp;
	uint32_t protected_region = EC_FLASH_PROTECT_ALL_NOW;
	uint32_t mask = EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_ALL_AT_BOOT;

	if (select == VB_SELECT_FIRMWARE_READONLY)
		protected_region = EC_FLASH_PROTECT_RO_NOW;

	/* Update protection */
	if (ec_flash_protect(me, mask, enable ? mask : 0, &resp) < 0) {
		printf("Failed to update EC flash protection.\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (!enable) {
		/* If protection is still enabled, need reboot */
		if (resp.flags & protected_region)
			return VB2_REQUEST_REBOOT_EC_TO_RO;

		return VB2_SUCCESS;
	}

	/*
	 * If write protect and ro-at-boot aren't both asserted, don't expect
	 * protection enabled.
	 */
	if ((~resp.flags) & (EC_FLASH_PROTECT_GPIO_ASSERTED |
			     EC_FLASH_PROTECT_RO_AT_BOOT))
		return VB2_SUCCESS;

	/* If flash is protected now, success */
	if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
		return VB2_SUCCESS;

	/* If RW will be protected at boot but not now, need a reboot */
	if (resp.flags & EC_FLASH_PROTECT_ALL_AT_BOOT)
		return VB2_REQUEST_REBOOT_EC_TO_RO;

	/* Otherwise, it's an error */
	return VB2_ERROR_UNKNOWN;
}

static vb2_error_t vboot_update_image(VbootEcOps *vbec,
				      enum vb2_firmware_selection select,
				      const uint8_t *image, int image_size)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	uint32_t region_offset, region_size;
	enum ec_flash_region region = vboot_to_ec_region(select);
	vb2_error_t rv = vboot_set_region_protection(me, select, 0);
	if (rv == VB2_REQUEST_REBOOT_EC_TO_RO || rv != VB2_SUCCESS)
		return rv;

	if (ec_flash_offset(me, region, &region_offset, &region_size))
		return VB2_ERROR_UNKNOWN;
	if (image_size > region_size)
		return VB2_ERROR_INVALID_PARAMETER;

	/*
	 * Erase the entire region, so that the EC doesn't see any garbage
	 * past the new image if it's smaller than the current image.
	 *
	 * TODO: could optimize this to erase just the current image, since
	 * presumably everything past that is 0xff's.  But would still need to
	 * round up to the nearest multiple of erase size.
	 */
	if (ec_flash_erase(me, region_offset, region_size))
		return VB2_ERROR_UNKNOWN;

	/* Write the image */
	if (ec_flash_write(me, image, region_offset, image_size))
		return VB2_ERROR_UNKNOWN;

	/* Verify the image */
	if (CONFIG(EC_EFS) && ec_efs_verify(me, region))
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

static vb2_error_t vboot_protect(VbootEcOps *vbec,
				 enum vb2_firmware_selection select)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	return vboot_set_region_protection(me, select, 1);
}

static vb2_error_t vboot_battery_cutoff(VbootEcOps *vbec)
{
	if (cros_ec_battery_cutoff(EC_BATTERY_CUTOFF_FLAG_AT_SHUTDOWN) < 0)
		return VB2_ERROR_UNKNOWN;
	return VB2_SUCCESS;
}

static vb2_error_t vboot_check_limit_power(VbootEcOps *vbec, int *limit_power)
{
	if (cros_ec_read_limit_power_request(limit_power) < 0)
		return VB2_ERROR_UNKNOWN;
	return VB2_SUCCESS;
}

static vb2_error_t vboot_enable_power_button(VbootEcOps *vbec, int enable)
{
	uint32_t flags = 0;

	if (enable)
		flags = EC_POWER_BUTTON_ENABLE_PULSE;

	if (cros_ec_config_powerbtn(flags) < 0)
		return VB2_ERROR_UNKNOWN;
	return VB2_SUCCESS;
}

static vb2_error_t vboot_protect_tcpc_ports(VbootEcOps *vbec)
{
	CrosEc *cros_ec = container_of(vbec, CrosEc, vboot);

	if (!CONFIG(DRIVER_BUS_I2C_CROS_EC_TUNNEL))
		return VB2_SUCCESS;

	int ret = cros_ec_tunnel_i2c_protect_tcpc_ports(cros_ec);
	if (ret == -EC_RES_INVALID_COMMAND) {
		printf("EC does not support TCPC sync in RW... ignoring.\n");
	} else if (ret < 0) {
		printf("ERROR: cannot protect TCPC I2C tunnels\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

CrosEc *new_cros_ec(CrosEcBusOps *bus, GpioOps *interrupt_gpio)
{
	CrosEc *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->interrupt_gpio = interrupt_gpio;

	me->vboot.running_rw = vboot_running_rw;
	me->vboot.jump_to_rw = vboot_jump_to_rw;
	me->vboot.disable_jump = vboot_disable_jump;
	me->vboot.hash_image = vboot_hash_image;
	me->vboot.update_image = vboot_update_image;
	me->vboot.protect = vboot_protect;
	me->vboot.reboot_to_ro = vboot_reboot_to_ro;
	me->vboot.reboot_switch_rw = vboot_reboot_switch_rw;
	me->vboot.reboot_ap_off = vboot_reboot_ap_off;
	me->vboot.battery_cutoff = vboot_battery_cutoff;
	me->vboot.check_limit_power = vboot_check_limit_power;
	me->vboot.enable_power_button = vboot_enable_power_button;
	me->vboot.protect_tcpc_ports = vboot_protect_tcpc_ports;

	return me;
}
