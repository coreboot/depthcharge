// SPDX-License-Identifier: GPL-2.0

/*
 * MCU - microcontroller
 * OCM - on chip microcontroller
 * OTP - one time programmable (ROM)
 */

#include <endian.h>
#include <libpayload.h>
#include <stddef.h>
#include <vb2_api.h>

#include "drivers/ec/anx3447/anx3447.h"

/*
 * enable debug code.
 * level 1 adds some logging.
 * level 2 forces FW update, even more logging.
 */
#define ANX3447_DEBUG	0

#if (ANX3447_DEBUG > 0)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#define dprintf(msg, args...)	printf(msg, ##args)
#else
#define debug(msg, args...)
#define dprintf(msg, args...)
#endif

#define ANX_RESTART_MS		(1 * MSECS_PER_SEC) /* 1s */

/* ANX3447 registers */
#define DATA_BLOCK_SIZE			32
#define I2C_TCPC_ADDR			0x58
#define I2C_SPI_ADDR			0x7E
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

#define GENERAL_INSTRUCTION_EN		0x40
#define FLASH_ERASE_EN			0x20
#define WRITE_STATUS_EN			0x04
#define FLASH_READ			0x02
#define FLASH_WRITE			0x01

#define R_FLASH_STATUS_0		0x31
#define FLASH_INSTRUCTION_TYPE		0x33
#define FLASH_ERASE_TYPE		0x34
#define R_FLASH_STATUS_REGISTER_READ_0	0x35
#define WIP				0x01
#define FLASH_READ_DATA_0		0x3C
#define R_I2C_0				0x5C
#define READ_STATUS_EN			0x80
#define OCM_CTRL_0			0x6e
#define OCM_RESET			0x40
#define ADDR_GPIO_CTRL_0		0x88
#define SPI_WP				0x80
#define WRITE_EN			0x06
#define DEEP_PWRDN			0xB9
#define CHIP_ERASE			0x60
#define ANX_FW_I2C_ADDR			0x3F

#define ANALOGIX_VENDOR_ID		0x7977
#define ANX3447_PRODUCT_ID		0x7447

#define ANX3447_FW_POINTER		0x1f800
#define ANX3447_FW_SECTION_A		0xff
#define ANX3447_FW_SECTION_B		0x00

#define ANX3447_FW_SECTION_A_OFFSET	0x0
#define ANX3447_FW_SECTION_B_OFFSET	((uint16_t) 0x10000)

#define ANX3447_FW_CRC_SECTION_A	0xdffc
#define ANX3447_FW_CRC_SECTION_B	0x1fbfc

#define ANX_WAIT_TIMEOUT_US		100000


static int __must_check read_reg(Anx3447 *me, uint8_t reg, uint8_t *data)
{
	return i2c_readb(&me->bus->ops, ANX_FW_I2C_ADDR, reg, data);
}

static int __must_check write_reg(Anx3447 *me, uint8_t reg, uint8_t data)
{
	return i2c_writeb(&me->bus->ops, ANX_FW_I2C_ADDR, reg, data);
}

static int __must_check write_reg_or(Anx3447 *me, uint8_t reg, uint8_t data)
{
	uint8_t val;

	if (read_reg(me, reg, &val) != 0) {
		debug("read reg:0x%x failed\n", reg);
		return -1;
	}

	return write_reg(me, reg, data | val);
}

static int __must_check update_reg(Anx3447 *me, uint8_t reg, uint8_t mask,
				   uint8_t data)
{
	uint8_t val;

	if (read_reg(me, reg, &val))
		return -1;

	val = (val & ~mask) | (data & mask);

	if (write_reg(me, reg, val))
		return -1;

	return 0;
}

static int __must_check wait_flash_op_done(Anx3447 *me)
{
	uint64_t t0 = timer_us(0);
	uint8_t val;

	while (1) {
		if (read_reg(me, R_RAM_CTRL, &val) != 0)
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

static int __must_check anx3447_flash_operation_init(Anx3447 *me)
{
	uint8_t val;

	/*
	 * chip wake-up, to read one register of I2C_TCPC_ADDR to
	 * to wake up chip
	 */
	if (read_reg(me, 0x0, &val))
		return -1;

	/* add 10ms delay to make sure chip is waked up */
	mdelay(10);

	/* reset On chip MCU */
	if (update_reg(me, OCM_CTRL_0, OCM_RESET, OCM_RESET))
		return -1;

	/* read flash for deep power down wake up */
	if (write_reg(me, R_FLASH_LEN_H, 0x0))
		return -1;

	if (write_reg(me, R_FLASH_LEN_L, 0x1f))
		return -1;

	/* enable flash read operation */
	if (update_reg(me, R_FLASH_RW_CTRL, FLASH_READ, FLASH_READ))
		return -1;

	mdelay(4);

	if (wait_flash_op_done(me)) {
		debug("Read flash failed: timeout\n");
		return -1;
	}

	mdelay(5);

	return 0;
}

static int __must_check anx3447_flash_unprotect(Anx3447 *me)
{
	/* disable hardware protected mode */
	if (update_reg(me, ADDR_GPIO_CTRL_0, SPI_WP, SPI_WP))
		return -1;

	if (write_reg(me, FLASH_INSTRUCTION_TYPE , WRITE_EN))
		return -1;

	if (update_reg(me, R_FLASH_RW_CTRL, GENERAL_INSTRUCTION_EN,
		       GENERAL_INSTRUCTION_EN))
		return -1;

	/* wait for Write Enable (WREN) Sequence done */
	if (wait_flash_op_done(me)) {
		debug("Enable write flash failed: timeout\n");
		return -1;
	}

	/* disable protect */
	if (update_reg(me, R_FLASH_STATUS_0, 0xBC, 0))
		return -1;

	if (update_reg(me, R_FLASH_RW_CTRL, WRITE_STATUS_EN, WRITE_STATUS_EN))
		return -1;

	if (wait_flash_op_done(me)) {
		debug("Write flash status failed: timeout\n");
		return -1;
	}

	mdelay(10);

	return 0;
}

static int __must_check anx3447_flash_write_block(Anx3447 *me, uint32_t addr,
						  const uint8_t *buf)
{
	uint8_t i, val;
	uint8_t blank_data[DATA_BLOCK_SIZE];

	/* skip the empty block */
	memset(blank_data, 0xff, DATA_BLOCK_SIZE);
	if (!memcmp(buf, blank_data, DATA_BLOCK_SIZE))
		return 0;

	/* move data to registers */
	for (i = 0; i < DATA_BLOCK_SIZE; i++) {
		if (write_reg(me, FLASH_WRITE_DATA_0 + i, buf[i]))
			return -1;
	}

	/* flash write enable */
	if (write_reg(me, FLASH_INSTRUCTION_TYPE, WRITE_EN))
		return -1;

	if (update_reg(me, R_FLASH_RW_CTRL, GENERAL_INSTRUCTION_EN,
		       GENERAL_INSTRUCTION_EN))
		return -1;

	/* wait for Write Enable (WREN) Sequence done */
	if (wait_flash_op_done(me)) {
		debug("Enable write flash failed: timeout\n");
		return -1;
	}

	/* write flash address */
	if (update_reg(me, R_RAM_LEN_H, FLASH_ADDR_EXTEND, 0))
		return -1;

	if (write_reg(me, R_FLASH_ADDR_H, (uint8_t)(addr >> 8)))
		return -1;

	if (write_reg(me, R_FLASH_ADDR_L, (uint8_t)(addr & 0xff)))
		return -1;

	/* write data length */
	if (write_reg(me, R_FLASH_LEN_H, 0x0))
		return -1;

	if (write_reg(me, R_FLASH_LEN_L, 0x1f))
		return -1;

	/* flash write start */
	if (update_reg(me, R_FLASH_RW_CTRL, FLASH_WRITE, FLASH_WRITE))
		return -1;

	/* wait flash write sequence done */
	if (wait_flash_op_done(me)) {
		debug("write flash data failed: timeout\n");
		return -1;
	}

	do {
		if (update_reg(me, R_I2C_0, READ_STATUS_EN, READ_STATUS_EN))
			return -1;
		/* wait for Read Status Register (RDSR) Sequence done */
		if (wait_flash_op_done(me))
			debug("Read flash op failed: timeout\n");

		if (read_reg(me, R_FLASH_STATUS_REGISTER_READ_0, &val))
			return -1;
	} while (val & WIP);

	return 0;
}

static int __must_check anx3447_flash_chip_erase(Anx3447 *me)
{
	int ret;

	ret = write_reg(me, FLASH_INSTRUCTION_TYPE, WRITE_EN);
	ret |= write_reg_or(me, R_FLASH_RW_CTRL, GENERAL_INSTRUCTION_EN);
	if (ret) {
		debug("I2C access FLASH fail: GENERAL_CMD\n");
		return -1;
	}

	if (wait_flash_op_done(me)) {
		debug("Enable write flash failed: timeout\n");
		return -1;
	}

	ret = write_reg(me, FLASH_ERASE_TYPE, CHIP_ERASE);
	ret |= write_reg_or(me, R_FLASH_RW_CTRL, FLASH_ERASE_EN);
	if (ret) {
		debug("I2C access FLASH fail: FLASH_ERASE\n");
		return -1;
	}

	if (wait_flash_op_done(me)) {
		debug("Flash erase failed: timeout\n");
		return -1;
	}

	debug("Flash erase done\n");
	return 0;
}

static int anx3447_ec_pd_suspend(Anx3447 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_SUSPEND);
	if (status == -EC_RES_BUSY)
		printf("anx3447.%d: PD_SUSPEND busy! Could be only power "
		       "source.\n",
		       me->ec_pd_id);
	else  if (status != 0)
		printf("anx3447.%d: PD_SUSPEND failed!\n", me->ec_pd_id);
	return status;
}

static int anx3447_ec_pd_resume(Anx3447 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_RESUME);
	if (status != 0)
		printf("anx3447.%d: PD_RESUME failed!\n", me->ec_pd_id);
	return status;
}

static int anx3447_ec_pd_reset(Anx3447 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_RESET);
	if (status != 0)
		printf("anx3447.%d: PD_RESET failed!\n", me->ec_pd_id);
	return status;
}

/**
 * capture chip (vendor, product, device, rev) IDs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */
static int __must_check anx3447_capture_device_id(Anx3447 *me)
{
	struct ec_response_pd_chip_info_v2 r;

	if (me->chip.vendor != 0)
		return 0;

	int status = cros_ec_pd_chip_info(me->ec_pd_id, 0, &r);
	if (status < 0) {
		printf("anx3447.%d: could not get chip info!\n", me->ec_pd_id);
		return -1;
	}

	uint16_t vendor  = r.vendor_id;
	uint16_t product = r.product_id;
	uint16_t device  = r.device_id;
	uint16_t fw_rev = r.fw_version_number;

	if (vendor != ANALOGIX_VENDOR_ID || product != ANX3447_PRODUCT_ID) {
		printf("anx3447.%d: Unsupport vendor 0x%04x, product 0x%04x\n",
		       me->ec_pd_id, vendor, product);
		return -1;
	}

	me->chip.vendor = vendor;
	me->chip.product = product;
	me->chip.device = device;
	me->chip.fw_rev = fw_rev;
	return 0;
}

static vb2_error_t anx3447_check_hash(const VbootAuxfwOps *vbaux,
				      const uint8_t *hash, size_t hash_size,
				      enum vb2_auxfw_update_severity *severity)
{
	Anx3447 *me = container_of(vbaux, Anx3447, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	uint16_t fw_hash = (hash[0] << 8) | hash[1];

	debug("call...\n");

	if (hash_size != 2)
		return VB2_ERROR_INVALID_PARAMETER;

	if (anx3447_capture_device_id(me) == 0) {
		status = VB2_SUCCESS;
	} else {
		printf("Not anx3447, no update required.\n");
		*severity = VB2_AUXFW_NO_DEVICE;
		return VB2_SUCCESS;
	}

	if (status != VB2_SUCCESS)
		return status;

	if (fw_hash == me->chip.fw_rev) {
		if (ANX3447_DEBUG >= 2 && !me->debug_updated) {
			debug("forcing VB2_AUXFW_SLOW_UPDATE\n");
			*severity = VB2_AUXFW_SLOW_UPDATE;
		} else {
			*severity = VB2_AUXFW_NO_UPDATE;
			return VB2_SUCCESS;
		}
	} else {
		*severity = VB2_AUXFW_SLOW_UPDATE;
	}

	switch (anx3447_ec_pd_suspend(me)) {
	case -EC_RES_BUSY:
		printf("anx3447.%d: pd suspend busy\n", me->ec_pd_id);
		*severity = VB2_AUXFW_NO_UPDATE;
		break;
	case EC_RES_SUCCESS:
		break;
	default:
		printf("anx3447.%d: pd suspend failed\n", me->ec_pd_id);
		*severity = VB2_AUXFW_NO_DEVICE;
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static void anx3447_clear_device_id(Anx3447 *me)
{
	memset(&me->chip, 0, sizeof(me->chip));
}

static vb2_error_t anx3447_flash(Anx3447 *me, const uint8_t *image,
				 const size_t image_size, uint8_t section)
{
	uint16_t i, base;

	switch (section) {
	case ANX3447_FW_SECTION_A:
		base = ANX3447_FW_SECTION_A_OFFSET;
		break;
	case ANX3447_FW_SECTION_B:
		base = ANX3447_FW_SECTION_B_OFFSET;
		break;
	default:
		return VB2_ERROR_INVALID_PARAMETER;
	}

	if (anx3447_flash_operation_init(me)) {
		debug("Flash operation initial failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (anx3447_flash_unprotect(me)) {
		debug("Disable flash protection failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (anx3447_flash_chip_erase(me) != 0) {
		debug("Anx3447.%d erase flash failed\n", me->ec_pd_id);
		return VB2_ERROR_UNKNOWN;
	}

	for (i = 0; i + DATA_BLOCK_SIZE < image_size; i += DATA_BLOCK_SIZE) {
		if (anx3447_flash_write_block(me, base + i, image + i))
			return VB2_ERROR_UNKNOWN;
	}

	if (i < image_size) {
		uint8_t tmp[DATA_BLOCK_SIZE] = {[0 ... DATA_BLOCK_SIZE - 1] =
							0xff};
		memcpy(tmp, image + i, image_size - i);
		if (anx3447_flash_write_block(me, base + i, tmp))
			return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static vb2_error_t anx3447_update_image(const VbootAuxfwOps *vbaux,
					const uint8_t *image,
					const size_t image_size)
{
	Anx3447 *me = container_of(vbaux, Anx3447, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	int protected;
	uint8_t curr_section = ANX3447_FW_SECTION_A;

	debug("call...\n");

	if (cros_ec_tunnel_i2c_protect_status(me->bus, &protected) < 0) {
		printf("anx3447.%d: could not get EC I2C tunnel status!\n",
		       me->ec_pd_id);
		return VB2_ERROR_UNKNOWN;
	}

	if (protected) {
		debug("already protected\n");
		return VB2_REQUEST_REBOOT_EC_TO_RO;
	}

	status = anx3447_flash(me, image, image_size, curr_section);

	if (anx3447_ec_pd_reset(me) != 0)
		status = VB2_ERROR_UNKNOWN;

	if (anx3447_ec_pd_resume(me) != 0)
		status = VB2_ERROR_UNKNOWN;

	/* force re-read */
	anx3447_clear_device_id(me);

	/*
	 * wait for the TCPC and pd_task() to restart
	 *
	 * TODO(b/64696543): remove delay when the EC can delay TCPC
	 * host commands until it can service them.
	 */
	mdelay(ANX_RESTART_MS);

	return status;
}

static const VbootAuxfwOps anx3447_fw_ops = {
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
