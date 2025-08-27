// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#include <stdio.h>

#include "arch/types.h"
#include "base/init_funcs.h"
#include "drivers/ec/common/pdc_utils.h"
#include "drivers/ec/cros/ec.h"

#include "tps6699x.h"
#include "tps6699x_fwup.h"

#define TPS6699X_RESTART_DELAY_US 7000000 /* 7s */
#define TPS6699X_FW_HASH_FILE_SIZE 11
#define TPS6699X_PROJECT_NAME_LENGTH 8

static void tps6699x_set_i2c_speed(Tps6699x *me)
{
	uint16_t old_speed_khz;
	int status;

	if (CONFIG_DRIVER_EC_TPS6699X_I2C_SPEED_KHZ == 0) {
		/* Feature not used. Do not adjust the bus speed. */
		printf("%s: Skipping adjusting I2C bus speed\n", me->chip_name);
		return;
	}

	status = cros_ec_i2c_set_speed(me->bus->remote_bus,
				       CONFIG_DRIVER_EC_TPS6699X_I2C_SPEED_KHZ, &old_speed_khz);
	if (status < 0) {
		printf("%s: Could not set I2C bus speed to %u kHz! (%d)\n", me->chip_name,
		       CONFIG_DRIVER_EC_TPS6699X_I2C_SPEED_KHZ, status);
		return;
	}

	if (old_speed_khz != EC_I2C_CONTROL_SPEED_UNKNOWN)
		me->saved_i2c_speed_khz = old_speed_khz;
}

static void tps6699x_restore_i2c_speed(Tps6699x *me)
{
	int status;

	if (me->saved_i2c_speed_khz == 0 || CONFIG_DRIVER_EC_TPS6699X_I2C_SPEED_KHZ == 0)
		return;

	/*
	 * some implementations support setting the I2C speed
	 * without supporting getting the current speed. only
	 * restore the speed if it was reported previously.
	 */
	status = cros_ec_i2c_set_speed(me->bus->remote_bus, me->saved_i2c_speed_khz, 0);
	if (!status)
		printf("%s: Could not restore I2C speed to %u kHz! (%d)\n", me->chip_name,
		       me->saved_i2c_speed_khz, status);
}

static int tps6699x_construct_i2c_tunnel(Tps6699x *me)
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

	printf("%s: Located chip at port %d, addr 0x%02x\n", me->chip_name, r.i2c_info.port,
	       r.i2c_info.addr_flags);

	me->bus = new_cros_ec_tunnel_i2c(r.i2c_info.port);
	me->chip_info.i2c_addr = r.i2c_info.addr_flags;

	return ret;
}

static vb2_error_t tps6699x_ec_tunnel_status(const VbootAuxfwOps *vbaux, int *protected)
{
	Tps6699x *me = container_of(vbaux, Tps6699x, fw_ops);
	int ret;

	ret = cros_ec_tunnel_i2c_protect_status(me->bus, protected);
	if (ret < 0) {
		printf("%s: Error %d getting EC I2C tunnel status.\n", me->chip_name, ret);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

/**
 * @brief Confirm the chip is present and collect FW version info via host command
 *
 * @param me Pointer to the Tps6699x struct
 *
 * @return true if chip is present
 * @return false if error occurred
 */
static bool tps6699x_query_chip_host_cmd(Tps6699x *me)
{
	struct ec_response_pd_chip_info_v2 r;
	int status;

	status = cros_ec_pd_chip_info(me->ec_pd_id, /* live= */ true, &r);
	if (status < 0) {
		printf("%s: could not get chip info (%d)\n", me->chip_name, status);
		return false;
	}

	me->chip_info.vid = r.vendor_id;
	me->chip_info.pid = r.product_id;
	me->chip_info.fw_version = (pdc_fw_ver_t)r.fw_version_number;
	memcpy(me->chip_info.fw_name_str, r.fw_name_str, TPS6699X_PROJECT_NAME_LENGTH);

	return true;
}

static bool force_update(Tps6699x *me)
{
	if (!CONFIG(DRIVER_EC_TPS6699X_FORCE_UPDATE)) {
		/* Force update is disabled */
		return false;
	}

	/* If forcing update, only do this once to avoid looping */
	if (me->has_updated) {
		printf("%s: Not forcing update because an update already happened.\n",
		       me->chip_name);
		return false;
	}

	printf("%s: Forcing an update\n", me->chip_name);
	return true;
}

static vb2_error_t tps6699x_check_hash(const VbootAuxfwOps *vbaux, const uint8_t *hash,
				       size_t hash_size,
				       enum vb2_auxfw_update_severity *severity)
{
	Tps6699x *me = container_of(vbaux, Tps6699x, fw_ops);
	pdc_fw_ver_t ver_current = me->chip_info.fw_version;
	pdc_fw_ver_t ver_new = PDC_FWVER_TO_INT(hash[0], hash[1], hash[2]);
	int ret;
	char hash_fw_name_str[TPS6699X_PROJECT_NAME_LENGTH + 1] = {0};

	if (hash_size != TPS6699X_FW_HASH_FILE_SIZE) {
		debug("%s: hash_size of %zu unexpected\n", me->chip_name, hash_size);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	memcpy(hash_fw_name_str, &hash[3], TPS6699X_PROJECT_NAME_LENGTH);
	/* Check if an update is needed (or if one is being forced)*/
	if ((ver_current == ver_new && !memcmp(hash_fw_name_str, me->chip_info.fw_name_str,
					       TPS6699X_PROJECT_NAME_LENGTH)) &&
	    !force_update(me)) {
		printf("No upgrade necessary for %s. Already at "
		       "%u.%u.%u(%s).\n",
		       me->chip_name, PDC_FWVER_MAJOR(ver_current),
		       PDC_FWVER_MINOR(ver_current), PDC_FWVER_PATCH(ver_current),
		       me->chip_info.fw_name_str);
		*severity = VB2_AUXFW_NO_UPDATE;
		return VB2_SUCCESS;
	}

	/*
	 * Prepare to perform a PDC update: shut off the EC's PD stack so it
	 * does not communicate with the PDC during updates and interfere.
	 */
	ret = cros_ec_pd_control(me->ec_pd_id, PD_SUSPEND);
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

	printf("%s: Update FW from %u.%u.%u (%s) to %u.%u.%u (%s)\n", me->chip_name,
	       PDC_FWVER_MAJOR(ver_current), PDC_FWVER_MINOR(ver_current),
	       PDC_FWVER_PATCH(ver_current), me->chip_info.fw_name_str,
	       PDC_FWVER_MAJOR(ver_new), PDC_FWVER_MINOR(ver_new), PDC_FWVER_PATCH(ver_new),
	       hash_fw_name_str);

	*severity = VB2_AUXFW_SLOW_UPDATE;
	debug("update severity %d\n", *severity);

	return VB2_SUCCESS;
}

static vb2_error_t tps6699x_update_image(const VbootAuxfwOps *vbaux, const uint8_t *image,
					 size_t image_size)
{
	Tps6699x *me = container_of(vbaux, Tps6699x, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	int protected;
	uint64_t start;
	int ret;

	debug("Updating TPS6699x image...\n");

	/* If the I2C tunnel is not known, probe EC for that */
	if (!me->bus && tps6699x_construct_i2c_tunnel(me)) {
		printf("%s: Error constructing i2c tunnel\n", me->chip_name);
		goto pd_restart;
	}

	if (tps6699x_ec_tunnel_status(vbaux, &protected) != 0)
		goto pd_restart;

	/* force reboot to RO */
	if (protected)
		return VB2_REQUEST_REBOOT_EC_TO_RO;

	if (image == NULL || image_size == 0) {
		status = VB2_ERROR_INVALID_PARAMETER;
		goto pd_restart;
	}

	me->fw_image.image = image;
	me->fw_image.size = image_size;

	tps6699x_set_i2c_speed(me);

	ret = tps6699x_perform_fw_update(me);
	if (ret == 0)
		status = VB2_SUCCESS;

	printf("%s: tps6699x_perform_fw_update return code: %d (vb2 status=%d)\n",
	       me->chip_name, ret, status);

	me->has_updated = true;

	tps6699x_restore_i2c_speed(me);

pd_restart:
	/* Re-enable the EC PD stack */
	ret = cros_ec_pd_control(me->ec_pd_id, PD_RESUME);
	if (ret) {
		printf("%s: Cannot resume PD: %d. Rebooting.\n", me->chip_name, ret);
		/* Resuming the EC PD stack failed. Reboot the EC to recover */
		return VB2_REQUEST_REBOOT_EC_TO_RO;
	}

	printf("%s: PDC control handed back to EC\n", me->chip_name);

	/* Give a generous timeout for the EC PD stack to resume */
	start = timer_us(0);
	do {
		mdelay(200);
		printf("%s: Polling for chip...\n", me->chip_name);
		if (tps6699x_query_chip_host_cmd(me)) {
			printf("%s: Chip is up. All done.\n", me->chip_name);
			return status;
		}
	} while (timer_us(start) < TPS6699X_RESTART_DELAY_US);

	printf("%s: Timed out contacting PDC\n", me->chip_name);

	/* Cannot contact PDC after timeout. Reboot the EC to recover. */
	return VB2_REQUEST_REBOOT_EC_TO_RO;
}

/* By default, output NULL to perform no update. Board code shall
 * override this function if supported.
 */
__weak void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path)
{
	*image_path = NULL;
	*hash_path = NULL;
}

Tps6699x *new_tps6699x(int ec_pd_id)
{
	VbootAuxfwOps fw_ops = {
		.check_hash = tps6699x_check_hash,
		.update_image = tps6699x_update_image,
	};

	board_tps6699x_get_image_paths(&fw_ops.fw_image_name, &fw_ops.fw_hash_name);

	if (fw_ops.fw_image_name == NULL || fw_ops.fw_hash_name == NULL) {
		/* No files, so don't perform an update */
		printf("Unknown PDC configuration. Skipping update.\n");
		return NULL;
	}

	printf("Using PDC image '%s' with hash file '%s'\n", fw_ops.fw_image_name,
	       fw_ops.fw_hash_name);

	Tps6699x *me = xzalloc(sizeof(*me));

	me->ec_pd_id = ec_pd_id;
	me->fw_ops = fw_ops;
	snprintf(me->chip_name, sizeof(me->chip_name), "tps6699x.%d", me->ec_pd_id);

	return me;
}

const VbootAuxfwOps *new_tps6699x_from_chip_info(struct ec_response_pd_chip_info_v2 *r,
						 uint8_t ec_pd_id)
{
	Tps6699x *tps6699x;

	/* Skip firmware update if fw_update_flags flag is set */
	if (r->fw_update_flags & USB_PD_CHIP_INFO_FWUP_FLAG_NO_UPDATE) {
		printf("Ignoring port%d to avoid double-upgrade\n", ec_pd_id);
		return NULL;
	}

	tps6699x = new_tps6699x(ec_pd_id);
	if (tps6699x == NULL) {
		printf("Error instantiating tps6699x driver. Skipping FW update.\n");
		return NULL;
	}

	tps6699x->chip_info.vid = r->vendor_id;
	tps6699x->chip_info.pid = r->product_id;
	tps6699x->chip_info.fw_version = (pdc_fw_ver_t)r->fw_version_number;
	memcpy(tps6699x->chip_info.fw_name_str, r->fw_name_str, TPS6699X_PROJECT_NAME_LENGTH);

	printf("TPS6699x VID/PID %04x:%04x\n", tps6699x->chip_info.vid,
	       tps6699x->chip_info.pid);
	return &tps6699x->fw_ops;
}

#define TPS6699X_CHIP_ENTRY(VID, PID)                                                          \
	{                                                                                      \
		.vid = (VID),                                                                  \
		.pid = (PID),                                                                  \
		.new_chip_aux_fw_ops = new_tps6699x_from_chip_info,                            \
	}

/** List of VIDs/PIDs used in conjunction with this chip */
static CrosEcAuxfwChipInfo aux_fw_tps6699x_info[] = {
	/* Default TI VID:PID for TPS6699x */
	TPS6699X_CHIP_ENTRY(0x0451, 0x0000),
	TPS6699X_CHIP_ENTRY(CONFIG_DRIVER_EC_TPS6699X_VID, CONFIG_DRIVER_EC_TPS6699X_PID),
};

__weak void board_tps6699x_register(void)
{
}

static int tps6699x_register(void)
{
	for (int i = 0; i < ARRAY_SIZE(aux_fw_tps6699x_info); i++)
		list_insert_after(&aux_fw_tps6699x_info[i].list_node, &ec_aux_fw_chip_list);

	/* Override this function in board level if additional PIDs need to be included. */
	board_tps6699x_register();
	return 0;
}

INIT_FUNC(tps6699x_register);
