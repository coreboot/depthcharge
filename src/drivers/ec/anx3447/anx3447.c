/*
 * Copyright 2022 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <endian.h>
#include <libpayload.h>
#include <stddef.h>

#include "base/container_of.h"
#include "drivers/ec/anx3447/anx3447.h"

#define I2C_TCPC_ADDR			(0x58 >> 1)
#define I2C_SPI_ADDR			(0x7E >> 1)

#define TCPC_COMMAND			0x23

#define DATA_BLOCK_SIZE			32
#define R_RAM_LEN_H			0x03
#define FLASH_ADDR_EXTEND		0x80
#define R_RAM_CTRL			0x05
#define FLASH_DONE			0x80
#define R_FLASH_ADDR_H			0x0c
#define R_FLASH_ADDR_L			0x0d
#define FLASH_WRITE_DATA_0		0x0e
#define R_FLASH_LEN_H			0x2e
#define R_FLASH_LEN_L			0x2f
#define R_FLASH_RW_CTRL			0x30
#define GENERAL_CMD			0x40
#define FLASH_ERASE			0x20
#define WRITE_STATUS			0x04
#define FLASH_READ			0x02
#define FLASH_WRITE			0x01
#define R_FLASH_STATUS_0		0x31
#define DISABLE_WP			0x43
#define GENERAL_CMD_TYPE		0x33
#define FLASH_ERASE_TYPE		0x34
#define FLASH_STATUS			0x35
#define WIP				0x01
#define FLASH_READ_DATA_0		0x3C
#define R_I2C_0				0x5C
#define READ_STATUS			0x80
#define OCM_CTRL_0			0x6e
#define OCM_RESET			0x40
#define ADDR_GPIO_CTRL_0		0x88
#define SPI_WP				0x80
#define WRITE_EN			0x06
#define CHIP_ERASE			0x60
#define FW_VER01			0xB4
#define FW_VER02			0xB5

#define ANX_WAIT_TIMEOUT_US		100000
#define ANX_WAKEUP_MS			10

#define ANX3447_ID			0x7977

#define ANX3447_DEBUG			1
#if (ANX3447_DEBUG > 0)
#define debug(msg, args...)		printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

static int __must_check read_reg(Anx3447 *me, uint8_t addr,
				 uint8_t reg, uint8_t *data)
{
	return i2c_readb(&me->bus->ops, addr, reg, data);
}

static int __must_check write_reg(Anx3447 *me, uint8_t addr,
				  uint8_t reg, uint8_t data)
{
	return i2c_writeb(&me->bus->ops, addr, reg, data);
}

static int __must_check write_reg_or(Anx3447 *me, uint8_t addr,
				     uint8_t reg, uint8_t data)
{
	uint8_t val;

	if (read_reg(me, addr, reg, &val) != 0) {
		debug("read reg:0x%x failed\n", reg);
		return -1;
	}

	return write_reg(me, addr, reg, data | val);
}

static int __must_check write_reg_and(Anx3447 *me, uint8_t addr,
				      uint8_t reg, uint8_t data)
{
	uint8_t val;

	if (read_reg(me, addr, reg, &val) != 0) {
		debug("read reg:0x%x failed\n", reg);
		return -1;
	}

	return write_reg(me, addr, reg, data & val);
}

static int __must_check write_block(Anx3447 *me, uint8_t addr, uint8_t reg,
				    const uint8_t *data, size_t len)
{
	return i2c_writeblock(&me->bus->ops, addr, reg, data, len);
}

static int __must_check read_block(Anx3447 *me, uint8_t addr, uint8_t reg,
				   uint8_t *data, size_t len)
{
	return i2c_readblock(&me->bus->ops, addr, reg, data, len);
}

static int __must_check wait_flash_op_done(Anx3447 *me)
{
	uint64_t t0 = timer_us(0);
	uint8_t val;

	while (1) {
		if (read_reg(me, I2C_SPI_ADDR, R_RAM_CTRL, &val) != 0)
			return -1;

		if (val & FLASH_DONE)
			break;

		mdelay(1);
		if (timer_us(t0) >= ANX_WAIT_TIMEOUT_US) {
			printf("anx3447.%d: wait timeout\n", me->ec_pd_id);
			return -1;
		}
	}

	return 0;
}

static int __must_check flash_operation_init(Anx3447 *me)
{
	int ret;

	mdelay(ANX_WAKEUP_MS);

	ret = write_reg_or(me, I2C_SPI_ADDR, OCM_CTRL_0, OCM_RESET);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_LEN_H, 0);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_LEN_L, 0x1f);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, FLASH_READ);
	if (ret) {
		debug("I2C access FLASH fail: FLASH_READ\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("Read flash failed: timeout\n");
		return -1;
	}

	return 0;
}

static int __must_check flash_unprotect(Anx3447 *me)
{
	int ret;

	ret = write_reg_or(me, I2C_SPI_ADDR, ADDR_GPIO_CTRL_0, SPI_WP);
	ret |= write_reg(me, I2C_SPI_ADDR, GENERAL_CMD_TYPE, WRITE_EN);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, GENERAL_CMD);
	if (ret) {
		debug("I2C access FLASH fail: GENERAL_CMD\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("Enable write flash failed: timeout\n");
		return -1;
	}

	ret = write_reg_and(me, I2C_SPI_ADDR, R_FLASH_STATUS_0, DISABLE_WP);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, WRITE_STATUS);
	if (ret) {
		debug("I2C access FLASH fail: WRITE_STATUS\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("Write flash status failed: timeout\n");
		return -1;
	}

	return 0;
}

static int __must_check flash_block_read(Anx3447 *me, uint32_t addr,
					 uint8_t *buf)
{
	int ret;

	/* Write flash address*/
	ret = write_reg_and(me, I2C_SPI_ADDR, R_RAM_LEN_H,
			    (~FLASH_ADDR_EXTEND) & 0xff);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_ADDR_H, (addr >> 8) & 0xff);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_ADDR_L, addr & 0xff);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_LEN_H, 0);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_LEN_L, DATA_BLOCK_SIZE - 1);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, FLASH_READ);
	if (ret) {
		debug("I2C access FLASH fail: FLASH_READ\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("read flash data failed: timeout\n");
		return -1;
	}
	mdelay(1);
	ret = read_block(me, I2C_SPI_ADDR, FLASH_READ_DATA_0,
			 buf, DATA_BLOCK_SIZE);
	if (ret < 0) {
		debug("read flash data failed: read data\n");
		return -1;
	}

	return 0;
}

static int __must_check flash_block_write(Anx3447 *me,
					  uint32_t addr,
					  const uint8_t *buf)
{
	int ret;
	uint8_t val;
	uint64_t t0;

	ret = write_reg(me, I2C_SPI_ADDR, GENERAL_CMD_TYPE, WRITE_EN);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, GENERAL_CMD);
	if (ret) {
		debug("I2C access FLASH fail: GENERAL_CMD\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("Enable write flash failed: timeout\n");
		return -1;
	}

	ret = write_block(me, I2C_SPI_ADDR, FLASH_WRITE_DATA_0,
			  buf, DATA_BLOCK_SIZE);
	ret |= write_reg_and(me, I2C_SPI_ADDR, R_RAM_LEN_H,
			     (uint8_t)~FLASH_ADDR_EXTEND);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_ADDR_H, (addr >> 8) & 0xff);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_ADDR_L, addr & 0xff);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_LEN_H, 0);
	ret |= write_reg(me, I2C_SPI_ADDR, R_FLASH_LEN_L, DATA_BLOCK_SIZE - 1);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, FLASH_WRITE);
	if (ret) {
		debug("I2C access FLASH fail: FLASH_WRITE\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("write flash data failed: timeout\n");
		return -1;
	}

	t0 = timer_us(0);
	while (1) {
		if (write_reg_or(me, I2C_SPI_ADDR, R_I2C_0, READ_STATUS) != 0) {
			debug("I2C access R_I2C_0 fail: READ_STATUS\n");
			return -1;
		}

		if (wait_flash_op_done(me) != 0)
			debug("Read flash op failed: timeout\n");

		if (read_reg(me, I2C_SPI_ADDR, FLASH_STATUS, &val) != 0) {
			debug("I2C access R_I2C_0 fail: READ_STATUS\n");
			return -1;
		}

		if (!(val & WIP))
			break;

		mdelay(1);
		if (timer_us(t0) > ANX_WAIT_TIMEOUT_US * 5) {
			debug("Read flash status failed: timeout\n");
			return -1;
		}
	}

	return 0;
}

static int __must_check anx3447_flash_read_verify(Anx3447 *me,
						  const uint8_t *buf,
						  const size_t len)
{
	int i;
	uint32_t offset;
	uint8_t flash_data[DATA_BLOCK_SIZE];

	if (flash_operation_init(me) != 0) {
		debug("Read flash init failed\n");
		return -1;
	}

	for (i = 0; i < len / DATA_BLOCK_SIZE; i++) {
		offset = i * DATA_BLOCK_SIZE;
		if (flash_block_read(me, offset, flash_data) != 0) {
			debug("Flash read failed at 0ffset(0x%x)\n", offset);
			return -1;
		}

		if (memcmp(&buf[offset], flash_data, DATA_BLOCK_SIZE)) {
			debug("Flash compare failed at 0ffset(0x%x)\n", offset);
			return -1;
		}
	}

	return 0;
}

static int __must_check anx3447_flash_write(Anx3447 *me,
					    const uint8_t *buf,
					    const size_t len)
{
	int i;
	uint32_t offset;
	uint8_t blank_data[DATA_BLOCK_SIZE];

	if (flash_operation_init(me) != 0) {
		debug("Write flash init failed\n");
		return -1;
	}

	if (flash_unprotect(me) != 0) {
		debug("Flash disable write protect failed\n");
		return -1;
	}

	memset(blank_data, 0xff, DATA_BLOCK_SIZE);
	for (i = 0; i < len / DATA_BLOCK_SIZE; i++) {
		offset = i * DATA_BLOCK_SIZE;
		if (!memcmp(&buf[offset], blank_data, DATA_BLOCK_SIZE))
			continue;

		if (flash_block_write(me, offset, &buf[offset]) != 0) {
			debug("Flash write failed at 0ffset(0x%x)\n", offset);
			return -1;
		}
	}

	debug("Write flash finished!\n");
	return 0;
}

static int __must_check anx3447_flash_chip_erase(Anx3447 *me)
{
	int ret;

	if (flash_operation_init(me) != 0) {
		debug("Flash operation initial failed\n");
		return -1;
	}

	if (flash_unprotect(me) != 0) {
		debug("Disable flash protection failed\n");
		return -1;
	}

	ret = write_reg(me, I2C_SPI_ADDR, GENERAL_CMD_TYPE, WRITE_EN);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, GENERAL_CMD);
	if (ret) {
		debug("I2C access FLASH fail: GENERAL_CMD\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("Enable write flash failed: timeout\n");
		return -1;
	}

	ret = write_reg(me, I2C_SPI_ADDR, FLASH_ERASE_TYPE, CHIP_ERASE);
	ret |= write_reg_or(me, I2C_SPI_ADDR, R_FLASH_RW_CTRL, FLASH_ERASE);
	if (ret) {
		debug("I2C access FLASH fail: FLASH_ERASE\n");
		return -1;
	}

	if (wait_flash_op_done(me) != 0) {
		debug("Flash erase failed: timeout\n");
		return -1;
	}

	debug("Flash erase done\n");
	return 0;
}

/**
 * capture chip (vendor, product, device, rev) IDs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */
static int __must_check anx3447_capture_device_id(Anx3447 *me)
{
	struct ec_params_pd_chip_info p;
	struct ec_response_pd_chip_info r;

	p.port = me->ec_pd_id;
	p.renew = 0;
	int status = ec_command(me->bus->ec, EC_CMD_PD_CHIP_INFO, 0,
				&p, sizeof(p), &r, sizeof(r));
	if (status < 0) {
		printf("anx3447.%d: could not get chip info!\n", me->ec_pd_id);
		return -1;
	}

	uint16_t vendor  = r.vendor_id;
	uint16_t product = r.product_id;
	uint16_t device  = r.device_id;
	uint8_t fw_rev = r.fw_version_number;

	printf("anx3447.%d: vendor 0x%04x product 0x%04x "
	       "device 0x%04x fw_rev 0x%02x\n",
	       me->ec_pd_id, vendor, product, device, fw_rev);

	if (vendor != ANX3447_ID) {
		printf("anx3447.%d: vendor 0x%04x, expected %04x\n",
		       me->ec_pd_id, vendor, ANX3447_ID);
		return -1;
	}

	me->chip.vendor = vendor;
	me->chip.product = product;
	me->chip.device = device;
	me->chip.fw_rev = fw_rev;

	return 0;
}

static VbError_t anx3447_check_hash(const VbootAuxFwOps *vbaux,
				    const uint8_t *hash, size_t hash_size,
				    VbAuxFwUpdateSeverity_t *severity)
{
	Anx3447 *me = container_of(vbaux, Anx3447, fw_ops);
	VbError_t status = VBERROR_UNKNOWN;

	debug("call...\n");

	if (anx3447_capture_device_id(me) == 0)
		status = VBERROR_SUCCESS;

	if (status != VBERROR_SUCCESS)
		return status;

	if (hash[0] == me->chip.fw_rev)
		*severity = VB_AUX_FW_NO_UPDATE;
	else
		*severity = VB_AUX_FW_SLOW_UPDATE;

	return VBERROR_SUCCESS;
}

static int anx3447_wake_up(Anx3447 *me)
{
	int ret;
	uint8_t val;

	ret = read_reg(me, I2C_TCPC_ADDR, TCPC_COMMAND, &val);
	if (ret)
		mdelay(ANX_WAKEUP_MS);

	if (read_reg(me, I2C_TCPC_ADDR, TCPC_COMMAND, &val) != 0) {
		debug("Anx3447.%d wakeup fail, val:%02x\n", me->ec_pd_id, val);
		return -1;
	}

	debug("Anx3447.%d wakeup OK\n", me->ec_pd_id);
	return 0;
}

static int anx3447_power_cycle(Anx3447 *me)
{
	int ret;

	ret = write_reg(me, I2C_TCPC_ADDR, TCPC_COMMAND, 0xFF);
	if (ret != 0)
		debug("Anx3447.%d ignore standby command fail return\n",
		      me->ec_pd_id);
	mdelay(ANX_WAKEUP_MS);

	if (anx3447_wake_up(me) != 0) {
		debug("Anx3447.%d, power cycle fail\n", me->ec_pd_id);
		return -1;
	}

	debug("Anx3447.%d, power cycle OK\n", me->ec_pd_id);

	return 0;
}

static int anx3447_check_fw_version(Anx3447 *me)
{
	uint8_t rev01, rev02;
	int ret;

	ret = read_reg(me, I2C_SPI_ADDR, FW_VER01, &rev01);
	ret |= read_reg(me, I2C_SPI_ADDR, FW_VER02, &rev02);
	if (ret) {
		debug("I2C access FW_VER01 fail\n");
		return -1;
	}

	debug("FW main version(0x%02x), subver(0x%02x)\n", rev01, rev02);
	return 0;
}

static int anx3447_flash_update(Anx3447 *me,
				const uint8_t *image,
				const size_t image_size)
{
	if (anx3447_check_fw_version(me) != 0) {
		debug("Anx3447.%d read FW version failed before erasing\n",
		      me->ec_pd_id);
		return -1;
	}

	if (anx3447_flash_chip_erase(me) != 0) {
		debug("Anx3447.%d erase flash failed\n", me->ec_pd_id);
		return -1;
	}

	if (anx3447_flash_write(me, image, image_size) != 0) {
		debug("Anx3447.%d write flash failed\n", me->ec_pd_id);
		return -1;
	}

	debug("Anx3447.%d write flash done\n", me->ec_pd_id);

	if (anx3447_flash_read_verify(me, image, image_size) != 0) {
		debug("Anx3447.%d verify flash failed\n", me->ec_pd_id);
		return -1;
	}

	debug("Anx3447.%d verify flash done, update success\n", me->ec_pd_id);

	return 0;
}

static VbError_t anx3447_update_image(const VbootAuxFwOps *vbaux,
				      const uint8_t *image,
				      const size_t image_size)
{
	Anx3447 *me = container_of(vbaux, Anx3447, fw_ops);
	VbError_t status = VBERROR_UNKNOWN;
	int protected, ret;

	debug("call...\n");

	if (cros_ec_tunnel_i2c_protect_status(me->bus, &protected) < 0) {
		debug("Anx3447.%d tunnel status failed\n", me->ec_pd_id);
		return VBERROR_UNKNOWN;
	}

	if (protected) {
		debug("Anx3447.%d already protected\n", me->ec_pd_id);
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}

	ret = cros_ec_pd_control(me->ec_pd_id, PD_SUSPEND);
	switch (ret) {
	case -EC_RES_BUSY:
		return VBERROR_PERIPHERAL_BUSY;
	case EC_RES_SUCCESS:
		debug("Anx3447.%d suspend success\n", me->ec_pd_id);
		break;
	default:
		debug("Anx3447.%d suspend failed\n", me->ec_pd_id);
		return VBERROR_UNKNOWN;
	}

	if (anx3447_wake_up(me) != 0) {
		debug("Anx3447.%d wake up failed\n", me->ec_pd_id);
		return VBERROR_UNKNOWN;
	}

	if (anx3447_flash_update(me, image, image_size) != 0) {
		debug("Anx3447.%d flash update failed\n", me->ec_pd_id);
		goto pd_resume;
	}

	if (anx3447_power_cycle(me) != 0) {
		debug("Anx3447.%d reboot failed\n", me->ec_pd_id);
		goto pd_resume;
	}

	mdelay(ANX_WAKEUP_MS);

	if (anx3447_check_fw_version(me) != 0)
		debug("Anx3447.%d check new fw version failed\n", me->ec_pd_id);

	status = VBERROR_SUCCESS;

pd_resume:
	ret = cros_ec_pd_control(me->ec_pd_id, PD_RESUME);
	if (!ret) {
		debug("Anx3447.%d resume success\n", me->ec_pd_id);
	} else {
		debug("Anx3447.%d resume failed\n", me->ec_pd_id);
		status = VBERROR_UNKNOWN;
	}

	memset(&me->chip, 0, sizeof(me->chip));
	/*
	 * wait for the TCPC and pd_task() to restart
	 *
	 * TODO(b/64696543): remove delay when the EC can delay TCPC
	 * host commands until it can service them.
	 */

	return status;
}

static const VbootAuxFwOps anx3447_fw_ops = {
	.fw_image_name = "anx3447_ocm.bin",
	.fw_hash_name = "anx3447_ocm.hash",
	.check_hash = anx3447_check_hash,
	.update_image = anx3447_update_image,
};

Anx3447 *new_anx3447(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx3447 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx3447_fw_ops;

	return me;
}
