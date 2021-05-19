// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <cbfs.h>
#include <endian.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "base/container_of.h"
#include "base/init_funcs.h"
#include "drivers/ec/ccgxxf/ccgxxf.h"
#include "drivers/flash/cbfs.h"

static inline int ccgxxf_i2c_readw(I2cOps *ops, uint8_t reg, uint16_t *data)
{
	int rv;

	rv = i2c_readw(ops, CCGXXF_I2C_ADDR1_FLAGS, reg, data);
	if (!rv)
		*data = be16toh(*data);
	return rv;
}

static inline int ccgxxf_i2c_writew(I2cOps *ops, uint8_t reg, uint16_t data)
{
	return i2c_writew(ops, CCGXXF_I2C_ADDR1_FLAGS, reg, htobe16(data));
}

static inline int ccgxxf_i2c_write_block(I2cOps *ops, uint8_t reg,
					 const uint8_t *block)
{
	return i2c_writeblock(ops, CCGXXF_I2C_ADDR1_FLAGS, reg,
				block, CCGXXF_FWU_BUFFER_ROW_SIZE);
}

static int ccgxxf_get_fwu_status_reg(I2cOps *ops, uint16_t *status)
{
	int rv, i;

	for (i = 0; i < CCGXXF_FWU_RES_RETRY; i++) {
		/*
		 * This function is called immediately after issuing command
		 * to CCGXXF hence wait for 5ms before checking the status.
		 */
		mdelay(CCGXXF_FWU_STATUS_DELAY_MS);

		rv = ccgxxf_i2c_readw(ops, CCGXXF_REG_FWU_RESPONSE, status);
		if (rv)
			return rv;
		if (*status == CCGXXF_FWU_RES_CMD_FAILED)
			return VB2_ERROR_UNKNOWN;
		if (*status == CCGXXF_FWU_RES_CMD_IN_PROGRESS)
			continue;
		return VB2_SUCCESS;
	}
	return VB2_ERROR_UNKNOWN;
}

static int ccgxxf_update_image_block(I2cOps *ops, const uint8_t *image,
	size_t image_size)
{
	int i, rv, row_size;

	row_size = image_size / CCGXXF_FWU_BUFFER_ROW_SIZE;

	for (i = 0; i < row_size; i++) {
		/* Write the data buffer */
		rv = ccgxxf_i2c_write_block(ops, CCGXXF_REG_FWU_BUFFER,
				&image[i * CCGXXF_FWU_BUFFER_ROW_SIZE]);
		if (rv)
			return rv;

		/*
		 * Last buffer is metadata hence don't flash it but save it in
		 * buffer for verification.
		 */
		if (i < row_size - 1) {
			/* initiate flash write */
			rv = ccgxxf_i2c_writew(ops, CCGXXF_REG_FWU_COMMAND,
					CCGXXF_FWU_CMD_WRITE_FLASH_ROW);
			if (rv)
				return rv;

			/* Wait for internal programming cycle to complete */
			mdelay(CCGXXF_FWU_PROG_CYCLE_DELAY_MS);
		}
	}

	return VB2_SUCCESS;
}

static int ccgxxf_construct_i2c_tunnel(Ccgxxf *me)
{
	int rv;
	struct ec_response_locate_chip r;

	rv = cros_ec_locate_tcpc_chip(me->ec_pd_id, &r);
	if (rv)
		return rv;

	if (r.bus_type != EC_BUS_TYPE_I2C) {
		printf("%s: Unexpected bus %d for port %d\n",
			me->chip_name, r.bus_type, me->ec_pd_id);

		return VB2_ERROR_UNKNOWN;
	}

	if (r.i2c_info.addr_flags != CCGXXF_I2C_ADDR1_FLAGS &&
		r.i2c_info.addr_flags != CCGXXF_I2C_ADDR2_FLAGS) {
		printf("%s: Unexpected addr 0x%02x for port %d\n",
			me->chip_name, r.i2c_info.addr_flags, me->ec_pd_id);

		return VB2_ERROR_UNKNOWN;
	}
	me->bus = new_cros_ec_tunnel_i2c(cros_ec_get(), r.i2c_info.port);

	return VB2_SUCCESS;
}

static vb2_error_t ccgxxf_update_image(const VbootAuxfwOps *vboot,
	const uint8_t *image, size_t image_size)
{
	Ccgxxf *me = container_of(vboot, Ccgxxf, fw_ops);
	uint8_t new_fw_build, new_fw_major, new_fw_minor;
	uint8_t fw_build, fw_ver_major, fw_ver_minor;
	uint16_t read_val = 0;
	int rv;

	/* Make sure image has valid size */
	if ((image_size < CCGXXF_BUILD_VER_MAJOR_OFFSET) ||
		(image_size % CCGXXF_FWU_BUFFER_ROW_SIZE) != 0) {
		printf("Invalid image size\n");
		return VB2_ERROR_UNKNOWN;
	}

	/* Get the F/W version info from the device */
	rv = ccgxxf_i2c_readw(&me->bus->ops, CCGXXF_REG_FW_VERSION, &read_val);
	if (rv)
		return rv;
	fw_ver_major = (read_val >> 8) & 0x00FF;
	fw_ver_minor = read_val & 0x00FF;

	/* Get the F/W build info from the device */
	rv = ccgxxf_i2c_readw(&me->bus->ops, CCGXXF_REG_FW_VERSION_BUILD,
				&read_val);
	if (rv)
		return rv;
	fw_build = read_val & 0x00FF;

	printf("FW Version from device: %d.%d.%d\n",
			fw_build, fw_ver_major, fw_ver_minor);

	/* Get the F/W version & build info from the binary */
	new_fw_build = image[CCGXXF_BUILD_NUM_OFFSET];
	new_fw_major = image[CCGXXF_BUILD_VER_MAJOR_OFFSET];
	new_fw_minor = image[CCGXXF_BUILD_VER_MINOR_OFFSET];
	printf("FW Version from new image: %d.%d.%d\n",
			new_fw_build, new_fw_major, new_fw_minor);

	/* Nothing to do if the FW version of the binary is same as device */
	if (fw_ver_major ==  new_fw_major &&
		fw_ver_minor == new_fw_minor &&
		fw_build == new_fw_build) {
		printf("Nothing to do: FW is latest version\n");
		return VB2_SUCCESS;
	}

	/* Enable Firmware Upgrade Mode */
	rv = ccgxxf_i2c_writew(&me->bus->ops, CCGXXF_REG_FWU_COMMAND,
				CCGXXF_FWU_CMD_ENABLE);
	if (rv)
		return rv;

	rv = ccgxxf_get_fwu_status_reg(&me->bus->ops, &read_val);
	if (rv)
		goto disable_fw_upgd;

	if (read_val != CCGXXF_FWU_RES_SUCCESS) {
		rv = VB2_ERROR_UNKNOWN;
		goto disable_fw_upgd;
	}

	/* Update the firmware region */
	rv = ccgxxf_update_image_block(&me->bus->ops, image, image_size);
	if (rv)
		goto disable_fw_upgd;

	/* Validate f/w region */
	rv = ccgxxf_i2c_writew(&me->bus->ops, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_VALIDATE_FW_IMG);
	if (rv)
		goto disable_fw_upgd;

	/*
	 * Wait for 50ms to allow device to validate the updated
	 * firmare internally.
	 */
	mdelay(CCGXXF_FWU_VALIDATE_DELAY_MS);
	rv = ccgxxf_get_fwu_status_reg(&me->bus->ops, &read_val);
	if (rv)
		goto disable_fw_upgd;

	if (read_val != CCGXXF_FWU_RES_SUCCESS)
		rv = VB2_ERROR_UNKNOWN;

disable_fw_upgd:
	/* Disable Firmware Upgrade Mode */
	rv |= ccgxxf_i2c_writew(&me->bus->ops, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_DISABLE);

	/* Reset the chip */
	rv |= ccgxxf_i2c_writew(&me->bus->ops, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_RESET);

	printf("CCGXXF firmware upgrade %s\n", rv ? "failed" : "success");
	return rv;
}

Ccgxxf *get_ccgxxf_fwOps(CrosECTunnelI2c *bus, int ec_pd_id,
	const VbootAuxfwOps *image1, const VbootAuxfwOps *image2)
{
	Ccgxxf *me = xzalloc(sizeof(*me));
	uint16_t read_val;
	int rv;

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;

	/* If the I2C tunnel is not known, probe EC for that */
	if (!me->bus && ccgxxf_construct_i2c_tunnel(me)) {
		printf("%s: Error constructing i2c tunnel\n", me->chip_name);
		goto error_handler;
	}

	/* Make sure chip has correct vendor ID */
	rv = ccgxxf_i2c_readw(&me->bus->ops, CCGXXF_REG_VENDOR_ID, &read_val);
	if (rv)
		goto error_handler;

	if (read_val != CCGXXF_VENDOR_ID) {
		printf("CCGXXF Invalid Vendor ID\n");
		goto error_handler;
	}

	/* Make sure chip has correct product ID */
	rv = ccgxxf_i2c_readw(&me->bus->ops, CCGXXF_REG_PRODUCT_ID, &read_val);
	if (rv)
		goto error_handler;

	if (read_val != CCGXXF_PRODUCT_ID_CCG6DF &&
		read_val != CCGXXF_PRODUCT_ID_CCG6SF) {
		printf("CCGXXF Invalid Product ID\n");
		goto error_handler;
	}

	/* Get the current firmware region */
	rv = ccgxxf_i2c_writew(&me->bus->ops, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_GET_FW_MODE);
	if (rv)
		goto error_handler;

	rv = ccgxxf_get_fwu_status_reg(&me->bus->ops, &read_val);
	if (rv)
		goto error_handler;

	/*
	 * CCGXXF has two firmware regions and each region have separate
	 * copies of firmware. Upon reset, CCGXXF always boots from the last
	 * updated region regardless of the firmware versions.
	 *
	 * CCGXXF allows updating the non executing firmware region only and
	 * the image1 must be flashed on region-1 & image-2 must be flashed
	 * on region 2, hence assign the correct image to be updated on non
	 * executing region.
	 */
	if (read_val == CCGXXF_FWU_RES_FW_REGION_1)
		me->fw_ops = *image2;
	else if (read_val == CCGXXF_FWU_RES_FW_REGION_2)
		me->fw_ops = *image1;
	else
		goto error_handler;

	return me;

error_handler:
	free(me);

	return NULL;
}

static vb2_error_t ccgxxf_check_hash(const VbootAuxfwOps *vbaux,
	const uint8_t *hash, size_t hash_size,
	enum vb2_auxfw_update_severity *severity)
{
	return VB2_SUCCESS;
}

static const VbootAuxfwOps ccg6sf_image1_fw_ops = {
	.fw_image_name = "ccg6sf_image1.bin",
	.fw_hash_name = "ccg6sf_image1.hash",
	.check_hash = ccgxxf_check_hash,
	.update_image = ccgxxf_update_image,
};

static const VbootAuxfwOps ccg6sf_image2_fw_ops = {
	.fw_image_name = "ccg6sf_image2.bin",
	.fw_hash_name = "ccg6sf_image2.hash",
	.check_hash = ccgxxf_check_hash,
	.update_image = ccgxxf_update_image,
};

static const VbootAuxfwOps ccg6df_image1_fw_ops = {
	.fw_image_name = "ccg6df_image1.bin",
	.fw_hash_name = "ccg6df_image1.hash",
	.check_hash = ccgxxf_check_hash,
	.update_image = ccgxxf_update_image,
};

static const VbootAuxfwOps ccg6df_image2_fw_ops = {
	.fw_image_name = "ccg6df_image2.bin",
	.fw_hash_name = "ccg6df_image1.hash",
	.check_hash = ccgxxf_check_hash,
	.update_image = ccgxxf_update_image,
};

Ccgxxf *new_ccg6sf(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return get_ccgxxf_fwOps(bus, ec_pd_id,
			&ccg6sf_image1_fw_ops, &ccg6sf_image2_fw_ops);
}

Ccgxxf *new_ccg6df(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return get_ccgxxf_fwOps(bus, ec_pd_id,
			&ccg6df_image1_fw_ops, &ccg6df_image2_fw_ops);
}

static const VbootAuxfwOps *new_ccgxxf_from_chip_info(
	struct ec_response_pd_chip_info *r, uint8_t ec_pd_id)
{
	Ccgxxf *ccgxxf;

	switch(r->product_id) {
	case CCGXXF_PRODUCT_ID_CCG6SF:
		ccgxxf = new_ccg6sf(NULL, ec_pd_id);
		break;
	case CCGXXF_PRODUCT_ID_CCG6DF:
		ccgxxf = new_ccg6df(NULL, ec_pd_id);
		break;
	default:
		return NULL;
	}

	ccgxxf->chip.vendor = r->vendor_id;
	ccgxxf->chip.product = r->product_id;
	ccgxxf->chip.device = r->device_id;
	ccgxxf->chip.fw_rev = r->fw_version_number;

	printf("%s: vendor 0x%04x product 0x%04x device 0x%04x fw_rev 0x%02x\n",
		ccgxxf->chip_name, ccgxxf->chip.vendor, ccgxxf->chip.product,
		ccgxxf->chip.device, ccgxxf->chip.fw_rev);

	return &ccgxxf->fw_ops;
}

static CrosEcAuxfwChipInfo aux_fw_ccg6df_info = {
	.vid = CCGXXF_VENDOR_ID,
	.pid = CCGXXF_PRODUCT_ID_CCG6DF,
	.new_chip_aux_fw_ops = new_ccgxxf_from_chip_info,
};

static CrosEcAuxfwChipInfo aux_fw_ccg6sf_info = {
	.vid = CCGXXF_VENDOR_ID,
	.pid = CCGXXF_PRODUCT_ID_CCG6SF,
	.new_chip_aux_fw_ops = new_ccgxxf_from_chip_info,
};

static int ccgxxf_register(void)
{
	list_insert_after(&aux_fw_ccg6df_info.list_node,
			&ec_aux_fw_chip_list);
	list_insert_after(&aux_fw_ccg6sf_info.list_node,
			&ec_aux_fw_chip_list);
	return 0;
}

INIT_FUNC(ccgxxf_register);
