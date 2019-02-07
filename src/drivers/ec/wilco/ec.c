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

#include "base/cleanup_funcs.h"
#include "base/container_of.h"
#include "drivers/ec/wilco/ec.h"

static VbError_t vboot_hash_image(VbootEcOps *vbec,
				  enum VbSelectFirmware_t select,
				  const uint8_t **hash, int *hash_size)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_update_image(VbootEcOps *vbec,
	enum VbSelectFirmware_t select, const uint8_t *image, int image_size)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_reboot_to_ro(VbootEcOps *vbec)
{
	WilcoEc *ec = container_of(vbec, WilcoEc, vboot);
	if (wilco_ec_reboot(ec) < 0)
		return VBERROR_UNKNOWN;
	return VBERROR_SUCCESS;
}

static VbError_t vboot_jump_to_rw(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_reboot_switch_rw(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_disable_jump(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_entering_mode(VbootEcOps *vbec, enum VbEcBootMode_t vbm)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_protect(VbootEcOps *vbec, enum VbSelectFirmware_t select)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_running_rw(VbootEcOps *vbec, int *in_rw)
{
	*in_rw = 0;
	return VBERROR_SUCCESS;
};

static VbError_t vboot_battery_cutoff(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static VbError_t vboot_check_limit_power(VbootEcOps *vbec, int *limit_power)
{
	*limit_power = 0;
	return VBERROR_SUCCESS;
}

static VbError_t vboot_enable_power_button(VbootEcOps *vbec, int enable)
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

	if (ec_host_base == 0 || mec_emi_base == 0 ||
	    flash_offset == 0 || flash_size == 0) {
		printf("%s: Invalid parameter\n", __func__);
		return NULL;
	}

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
