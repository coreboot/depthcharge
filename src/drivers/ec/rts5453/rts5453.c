// SPDX-License-Identifier: GPL-2.0
/* Copyright 2023 Google LLC.  */

#include <libpayload.h>
#include <inttypes.h>
#include <vb2_api.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

#include "rts5453.h"
#include "rts5453_internal.h"

#define GOOGLE_BROX_VENDOR_ID 0x18d1
#define GOOGLE_BROX_PRODUCT_ID 0x5065

#define RTS545X_I2C_ADDR 0x67
#define RTS_I2C_WINDOW_SPEED_KHZ 400
#define FW_MAJOR_VERSION_SHIFT 16
#define FW_MINOR_VERSION_SHIFT 8
#define FW_PATCH_VERSION_SHIFT 0
#define FW_HASH_SIZE 3

#define PING_STATUS_DELAY_MS 10
#define PING_STATUS_TIMEOUT_US 2000000 /* 2s */
#define PING_STATUS_MASK 0x3
#define PING_STATUS_COMPLETE 0x1
#define PING_STATUS_INVALID_FMT 0x3
#define RTS_RESTART_DELAY_US 7000000 /* 7s */

#define MAX_COMMAND_SIZE 32
#define FW_CHUNK_SIZE 29
#define FLASH_SEGMENT_SIZE (64 * 1024)

#define RTS545X_VENDOR_CMD 0x01
#define RTS545X_FLASH_ERASE_CMD 0x03
#define RTS545X_FLASH_WRITE_0_64K_CMD 0x04
#define RTS545X_RESET_TO_FLASH_CMD 0x05
#define RTS545X_FLASH_WRITE_64K_128K_CMD 0x06
#define RTS545X_FLASH_WRITE_128K_192K_CMD 0x13
#define RTS545X_FLASH_WRITE_192K_256K_CMD 0x14
#define RTS545X_VALIDATE_ISP_CMD 0x16
#define RTS545X_GET_IC_STATUS_CMD 0x3A
#define RTS545X_READ_DATA_CMD 0x80

#define ETIMEDOUT 10
#define EINVAL 11

#define RTS545X_DEBUG 0

#if (RTS545X_DEBUG > 0)
#define debug(msg, args...) printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

enum flash_write_cmd_off {
	ADDR_L_OFF,
	ADDR_H_OFF,
	DATA_COUNT_OFF,
	DATA_OFF,
};

struct rts5453_ic_status {
	uint8_t byte_count;
	uint8_t code_location;
	uint16_t reserved_0;
	uint8_t major_version;
	uint8_t minor_version;
	uint8_t patch_version;
	uint16_t reserved_1;
	uint8_t pd_typec_status;
	uint8_t vid_pid[4];
	uint8_t reserved_2;
	uint8_t flash_bank;
	uint8_t reserved_3[16];
} __attribute__((__packed__));
static struct rts5453_ic_status ic_status;

typedef struct Rts545x {
	VbootAuxfwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;

	/* these are cached from chip regs */
	struct {
		uint16_t vid;
		uint16_t pid;
	} chip_info;

	struct {
		uint8_t major_ver;
		uint8_t minor_ver;
		uint8_t patch_ver;
		uint8_t location;
		uint8_t flash_bank;
	} fw_info;
	char chip_name[16];
	uint16_t saved_i2c_speed_khz;
} Rts545x;

static int rts545x_ping_status(Rts545x *me, uint8_t *status_byte)
{
	uint64_t start = timer_us(0);
	int ret;

	while (timer_us(start) < PING_STATUS_TIMEOUT_US) {
		ret = i2c_read_raw(&me->bus->ops, RTS545X_I2C_ADDR, status_byte, 1);
		if (ret < 0) {
			printf("%s: Error %d reading ping_status\n", me->chip_name, ret);
			return ret;
		}

		/* Command execution is complete */
		if ((*status_byte & PING_STATUS_MASK) == PING_STATUS_COMPLETE)
			return 0;

		/* Invalid command format */
		if ((*status_byte & PING_STATUS_MASK) == PING_STATUS_INVALID_FMT)
			return -EINVAL;

		mdelay(PING_STATUS_DELAY_MS);
	}
	return -ETIMEDOUT;
}

static int rts545x_block_out_transfer(Rts545x *me, uint8_t cmd_code, size_t len,
				      const uint8_t *write_data, uint8_t *status_byte)
{
	int ret;
	/* Command byte + Byte Count + Data[0..31] */
	uint8_t write_buf[MAX_COMMAND_SIZE + 2];

	if ((len + 2) > ARRAY_SIZE(write_buf))
		return -EINVAL;

	write_buf[0] = cmd_code;
	write_buf[1] = len;
	memcpy(&write_buf[2], write_data, len);

	ret = i2c_write_raw(&me->bus->ops, RTS545X_I2C_ADDR, write_buf, len + 2);
	if (ret < 0) {
		printf("%s: Error %d sending command %#02x\n", me->chip_name, ret, cmd_code);
		return ret;
	}

	if (cmd_code == RTS545X_RESET_TO_FLASH_CMD)
		return 0;

	return rts545x_ping_status(me, status_byte);
}

static int rts545x_block_in_transfer(Rts545x *me, size_t len, uint8_t *read_data)
{
	return i2c_readblock(&me->bus->ops, RTS545X_I2C_ADDR, RTS545X_READ_DATA_CMD, read_data,
			     len);
}

static int rts545x_vendor_cmd_enable(Rts545x *me)
{
	uint8_t vendor_cmd_enable[] = {0xDA, 0x0B, 0x01};
	uint8_t ping_status;
	int ret;

	ret = rts545x_block_out_transfer(me, RTS545X_VENDOR_CMD, ARRAY_SIZE(vendor_cmd_enable),
					 vendor_cmd_enable, &ping_status);
	if (ret)
		printf("%s failed: %d\n", __func__, ret);
	return ret;
}

static int rts545x_get_ic_status(Rts545x *me, struct rts5453_ic_status *ic_sts)
{
	uint8_t get_ic_status[] = {0x00, 0x00, sizeof(*ic_sts) - 1};
	uint8_t ping_status;
	int ret;

	ret = rts545x_block_out_transfer(me, RTS545X_GET_IC_STATUS_CMD,
					 ARRAY_SIZE(get_ic_status), get_ic_status,
					 &ping_status);
	if (ret) {
		printf("%s failed: %d\n", __func__, ret);
		return ret;
	}

	ret = rts545x_block_in_transfer(me, sizeof(*ic_sts), (uint8_t *)ic_sts);
	if (ret) {
		printf("%s failed: %d reading IC_STATUS\n", __func__, ret);
		return ret;
	}

	printf("%s: VID:PID %x%x:%x%x, FW_Ver %u.%u.%u, %s Bank:%d\n", __func__,
	       ic_sts->vid_pid[1], ic_sts->vid_pid[0], ic_sts->vid_pid[3], ic_sts->vid_pid[2],
	       ic_sts->major_version, ic_sts->minor_version, ic_sts->patch_version,
	       ic_sts->code_location ? "Flash" : "ROM", ic_sts->flash_bank ? 1 : 0);
	return 0;
}

static int rts545x_flash_access_enable(Rts545x *me)
{
	uint8_t flash_access_enable[] = {0xDA, 0x0B, 0x03};
	uint8_t ping_status;
	int ret;

	ret = rts545x_block_out_transfer(me, RTS545X_VENDOR_CMD,
					 ARRAY_SIZE(flash_access_enable), flash_access_enable,
					 &ping_status);
	if (ret)
		printf("%s failed: %d\n", __func__, ret);

	return ret;
}

static int rts545x_flash_write(Rts545x *me, const uint8_t *image, size_t image_size)
{
	uint8_t flash_write[MAX_COMMAND_SIZE] = {0};
	uint8_t bank0_write_cmds[] = {RTS545X_FLASH_WRITE_0_64K_CMD,
				      RTS545X_FLASH_WRITE_64K_128K_CMD};
	uint8_t bank1_write_cmds[] = {RTS545X_FLASH_WRITE_128K_192K_CMD,
				      RTS545X_FLASH_WRITE_192K_256K_CMD};
	uint8_t *flash_write_cmds;
	uint32_t seg_boundary = FLASH_SEGMENT_SIZE;
	uint8_t cmd, ping_status, segment, size = 0;
	uint32_t offset = 0;
	uint32_t progress_counter = 0;
	int ret = 0;

	/* If running ROM code or flash_bank 1, program flash_bank 0 */
	if (!ic_status.code_location || ic_status.flash_bank)
		flash_write_cmds = bank0_write_cmds;
	else
		flash_write_cmds = bank1_write_cmds;

	uint64_t start = timer_us(0);

	while (offset < image_size) {
		segment = offset < FLASH_SEGMENT_SIZE ? 0 : 1;
		seg_boundary = (segment + 1) * FLASH_SEGMENT_SIZE;
		cmd = flash_write_cmds[segment];

		size = MIN(FW_CHUNK_SIZE, MIN(seg_boundary, image_size) - offset);
		flash_write[ADDR_L_OFF] = (uint8_t)(offset & 0xff);
		flash_write[ADDR_H_OFF] = (uint8_t)((offset >> 8) & 0xff);
		flash_write[DATA_COUNT_OFF] = size;
		memcpy(&flash_write[DATA_OFF], &image[offset], size);

		/* Account for ADDR_L, ADDR_H, Write Data Count */
		ret = rts545x_block_out_transfer(me, cmd, size + 3, flash_write, &ping_status);
		if (ret) {
			printf("%s: failed(%d) @off:0x%x\n", __func__, ret, offset);
			break;
		}
		offset += size;
		progress_counter += size;

		if (progress_counter >= 4000) {
			/* Prints an update every 4000 bytes transferred */
			printf("%s: Progress: %u / %zu (%llu ms)\n", __func__, offset,
			       image_size, timer_us(start) / 1000);
			progress_counter = 0;
		}
	}

	return ret;
}

static int rts545x_flash_access_disable(Rts545x *me)
{
	return rts545x_vendor_cmd_enable(me);
}

static int rts545x_validate_firmware(Rts545x *me, const uint8_t *image, size_t image_size)
{
	uint8_t validate_isp[] = {0x01};
	uint8_t ping_status;
	int ret;

	/* ROM Code does not support validate_isp command */
	if (!ic_status.code_location)
		return 0;

	ret = rts545x_block_out_transfer(me, RTS545X_VALIDATE_ISP_CMD, ARRAY_SIZE(validate_isp),
					 validate_isp, &ping_status);
	if (ret)
		printf("%s: failed: %d", __func__, ret);
	return ret;
}

static int rts545x_reset_to_flash(Rts545x *me)
{
	uint8_t reset_to_flash[] = {0xDA, 0x0B, 0x01};
	uint8_t ping_status;
	int ret;

	ret = rts545x_block_out_transfer(me, RTS545X_RESET_TO_FLASH_CMD,
					 ARRAY_SIZE(reset_to_flash), reset_to_flash,
					 &ping_status);
	if (ret)
		printf("%s failed: %d\n", __func__, ret);

	/* Programming sequence recommends 5 second delay for reset */
	mdelay(5000);
	return ret;
}

static int rts545x_confirm_flash_update(Rts545x *me)
{
	int ret;
	struct rts5453_ic_status new_ic_status;
	uint8_t exp_flash_bank =
		ic_status.code_location == 0 ? 0x00 : (ic_status.flash_bank ^ 0x10);

	ret = rts545x_vendor_cmd_enable(me);
	if (ret)
		return ret;

	ret = rts545x_get_ic_status(me, &new_ic_status);
	if (ret)
		return ret;

	if (new_ic_status.flash_bank != exp_flash_bank) {
		printf("%s: failed. Exp %#02x != Actual %#02x\n", __func__, exp_flash_bank,
		       new_ic_status.flash_bank);
		return -1;
	}
	return 0;
}

static int rts545x_construct_i2c_tunnel(Rts545x *me)
{
	int ret;
	struct ec_response_locate_chip r;

	ret = cros_ec_locate_pdc_chip(me->ec_pd_id, &r);
	if (ret)
		return ret;

	if (r.bus_type != EC_BUS_TYPE_I2C) {
		printf("%s: Unexpected bus %d for port %d\n", me->chip_name, r.bus_type,
		       me->ec_pd_id);
		return -1;
	}

	printf("%s: Located chip at port %d, addr %02x", me->chip_name, r.i2c_info.port,
	       r.i2c_info.addr_flags);

	me->bus = new_cros_ec_tunnel_i2c(r.i2c_info.port);
	return ret;
}

static vb2_error_t rts545x_ec_tunnel_status(const VbootAuxfwOps *vbaux, int *protected)
{
	Rts545x *me = container_of(vbaux, Rts545x, fw_ops);
	int ret;

	ret = cros_ec_tunnel_i2c_protect_status(me->bus, protected);
	if (ret < 0) {
		printf("%s: Error %d getting EC I2C tunnel status.\n", me->chip_name, ret);
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

static int rts545x_set_i2c_speed(Rts545x *me)
{
	uint16_t old_speed_khz;
	int status;

	status = cros_ec_i2c_set_speed(me->bus->remote_bus, RTS_I2C_WINDOW_SPEED_KHZ,
				       &old_speed_khz);
	if (status < 0) {
		printf("%s: Could not set I2C bus speed to %u kHz!\n", me->chip_name,
		       RTS_I2C_WINDOW_SPEED_KHZ);
		return status;
	}
	if (old_speed_khz != EC_I2C_CONTROL_SPEED_UNKNOWN)
		me->saved_i2c_speed_khz = old_speed_khz;

	return 0;
}

static int rts545x_restore_i2c_speed(Rts545x *me)
{
	int status;

	if (me->saved_i2c_speed_khz != 0) {
		/*
		 * some implementations support setting the I2C speed
		 * without supporting getting the current speed. only
		 * restore the speed if it was reported previously.
		 */
		status = cros_ec_i2c_set_speed(me->bus->remote_bus, me->saved_i2c_speed_khz, 0);
		if (status != 0) {
			printf("%s: Could not restore I2C speed to %u kHz!\n", me->chip_name,
			       me->saved_i2c_speed_khz);
			return status;
		}
	}

	return 0;
}

static bool is_rts545x_device_present(Rts545x *me, int live)
{
	struct ec_response_pd_chip_info r;
	int status;

	if (!me->chip_info.vid && !live)
		return true;

	status = cros_ec_pd_chip_info(me->ec_pd_id, live, &r);
	if (status < 0) {
		printf("%s: could not get chip info\n", me->chip_name);
		return false;
	}

	if (me->chip_info.vid != r.vendor_id || me->chip_info.pid != r.product_id) {
		printf("%s: VID/PID mismatch Expected(%04x:%04x) != Live(%04x:%04x)\n",
		       me->chip_name, me->chip_info.vid, me->chip_info.pid, r.vendor_id,
		       r.product_id);
		return false;
	}

	me->fw_info.major_ver = (r.fw_version_number >> FW_MAJOR_VERSION_SHIFT) & 0xff;
	me->fw_info.minor_ver = (r.fw_version_number >> FW_MINOR_VERSION_SHIFT) & 0xff;
	me->fw_info.patch_ver = (r.fw_version_number >> FW_PATCH_VERSION_SHIFT) & 0xff;
	return true;
}

/**
 * @brief Check if the new PDC FW version is compatible with the currently running PDC FW
 *
 * @param current FW version currently installed on the PDC
 * @param new FW version to be flashed onto the PDC
 * @return true The update may proceed
 * @return false If the update is incompatible and should NOT proceed
 */
bool rts545x_check_update_compatibility(pdc_fw_ver_t current, pdc_fw_ver_t new)
{
	switch (PDC_FWVER_MAJOR(new)) {
	case PDC_FWVER_TYPE_BASE:
		/* We do not flash base FWs through Depthcharge */
		printf("Upgrading to base FW (0x%06x) not supported\n", new);
		return false;
	case PDC_FWVER_TYPE_TEST:
		if (new <= PDC_FWVER_TO_INT(1, 22, 1)) {
			/* Versions up to and including 1.22 use the legacy
			 * proj naming scheme and can always be flashed.
			 */
			return true;
		}
		/* new > PDC_FWVER_TO_INT(1, 22, 1) */

		/* Versions after 1.22 have the new config format
		 * and will only work if current image uses prefix-based
		 * proj name verification or no proj name verification.
		 *
		 * Break this check down by the current firmware type.
		 */
		switch (PDC_FWVER_MAJOR(current)) {
		case PDC_FWVER_TYPE_BASE:
			return current == PDC_FWVER_TO_INT(2, 0, 1) ||
			       current == PDC_FWVER_TO_INT(2, 2, 1);
		case PDC_FWVER_TYPE_TEST:
			return current > PDC_FWVER_TO_INT(1, 22, 1);
		case PDC_FWVER_TYPE_RELEASE:
			return current >= PDC_FWVER_TO_INT(0, 20, 1);
		}

		/* All other cases are invalid */
		printf("Cannot parse current FW version 0x%06x (new FW is test)\n", current);
		break;

	case PDC_FWVER_TYPE_RELEASE:
		if (new <= PDC_FWVER_TO_INT(0, 20, 1)) {
			/* Versions up to and including 0.20 use the legacy
			 * proj naming scheme and can always be flashed.
			 */
			return true;
		}
		/* new > PDC_FWVER_TO_INT(0, 20, 1) */

		/* Versions after 0.20 have the new config format
		 * and will only work if current image uses prefix-based
		 * proj name verification or no proj name verification.
		 *
		 * Break this check down by the current firmware type.
		 */
		switch (PDC_FWVER_MAJOR(current)) {
		case PDC_FWVER_TYPE_BASE:
			return current == PDC_FWVER_TO_INT(2, 0, 1) ||
			       current == PDC_FWVER_TO_INT(2, 2, 1);
		case PDC_FWVER_TYPE_TEST:
			return current > PDC_FWVER_TO_INT(1, 22, 1);
		case PDC_FWVER_TYPE_RELEASE:
			return current >= PDC_FWVER_TO_INT(0, 20, 1);
		}

		/* All other cases are invalid */
		printf("Cannot parse current FW version 0x%06x (new FW is release)\n", current);
		break;

	default:
		printf("Cannot parse new FW version 0x%06x\n", new);
		return false;
	}

	/* Other upgrade paths are not supported. */
	printf("Cannot match an upgrade path from 0x%06x to 0x%06x\n", current, new);
	return false;
}

/*
 * Reading the firmware takes long time (~15-20 s), so we'll just use the
 * firmware rev as a trivial hash.
 */
static vb2_error_t rts5453_check_hash(const VbootAuxfwOps *vbaux, const uint8_t *hash,
				      size_t hash_size,
				      enum vb2_auxfw_update_severity *severity)
{
	Rts545x *me = container_of(vbaux, Rts545x, fw_ops);
	pdc_fw_ver_t ver_current = PDC_FWVER_TO_INT(
		me->fw_info.major_ver, me->fw_info.minor_ver, me->fw_info.patch_ver);
	pdc_fw_ver_t ver_new = PDC_FWVER_TO_INT(hash[0], hash[1], hash[2]);
	bool dev_is_present;
	int ret;

	if (hash_size != FW_HASH_SIZE) {
		debug("hash_size %zu unexpected\n", hash_size);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	dev_is_present = is_rts545x_device_present(me, false);
	if (!dev_is_present) {
		*severity = VB2_AUXFW_NO_DEVICE;
		printf("Skipping upgrade for %s. Device not present.\n", me->chip_name);
		return VB2_SUCCESS;
	}

	if (ver_current == ver_new) {
		printf("No upgrade necessary for %s. Already at %u.%u.%u.\n", me->chip_name,
		       PDC_FWVER_MAJOR(ver_current), PDC_FWVER_MINOR(ver_current),
		       PDC_FWVER_PATCH(ver_current));
		*severity = VB2_AUXFW_NO_UPDATE;
		return VB2_SUCCESS;
	}

	if (!rts545x_check_update_compatibility(ver_current, ver_new)) {
		printf("%s: Update FW from %u.%u.%u to %u.%u.%u not supported! Skipping.\n",
		       me->chip_name, PDC_FWVER_MAJOR(ver_current),
		       PDC_FWVER_MINOR(ver_current), PDC_FWVER_PATCH(ver_current),
		       PDC_FWVER_MAJOR(ver_new), PDC_FWVER_MINOR(ver_new),
		       PDC_FWVER_PATCH(ver_new));
		*severity = VB2_AUXFW_NO_UPDATE;
		return VB2_SUCCESS;
	}

	/* Prepare to perform a PDC update: shut off the EC's PD stack so it
	 * does not communicate with the PDC during updates and interfere.
	 */

	ret = cros_ec_pd_control(0, PD_SUSPEND);
	switch (ret) {
	case EC_RES_SUCCESS:
		/* PD stack is suspended. Safe to proceed. */
		break;
	case -EC_RES_BUSY:
		/* EC power state or battery not ready for update */
		printf("Skipping update: Battery SoC or power state inadequate: %d\n", ret);
		*severity = VB2_AUXFW_NO_UPDATE;
		return VB2_SUCCESS;
	default:
		/* Unknown error */
		printf("Skipping update: Error suspending PD stack: %d\n", ret);
		*severity = VB2_AUXFW_NO_DEVICE;
		return VB2_SUCCESS;
	}

	printf("%s: Update FW from %u.%u.%u to %u.%u.%u\n", me->chip_name,
	       PDC_FWVER_MAJOR(ver_current), PDC_FWVER_MINOR(ver_current),
	       PDC_FWVER_PATCH(ver_current), PDC_FWVER_MAJOR(ver_new), PDC_FWVER_MINOR(ver_new),
	       PDC_FWVER_PATCH(ver_new));
	*severity = VB2_AUXFW_SLOW_UPDATE;
	debug("update severity %d\n", *severity);
	return VB2_SUCCESS;
}

static int rts545x_update_flash(Rts545x *me, const uint8_t *image, size_t image_size)
{
	int ret;

	/* Follow RTS545x ISP Flow procedure. Note that the ISP flow for ROM to
	 * MC is not currently supported.
	 */

	printf("%s: Vendor command enable... ", __func__);
	ret = rts545x_vendor_cmd_enable(me);
	if (ret) {
		printf("fail (%d)\n", ret);
		return ret;
	}
	printf("success\n");

	ret = rts545x_get_ic_status(me, &ic_status);
	if (ret) {
		printf("%s: IC status failed (%d)\n", __func__, ret);
		return ret;
	}
	printf("%s: Got IC status\n", __func__);

	printf("%s: Flash access enable... ", __func__);
	ret = rts545x_flash_access_enable(me);
	if (ret) {
		printf("fail (%d)\n", ret);
		return ret;
	}
	printf("success\n");

	/* TODO(b/323608798): Add flash unlock step support */

	printf("%s: Starting flash write\n", __func__);
	ret = rts545x_flash_write(me, image, image_size);
	if (ret) {
		printf("%s: Failed during flash write", __func__);
		rts545x_flash_access_disable(me);
		return ret;
	}
	printf("%s: Completed flash write\n", __func__);

	printf("%s: Flash access disable... ", __func__);
	ret = rts545x_flash_access_disable(me);
	if (ret) {
		printf("fail (%d)\n", ret);
		return ret;
	}
	printf("success\n");

	printf("%s: Validate FW... ", __func__);
	ret = rts545x_validate_firmware(me, image, image_size);
	if (ret) {
		printf("fail (%d)\n", ret);
		return ret;
	}
	printf("success\n");

	printf("%s: Reset to flash... ", __func__);
	ret = rts545x_reset_to_flash(me);
	if (ret) {
		printf("fail (%d)\n", ret);
		return ret;
	}
	printf("success\n");

	printf("%s: Confirm update... ", __func__);
	ret = rts545x_confirm_flash_update(me);
	if (ret) {
		printf("fail (%d)\n", ret);
		return ret;
	}
	printf("success\n");

	return 0;
}

static vb2_error_t rts5453_update_image(const VbootAuxfwOps *vbaux, const uint8_t *image,
					size_t image_size)
{
	Rts545x *me = container_of(vbaux, Rts545x, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	int protected;
	uint64_t start;
	int ret;

	debug("Updating RTS5453 image...\n");

	/* If the I2C tunnel is not known, probe EC for that */
	if (!me->bus && rts545x_construct_i2c_tunnel(me)) {
		printf("%s: Error constructing i2c tunnel\n", me->chip_name);
		goto pd_restart;
	}

	if (rts545x_ec_tunnel_status(vbaux, &protected) != 0)
		goto pd_restart;

	/* force reboot to RO */
	if (protected)
		return VB2_REQUEST_REBOOT_EC_TO_RO;

	if (image == NULL || image_size == 0) {
		status = VB2_ERROR_INVALID_PARAMETER;
		goto pd_restart;
	}

	rts545x_set_i2c_speed(me);

	if (rts545x_update_flash(me, image, image_size) == 0)
		status = VB2_SUCCESS;

	rts545x_restore_i2c_speed(me);

pd_restart:
	/* Re-enable the EC PD stack */
	ret = cros_ec_pd_control(0, PD_RESUME);
	if (ret) {
		printf("Cannot resume PD: %d. Rebooting.\n", ret);
		/* Resuming the EC PD stack failed. Reboot the EC to recover */
		return VB2_REQUEST_REBOOT_EC_TO_RO;
	}

	/* Give a generous timeout for the EC PD stack to resume */
	start = timer_us(0);
	do {
		mdelay(200);
		if (is_rts545x_device_present(me, true))
			return status;
	} while (timer_us(start) < RTS_RESTART_DELAY_US);

	/* Cannot contact PDC after timeout. Reboot the EC to recover. */
	return VB2_REQUEST_REBOOT_EC_TO_RO;
}

/* By default, output NULL to perform no update. Board code shall
 * override this function if supported.
 */
__weak void board_rts5453_get_image_paths(const char **image_path, const char **hash_path)
{
	*image_path = NULL;
	*hash_path = NULL;
}

Rts545x *new_rts5453(CrosECTunnelI2c *bus, int ec_pd_id)
{
	VbootAuxfwOps fw_ops = {
		.check_hash = rts5453_check_hash,
		.update_image = rts5453_update_image,
	};

	board_rts5453_get_image_paths(&fw_ops.fw_image_name, &fw_ops.fw_hash_name);

	if (fw_ops.fw_image_name == NULL || fw_ops.fw_hash_name == NULL) {
		/* No files, so don't perform an update */
		printf("Unknown PDC configuration. Skipping update.\n");
		return NULL;
	}

	printf("Using PDC image '%s' with hash file '%s'\n", fw_ops.fw_image_name,
	       fw_ops.fw_hash_name);

	Rts545x *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = fw_ops;
	snprintf(me->chip_name, sizeof(me->chip_name), "rts5453.%d", ec_pd_id);

	return me;
}

static const VbootAuxfwOps *new_rts545x_from_chip_info(struct ec_response_pd_chip_info *r,
						       uint8_t ec_pd_id)
{
	Rts545x *rts545x;

	/* Ignore the 2nd port so we don't perform the update twice.
	 * TODO: b/324892496 Remove this when a better mechanism is
	 * identified.
	 */
	if (ec_pd_id > 0) {
		printf("Ignoring ports > 0 to avoid double-upgrade\n");
		return NULL;
	}

	switch (r->product_id) {
	case GOOGLE_BROX_PRODUCT_ID:
		rts545x = new_rts5453(NULL, ec_pd_id);
		if (rts545x == NULL) {
			printf("Error instantiating RTS5453 driver. Skipping FW update.\n");
			return NULL;
		}
		break;
	default:
		return NULL;
	}
	rts545x->chip_info.vid = r->vendor_id;
	rts545x->chip_info.pid = r->product_id;
	rts545x->fw_info.major_ver = (r->fw_version_number >> FW_MAJOR_VERSION_SHIFT) & 0xff;
	rts545x->fw_info.minor_ver = (r->fw_version_number >> FW_MINOR_VERSION_SHIFT) & 0xff;
	rts545x->fw_info.patch_ver = (r->fw_version_number >> FW_PATCH_VERSION_SHIFT) & 0xff;
	printf("RTS5453 VID/PID %04x:%04x\n", rts545x->chip_info.vid, rts545x->chip_info.pid);
	return &rts545x->fw_ops;
}

static CrosEcAuxfwChipInfo aux_fw_rts5453_info = {
	.vid = GOOGLE_BROX_VENDOR_ID,
	.pid = GOOGLE_BROX_PRODUCT_ID,
	.new_chip_aux_fw_ops = new_rts545x_from_chip_info,
};

static int rts545x_register(void)
{
	list_insert_after(&aux_fw_rts5453_info.list_node, &ec_aux_fw_chip_list);
	return 0;
}

INIT_FUNC(rts545x_register);
