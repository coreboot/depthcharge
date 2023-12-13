// SPDX-License-Identifier: GPL-2.0

#include <endian.h>
#include <libpayload.h>
#include <stddef.h>
#include <vb2_api.h>

#include "drivers/ec/anx7510/anx7510.h"

#define ANX7510_DEBUG 0

#if (ANX7510_DEBUG > 0)
#define debug(msg, args...) printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

/* ANX7510 registers */
#define DATA_BLOCK_SIZE			32
#define R_BOOT_RETRY			0x00
#define SRAM_CS_MASK			0xF0
#define SRAM_CS_SHIFT			4
#define LOAD_FW				7
#define SRAM_LEN_12_MASK		0x08
#define R_RAM_ADDR_H			0x01
#define R_RAM_ADDR_L			0X02

#define FLASH_LEN_16_MASK		0x10
#define SRAM_LEN_11_8_MASK		0x0F

#define R_RAM_LEN_H			0x03
#define FLASH_RAM_ADDR_SHIFT		5
#define FLASH_RAM_SIZE_SHIFT		4
#define FLASH_ADDR17_16_MASK		0x60
#define R_RAM_LEN_L			0x04

#define R_RAM_CTRL			0x05
#define FLASH_DONE			0x80
#define CRC_OK				0x20
#define LOAD_DONE			0x10
#define DECRYPT_EN			0x02
#define LOAD_START			0x01
#define R_FLASH_ADDR_H			0x0f
#define R_FLASH_ADDR_L			0x10
#define FLASH_WRITE_DATA_0		0x11
#define R_FLASH_LEN_H			0x31
#define R_FLASH_LEN_L			0x32
#define R_FLASH_RW_CTRL			0x33

#define GENERAL_INSTRUCTION_EN		0x40
#define FLASH_ERASE_EN			0x20
#define WRITE_STATUS_EN			0x04
#define FLASH_READ			0x02
#define FLASH_WRITE			0x01

#define R_FLASH_STATUS_0		0x34
#define FLASH_PROTECTION_PATTERN_MASK	0x9C
#define FLASH_INSTRUCTION_TYPE		0x36
#define WRITE_EN			0x06
#define RELEASSEDP			0xAB
#define SECTOR_ERASE			0x20

#define FLASH_SECTOR_SIZE		(4 * KiB)

#define FLASH_ERASE_TYPE		0x37
#define R_FLASH_STATUS_REGISTER_READ_0	0x38
#define FLASH_STATUS_11			0x3F
#define R_FLASH_CTRL_0			0x40
#define FLASH_LEN_SHIFT			3
#define WIP				0x01

#define FLASH_READ_DATA_0		0x60
#define READ_STATUS_EN			0x80

#define WATCH_DOG_TIMER_TH_7_0		0x50
#define WATCH_DOG_TIMER_TH_15_8		0x51
#define WATCH_DOG_TIMER_TH_23_16	0x52
#define WATCH_DOG_CTRL			0x53
#define WATCH_DOG_RST_DISABLE		0x80
#define WATCH_DOG_ENABLE		0x40
#define FLASH_WRITE_PROTECT		0x20

#define ANX7510_VENDOR_ID		0x7977

#define ANX7510_FW_SECTION_A		0x01
#define ANX7510_FW_SECTION_B		0x02

#define ANX7510_FW_SECTION_A_OFFSET	((uint32_t)0x01000)
#define ANX7510_FW_SECTION_B_OFFSET	((uint32_t)0x19000)
#define ANX7510_FW_LEN			((uint32_t)0x18000)

#define ANX7510_FW_HDCP_TX_OFFSET	((uint32_t)0x31000)
#define ANX7510_FW_HDCP_TX_LEN		0x3000

#define ANX7510_FW_HDCP_RX_OFFSET	((uint32_t)0x34000)
#define ANX7510_FW_HDCP_RX_LEN		0x3000

#define ANX7510_FW_CUST_DEF_OFFSET	((uint32_t)0x38000)
#define ANX7510_FW_CUST_DEF_LEN		0x1000

#define ANX7510_FW_POINTER_OFFSET	((uint32_t)0x3F000)
#define ANX7510_FW_POINTER_LEN		0x1000

#define ANX7510_S1_SADDR		(0x80 >> 1)
#define ANX7510_S2_SADDR		(0x90 >> 1)
#define ANX7510_TOP_ADDR		0x01	// SLAVE ID for flash access
#define ANX7510_TCPC_ADDR		0x02	// SLAVE ID for OCM ver
#define ANX7510_EMRB_ADDR		0x06	// SLAVE ID for customer def ver
#define ANX7510_HDMI_TX_ADDR		0x08	// SLAVE ID for TX ver
#define ANX7510_DP_RX_ADDR		0x30	// SLAVE ID for RX ver
#define ANX7510_VENDOR_ID_L		0x00
#define ANX7510_VENDOR_ID_H		0x01
#define ANX7510_FW_VERSION		0xFA
#define ANX7510_FW_SUBVERSION		0xFF
#define ANX7510_CUST_DEF_VER		0xE8
#define ANX7510_CUST_DEF_SUBVER		0xE9
#define ANX7510_HDCP_TX_VERSION		0x601
#define ANX7510_HDCP_TX_SUBVERSION	0x603
#define ANX7510_HDCP_RX_VERSION		0x144
#define ANX7510_HDCP_RX_SUBVERSION	0x14c

#define ANX_WAIT_TIMEOUT_US		100000

#define GETBITS(x, high, low)		((x & GENMASK(high, low)) >> low)
#define BITS17_16(x)			GETBITS(x, 17, 16)
#define BYTE1(x)			GETBITS(x, 15, 8)
#define BYTE0(x)			GETBITS(x, 7, 0)

static int __must_check anx7510_write_s1_saddr(Anx7510 *me,
					       uint8_t saddr, uint8_t offset_hi)
{
	if (me->last_offset == offset_hi && me->last_saddr == saddr)
		return 0;

	if (i2c_writeb(&me->bus->ops, ANX7510_S1_SADDR, saddr, offset_hi))
		return -1;

	me->last_offset = offset_hi;
	me->last_saddr = saddr;

	return 0;
}

static int __must_check anx7510_read8(Anx7510 *me, uint8_t saddr,
				      uint16_t offset, uint8_t *data)
{
	if (anx7510_write_s1_saddr(me, saddr, BYTE1(offset)))
		return -1;

	return i2c_readb(&me->bus->ops, ANX7510_S2_SADDR, BYTE0(offset), data);
}

static int __must_check anx7510_block_read(Anx7510 *me, uint8_t saddr,
					   uint16_t offset, uint8_t *data, uint8_t len)
{
	if (anx7510_write_s1_saddr(me, saddr, BYTE1(offset)))
		return -1;

	return i2c_readblock(&me->bus->ops, ANX7510_S2_SADDR, BYTE0(offset), data, len);
}

static int __must_check anx7510_write8(Anx7510 *me, uint8_t saddr,
				       uint16_t offset, uint8_t data)
{
	if (anx7510_write_s1_saddr(me, saddr, BYTE1(offset)))
		return -1;

	return i2c_writeb(&me->bus->ops, ANX7510_S2_SADDR, BYTE0(offset), data);
}

static int __must_check anx7510_block_write(Anx7510 *me, uint8_t saddr,
					    uint16_t offset, const uint8_t *data,
					    uint8_t len)
{
	if (anx7510_write_s1_saddr(me, saddr, BYTE1(offset)))
		return -1;

	return i2c_writeblock(&me->bus->ops, ANX7510_S2_SADDR, BYTE0(offset), data, len);
}

static int __must_check anx7510_clrset_bits(Anx7510 *me, uint8_t saddr, uint16_t offset,
					    uint8_t clear, uint8_t set)
{
	uint8_t val;

	if (anx7510_read8(me, saddr, offset, &val) != 0) {
		debug("read reg:%04x failed\n", offset);
		return -1;
	}

	return anx7510_write8(me, saddr, offset, (val & ~clear) | set);
}

static int __must_check anx7510_set_bits(Anx7510 *me, uint8_t saddr,
					 uint16_t offset, uint8_t data)
{
	return anx7510_clrset_bits(me, saddr, offset, 0, data);
}

static int __must_check anx7510_clr_bits(Anx7510 *me, uint8_t saddr,
					 uint16_t offset, uint8_t data)
{
	return anx7510_clrset_bits(me, saddr, offset, data, 0);
}

static int __must_check wait_flash_op_done(Anx7510 *me, uint8_t saddr)
{
	uint64_t t0 = timer_us(0);
	uint8_t val;

	while (1) {
		if (anx7510_read8(me, saddr, R_RAM_CTRL, &val) != 0)
			return -1;

		if (val & FLASH_DONE)
			break;

		mdelay(1);
		if (timer_us(t0) >= ANX_WAIT_TIMEOUT_US) {
			printf("anx7510.%d: wait timeout\n", me->ec_pd_id);
			return -1;
		}
	}

	return 0;
}

static int __must_check anx7510_flash_status_check(Anx7510 *me, uint8_t saddr)
{
	uint8_t val;
	uint64_t t0;

	t0 = timer_us(0);
	do {
		if (anx7510_set_bits(me, saddr, R_FLASH_CTRL_0, READ_STATUS_EN))
			return -1;

		if (anx7510_read8(me, saddr, R_FLASH_STATUS_REGISTER_READ_0, &val))
			return -1;

		if (val & WIP)
			mdelay(2);

		if (timer_us(t0) >= ANX_WAIT_TIMEOUT_US) {
			printf("anx7510.%d: wait timeout\n", me->ec_pd_id);
			return -1;
		}
	} while (val & WIP);

	return 0;
}

static int __must_check anx7510_flash_operation_init(Anx7510 *me, uint8_t saddr)
{
	int ret;

	/* Reset on-chip MCU, disable watch dog */
	if (anx7510_clr_bits(me, saddr, WATCH_DOG_CTRL,
			     (WATCH_DOG_RST_DISABLE | WATCH_DOG_ENABLE)))
		return -1;

	/* Set watch dog timer threshold */
	ret = anx7510_write8(me, saddr, WATCH_DOG_TIMER_TH_7_0, 0);
	ret |= anx7510_write8(me, saddr, WATCH_DOG_TIMER_TH_15_8, 0);
	ret |= anx7510_write8(me, saddr, WATCH_DOG_TIMER_TH_23_16, 0);
	/* Enable watch dog */
	ret |= anx7510_set_bits(me, saddr, WATCH_DOG_CTRL, WATCH_DOG_ENABLE);
	if (ret)
		return ret;

	/* Exit Deep power down mode */
	ret = anx7510_write8(me, saddr, FLASH_INSTRUCTION_TYPE, RELEASSEDP);
	ret |= anx7510_set_bits(me, saddr, R_FLASH_RW_CTRL,
				GENERAL_INSTRUCTION_EN);
	ret |= wait_flash_op_done(me, saddr);
	ret |= anx7510_flash_status_check(me, saddr);
	if (ret)
		return ret;

	/* Set flash length register as 0x20 */
	ret = anx7510_clr_bits(me, saddr, R_RAM_LEN_H, FLASH_LEN_16_MASK);
	ret |= anx7510_write8(me, saddr, R_FLASH_LEN_H, 0x0);
	ret |= anx7510_write8(me, saddr, R_FLASH_LEN_L, 0x1f);

	return ret;
}

static int __must_check anx7510_flash_write_en(Anx7510 *me, uint8_t saddr)
{
	int ret;

	/* Set flash instruction to WRITE */
	ret = anx7510_write8(me, saddr, FLASH_INSTRUCTION_TYPE, WRITE_EN);
	ret |= anx7510_write8(me, saddr, R_FLASH_RW_CTRL, GENERAL_INSTRUCTION_EN);
	ret |= wait_flash_op_done(me, saddr);
	ret |= anx7510_flash_status_check(me, saddr);

	return ret;
}

static int __must_check anx7510_flash_unprotect(Anx7510 *me, uint8_t saddr)
{
	int ret;

	/* Disable hardware protected mode */
	ret = anx7510_set_bits(me, saddr, WATCH_DOG_CTRL, FLASH_WRITE_PROTECT);
	ret |= anx7510_flash_write_en(me, saddr);
	if (ret)
		return ret;

	ret = anx7510_clr_bits(me, saddr, R_FLASH_STATUS_0,
			       FLASH_PROTECTION_PATTERN_MASK);
	ret |= anx7510_set_bits(me, saddr, R_FLASH_RW_CTRL, WRITE_STATUS_EN);
	ret |= wait_flash_op_done(me, saddr);
	ret |= anx7510_flash_status_check(me, saddr);
	if (ret)
		return ret;

	return anx7510_clr_bits(me, saddr, WATCH_DOG_CTRL, FLASH_WRITE_PROTECT);
}

static int __must_check anx7510_set_addr(Anx7510 *me, uint8_t saddr,
					 uint32_t flash_addr)
{
	int ret;

	/* Configure flash address */
	ret = anx7510_clrset_bits(me, saddr, R_RAM_LEN_H, FLASH_ADDR17_16_MASK,
				  (BITS17_16(flash_addr) << FLASH_RAM_ADDR_SHIFT) &
				  FLASH_ADDR17_16_MASK);
	ret |= anx7510_write8(me, saddr, R_FLASH_ADDR_H, BYTE1(flash_addr));
	ret |= anx7510_write8(me, saddr, R_FLASH_ADDR_L, BYTE0(flash_addr));

	return ret;
}

static int __must_check anx7510_flash_block_write(Anx7510 *me, uint8_t saddr,
						  uint32_t flash_addr,
						  const uint8_t *buf)
{
	int ret;
	uint8_t blank_data[DATA_BLOCK_SIZE];

	/* Skip the empty block */
	memset(blank_data, 0xff, DATA_BLOCK_SIZE);
	if (!memcmp(buf, blank_data, DATA_BLOCK_SIZE))
		return 0;

	/* Move data to registers */
	if (anx7510_block_write(me, saddr, FLASH_WRITE_DATA_0, buf, DATA_BLOCK_SIZE))
		return -1;

	/* Flash write enable */
	ret = anx7510_flash_write_en(me, saddr);
	if (ret)
		return ret;

	ret = anx7510_set_addr(me, saddr, flash_addr);
	ret |= anx7510_write8(me, saddr, R_FLASH_RW_CTRL, FLASH_WRITE);
	ret |= wait_flash_op_done(me, saddr);
	ret |= anx7510_flash_status_check(me, saddr);

	return ret;
}

static int __must_check anx7510_flash_block_read(Anx7510 *me, uint8_t saddr,
						 uint32_t flash_addr, uint8_t *buf)
{
	int ret;

	ret = anx7510_set_addr(me, saddr, flash_addr);
	ret |= anx7510_set_bits(me, saddr, R_FLASH_RW_CTRL, FLASH_READ);
	ret |= wait_flash_op_done(me, saddr);
	if (ret)
		return ret;

	if (anx7510_block_read(me, saddr, FLASH_READ_DATA_0, buf, DATA_BLOCK_SIZE))
		return -1;

	return 0;
}

static int __must_check anx7510_flash_verify(Anx7510 *me, uint8_t saddr,
					     uint32_t flash_addr,
					     const uint8_t *buf, size_t size)
{
	uint8_t data[DATA_BLOCK_SIZE];
	uint32_t base = flash_addr;
	uint32_t i;

	for (i = 0; i < size; i += DATA_BLOCK_SIZE) {
		if (anx7510_flash_block_read(me, saddr, base, data)) {
			debug("read flash data failed at %#x\n", base);
			return -1;
		}

		if (memcmp(data, &buf[i], DATA_BLOCK_SIZE)) {
			debug("Verify data failed at %#x\n", base);
			return -1;
		}

		base += DATA_BLOCK_SIZE;
	}

	return 0;
}

static int __must_check anx7510_flash_sector_erase(Anx7510 *me, uint8_t saddr,
						   uint32_t flash_addr)
{
	int ret;

	ret = anx7510_flash_write_en(me, saddr);
	if (ret) {
		debug("I2C access FLASH fail: GENERAL_CMD\n");
		return -1;
	}

	ret = anx7510_set_addr(me, saddr, flash_addr);
	ret |= anx7510_write8(me, saddr, FLASH_ERASE_TYPE, SECTOR_ERASE);
	ret |= anx7510_set_bits(me, saddr, R_FLASH_RW_CTRL, FLASH_ERASE_EN);
	ret |= wait_flash_op_done(me, saddr);
	ret |= anx7510_flash_status_check(me, saddr);
	if (ret)
		debug("I2C sector erase FLASH fail\n");

	return ret;
}

static int __must_check anx7510_flash_initial(Anx7510 *me, uint8_t saddr)
{
	debug("ANX7510 flash initial\n");

	if (anx7510_flash_operation_init(me, saddr)) {
		debug("ANX7510 flash operation initial failed\n");
		return -1;
	}

	if (anx7510_flash_unprotect(me, saddr)) {
		debug("ANX7510 flash disable protection failed\n");
		return -1;
	}

	debug("ANX7510 flash initial done\n");

	return 0;
}

static int __must_check anx7510_flash_erase(Anx7510 *me, uint8_t saddr,
					    uint32_t flash_addr_start,
					    uint32_t flash_addr_end)
{
	uint32_t addr;

	debug("ANX7510 flash erase start\n");

	for (addr = flash_addr_start; addr < flash_addr_end; addr += FLASH_SECTOR_SIZE)
		if (anx7510_flash_sector_erase(me, saddr, addr)) {
			debug("ANX7510 flash erase failed at %#x\n", addr);
			return -1;
		}

	debug("ANX7510 flash erase done\n");

	return 0;
}

static vb2_error_t anx7510_flash_write(Anx7510 *me, uint8_t saddr,
				       uint32_t flash_addr_start,
				       uint32_t flash_addr_end,
				       const uint8_t *buf,
				       const size_t size)
{
	uint32_t addr;

	debug("ANX7510 flash program start: %#x - %#x, size(%zx)\n",
	      flash_addr_start, flash_addr_end, size);

	if (flash_addr_end < flash_addr_start) {
		printf("flash_addr_end(%u) less than flash_addr_start(%u)\n",
		       flash_addr_end, flash_addr_start);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	if (size > flash_addr_end - flash_addr_start) {
		printf("Data total size(%zd), exceed physical flash size(%u)\n", size,
		       flash_addr_end - flash_addr_start);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	if (anx7510_flash_erase(me, saddr, flash_addr_start, flash_addr_end) != 0) {
		debug("anx7510.%d erase flash failed\n", me->ec_pd_id);
		return VB2_ERROR_UNKNOWN;
	}

	for (addr = flash_addr_start; addr < flash_addr_start + size;
	     addr += DATA_BLOCK_SIZE)
		if (anx7510_flash_block_write(me, saddr, addr,
					      &buf[addr - flash_addr_start])) {
			debug("flash write failed at address(%#x)\n", addr);
			return VB2_ERROR_UNKNOWN;
		}

	debug("ANX7510 flash program done\n");

	return VB2_SUCCESS;
}

static void anx7510_init_params(Anx7510 *me)
{
	me->last_offset = 0xFF;
	me->last_saddr = 0xFF;
}

/**
 * capture chip (vendor, device, rev) IDs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */
static int __must_check anx7510_capture_device_id(Anx7510 *me)
{
	uint8_t val0, val1;
	int ret;
	uint16_t vendor_id;

	ret = anx7510_read8(me, ANX7510_TCPC_ADDR, ANX7510_VENDOR_ID_L, &val0);
	ret |= anx7510_read8(me, ANX7510_TCPC_ADDR, ANX7510_VENDOR_ID_H, &val1);
	if (ret) {
		debug("Read firmware version failed\n");
		return ret;
	}

	vendor_id = val1 << 8 | val0;
	if (vendor_id != ANX7510_VENDOR_ID) {
		printf("anx7510.%d: Unsupport vendor 0x%04x\n", me->ec_pd_id, vendor_id);
		return -1;
	}
	me->chip.vendor = vendor_id;
	debug("vendor id: %#x\n", me->chip.vendor);

	ret = anx7510_read8(me, ANX7510_TCPC_ADDR, ANX7510_FW_VERSION, &val0);
	ret |= anx7510_read8(me, ANX7510_TCPC_ADDR, ANX7510_FW_SUBVERSION, &val1);
	if (ret) {
		debug("Read main ocm firmware version failed\n");
		return ret;
	}
	me->chip.fw_rev = val0 << 8 | val1;
	debug("Main ocm firmware version: %#x\n", me->chip.fw_rev);

	ret = anx7510_read8(me, ANX7510_EMRB_ADDR, ANX7510_CUST_DEF_VER, &val0);
	ret |= anx7510_read8(me, ANX7510_EMRB_ADDR, ANX7510_CUST_DEF_SUBVER, &val1);
	if (ret) {
		debug("Read cust define area version failed\n");
		return ret;
	}
	me->chip.cust_rev = val0 << 8 | val1;
	debug("Cust define area version: %#x\n", me->chip.cust_rev);

	ret = anx7510_read8(me, ANX7510_HDMI_TX_ADDR,
			    ANX7510_HDCP_TX_VERSION, &val0);
	ret |= anx7510_read8(me, ANX7510_HDMI_TX_ADDR,
			     ANX7510_HDCP_TX_SUBVERSION, &val1);
	if (ret) {
		debug("Read hdcp TX firmware version failed\n");
		return ret;
	}
	me->chip.hdcp_tx_rev = val0 << 8 | val1;
	debug("HDCP TX firmware version: %#x\n", me->chip.hdcp_tx_rev);

	ret = anx7510_read8(me, ANX7510_DP_RX_ADDR,
			    ANX7510_HDCP_RX_VERSION, &val0);
	ret |= anx7510_read8(me, ANX7510_DP_RX_ADDR,
			     ANX7510_HDCP_RX_SUBVERSION, &val1);
	if (ret) {
		debug("Read hdcp TX firmware version failed\n");
		return ret;
	}
	me->chip.hdcp_rx_rev = val0 << 8 | val1;
	debug("HDCP RX firmware version: %#x\n", me->chip.hdcp_rx_rev);

	return 0;
}

static void anx7510_clear_device_id(Anx7510 *me)
{
	memset(&me->chip, 0, sizeof(me->chip));
}

static int __must_check anx7510_update_pointer(Anx7510 *me, uint8_t saddr,
					       uint8_t section)
{
	uint32_t base = ANX7510_FW_POINTER_OFFSET;
	uint32_t start_addr = base + ANX7510_FW_POINTER_LEN - DATA_BLOCK_SIZE;
	uint8_t data[DATA_BLOCK_SIZE];
	int ret;

	memset(data, 0xFF, DATA_BLOCK_SIZE);

	/* Set pointer value, 0x00 means section B, 0xFF means section A */
	if (section == ANX7510_FW_SECTION_B)
		data[0x10] = 0x0;

	debug("update section(%d) pointer\n", section);

	if (anx7510_flash_erase(me, saddr, base, base + FLASH_SECTOR_SIZE) != 0) {
		debug("anx7510.%d erase flash failed\n", me->ec_pd_id);
		return -1;
	}

	/* Set flash length register as 0x20 */
	ret = anx7510_clr_bits(me, saddr, R_RAM_LEN_H, FLASH_LEN_16_MASK);
	ret |= anx7510_write8(me, saddr, R_FLASH_LEN_H, 0x0);
	ret |= anx7510_write8(me, saddr, R_FLASH_LEN_L, DATA_BLOCK_SIZE - 1);
	if (ret) {
		debug("Set flash length failed\n");
		return ret;
	}

	/* update pointer */
	if (anx7510_flash_block_write(me, saddr, start_addr, data))
		return -1;

	debug("update section(%d) pointer done\n", section);

	if (anx7510_flash_verify(me, ANX7510_TOP_ADDR, start_addr,
				 data, DATA_BLOCK_SIZE)) {
		debug("Flash verify(pointer) failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	debug("Flash data(pointer) verify succeeded\n");

	return 0;
}

static vb2_error_t anx7510_update_image(const VbootAuxfwOps *vbaux,
					const uint8_t *image,
					const size_t image_size,
					uint32_t base, uint32_t len,
					bool verify)
{
	Anx7510 *me = container_of(vbaux, Anx7510, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	int protected;

	debug("call...\n");

	if (cros_ec_tunnel_i2c_protect_status(me->bus, &protected) < 0) {
		printf("anx7510.%d: could not get EC I2C tunnel status!\n",
		       me->ec_pd_id);
		return VB2_ERROR_UNKNOWN;
	}

	if (protected) {
		debug("already protected\n");
		return VB2_REQUEST_REBOOT_EC_TO_RO;
	}

	if (anx7510_flash_initial(me, ANX7510_TOP_ADDR)) {
		debug("flash initial failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	status = anx7510_flash_write(me, ANX7510_TOP_ADDR, base,
				     base + len, image, image_size);
	if (status != VB2_SUCCESS) {
		debug("flash write failed\n");
		return status;
	}

	debug("Flash data program succeeded\n");

	if (!verify)
		goto out;

	if (anx7510_flash_verify(me, ANX7510_TOP_ADDR, base, image, image_size)) {
		debug("Flash verify failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	debug("Flash data verify succeeded\n");

out:
	/* force re-read */
	anx7510_clear_device_id(me);

	return status;
}

static int anx7510_validate_flash(Anx7510 *me, uint8_t saddr, uint32_t flash_addr)
{
	uint8_t val;
	int ret;
	uint64_t t0;
	uint16_t fw_len_shift = ANX7510_FW_LEN >> FLASH_RAM_SIZE_SHIFT;

	/* Set flash address */
	ret = anx7510_set_addr(me, saddr, flash_addr);
	/* Set RAM start address */
	ret |= anx7510_write8(me, saddr, R_RAM_ADDR_H, 0);
	ret |= anx7510_write8(me, saddr, R_RAM_ADDR_L, 0);
	/* Set flash length */
	ret |= anx7510_clrset_bits(me, saddr, R_RAM_LEN_H, FLASH_LEN_16_MASK,
				   BYTE1(fw_len_shift) & FLASH_LEN_16_MASK);
	ret |= anx7510_write8(me, saddr, R_FLASH_LEN_H, BYTE1(ANX7510_FW_LEN));
	ret |= anx7510_write8(me, saddr, R_FLASH_LEN_L, BYTE0(ANX7510_FW_LEN));
	/* Set SRAM length */
	ret |= anx7510_clrset_bits(me, saddr, R_RAM_LEN_H, SRAM_LEN_11_8_MASK,
				   BYTE1(fw_len_shift) & SRAM_LEN_11_8_MASK);
	ret |= anx7510_clrset_bits(me, saddr, R_FLASH_CTRL_0, SRAM_LEN_12_MASK,
				   (BITS17_16(ANX7510_FW_LEN) << FLASH_LEN_SHIFT) &
				   SRAM_LEN_12_MASK);
	ret |= anx7510_write8(me, saddr, R_RAM_LEN_L, BYTE0(fw_len_shift));
	/* Set SRAM load type and load start */
	ret |= anx7510_clrset_bits(me, saddr, R_BOOT_RETRY, SRAM_CS_MASK,
				   LOAD_FW << SRAM_CS_SHIFT);
	ret |= anx7510_clrset_bits(me, saddr, R_RAM_CTRL, DECRYPT_EN, LOAD_START);
	if (ret) {
		debug("config flash load register failed\n");
		return -1;
	}

	t0 = timer_us(0);
	do {
		mdelay(1);
		if (anx7510_read8(me, saddr, R_RAM_CTRL, &val)) {
			debug("anx7510.%d: read RAM_CTRL failed\n", me->ec_pd_id);
			return -1;
		}

		if (timer_us(t0) >= ANX_WAIT_TIMEOUT_US) {
			debug("anx7510.%d: flash load timeout\n", me->ec_pd_id);
			return -1;
		}
	} while (!(val & LOAD_DONE));

	if (!(val & CRC_OK)) {
		debug("flash load CRC check failed\n");
		return -1;
	}

	return 0;
}

static vb2_error_t anx7510_ocm_update_image(const VbootAuxfwOps *vbaux,
					    const uint8_t *image,
					    const size_t image_size)
{
	Anx7510 *me = container_of(vbaux, Anx7510, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	uint8_t val;
	uint8_t prog_section;
	uint32_t base;

	if (image_size <= ANX7510_FW_HDCP_TX_LEN) {
		debug("image_size(%zu) too small\n", image_size);
		return VB2_ERROR_UNKNOWN;
	}

	if (anx7510_read8(me, ANX7510_TOP_ADDR, FLASH_STATUS_11, &val)) {
		debug("failed to get flash current section\n");
		return VB2_ERROR_UNKNOWN;
	}

	prog_section = val & ANX7510_FW_SECTION_B ?
			ANX7510_FW_SECTION_A : ANX7510_FW_SECTION_B;
	switch (prog_section) {
	default:
	case ANX7510_FW_SECTION_A:
		base = ANX7510_FW_SECTION_A_OFFSET;
		break;
	case ANX7510_FW_SECTION_B:
		base = ANX7510_FW_SECTION_B_OFFSET;
		break;
	}

	debug("OCM fw will update to section:%d\n", prog_section);

	status = anx7510_update_image(vbaux, image, image_size,
				      base, ANX7510_FW_LEN, false);
	if (status != VB2_SUCCESS)
		return status;

	debug("OCM fw updated to section:%d done\n", prog_section);

	if (anx7510_validate_flash(me, ANX7510_TOP_ADDR, base)) {
		debug("anx7510 firmware validation failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (anx7510_update_pointer(me, ANX7510_TOP_ADDR, prog_section)) {
		debug("update flash section pointer failed\n");
		return VB2_ERROR_UNKNOWN;
	}

	debug("Flash update pointer succeeded\n");

	return status;
}

static vb2_error_t anx7510_cust_update_image(const VbootAuxfwOps *vbaux,
					     const uint8_t *image,
					     const size_t image_size)
{
	return anx7510_update_image(vbaux, image, image_size,
				    ANX7510_FW_CUST_DEF_OFFSET,
				    ANX7510_FW_CUST_DEF_LEN, true);
}

static vb2_error_t anx7510_tx_update_image(const VbootAuxfwOps *vbaux,
					   const uint8_t *image,
					   const size_t image_size)
{
	return anx7510_update_image(vbaux, image, image_size,
				    ANX7510_FW_HDCP_TX_OFFSET,
				    ANX7510_FW_HDCP_TX_LEN, true);
}

static vb2_error_t anx7510_rx_update_image(const VbootAuxfwOps *vbaux,
					   const uint8_t *image,
					   const size_t image_size)
{
	return anx7510_update_image(vbaux, image, image_size,
				    ANX7510_FW_HDCP_RX_OFFSET,
				    ANX7510_FW_HDCP_RX_LEN, true);
}

static vb2_error_t anx7510_check_hash(const VbootAuxfwOps *vbaux,
				      const uint8_t *hash,
				      size_t hash_size,
				      enum vb2_auxfw_update_severity *severity)
{
	Anx7510 *me = container_of(vbaux, Anx7510, fw_ops);
	uint16_t fw_hash = (hash[0] << 8) | hash[1];

	debug("call...\n");

	if (hash_size != 2)
		return VB2_ERROR_INVALID_PARAMETER;

	if (anx7510_capture_device_id(me) != 0) {
		printf("Not anx7510 or bus error, exit.\n");
		*severity = VB2_AUXFW_NO_DEVICE;
		return VB2_SUCCESS;
	}

	if (hash_size != 2)
		return VB2_ERROR_INVALID_PARAMETER;

	if (!me->get_hash) {
		printf("No get_hash found\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (fw_hash == me->get_hash(me)) {
		if (ANX7510_DEBUG >= 2 && !me->debug_updated) {
			debug("forcing VB2_AUXFW_SLOW_UPDATE\n");
			*severity = VB2_AUXFW_SLOW_UPDATE;
		} else {
			*severity = VB2_AUXFW_NO_UPDATE;
			return VB2_SUCCESS;
		}
	} else {
		*severity = VB2_AUXFW_SLOW_UPDATE;
	}

	return VB2_SUCCESS;
}

static uint16_t anx7510_ocm_get_hash(Anx7510 *me)
{
	return me->chip.fw_rev;
}

static uint16_t anx7510_cust_get_hash(Anx7510 *me)
{
	return me->chip.cust_rev;
}

static uint16_t anx7510_tx_get_hash(Anx7510 *me)
{
	return me->chip.hdcp_tx_rev;
}

static uint16_t anx7510_rx_get_hash(Anx7510 *me)
{
	return me->chip.hdcp_rx_rev;
}

static const VbootAuxfwOps anx7510_fw_ops = {
	.fw_image_name = "anx7510_ocm.bin",
	.fw_hash_name = "anx7510_ocm.hash",
	.check_hash = anx7510_check_hash,
	.update_image = anx7510_ocm_update_image,
};

/* main ocm firmware update */
Anx7510 *new_anx7510_ocm(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx7510 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx7510_fw_ops;
	me->get_hash = anx7510_ocm_get_hash;
	anx7510_init_params(me);

	return me;
}

static const VbootAuxfwOps anx7510_cust_fw_ops = {
	.fw_image_name = "anx7510_cust.bin",
	.fw_hash_name = "anx7510_cust.hash",
	.check_hash = anx7510_check_hash,
	.update_image = anx7510_cust_update_image,
};

/* customer define area update */
Anx7510 *new_anx7510_cust(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx7510 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx7510_cust_fw_ops;
	me->get_hash = anx7510_cust_get_hash;
	anx7510_init_params(me);

	return me;
}

static const VbootAuxfwOps anx7510_rx_fw_ops = {
	.fw_image_name = "anx7510_rx.bin",
	.fw_hash_name = "anx7510_rx.hash",
	.check_hash = anx7510_check_hash,
	.update_image = anx7510_rx_update_image,
};

/* HDCP RX firmware update */
Anx7510 *new_anx7510_rx(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx7510 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx7510_rx_fw_ops;
	me->get_hash = anx7510_rx_get_hash;
	anx7510_init_params(me);

	return me;
}

static const VbootAuxfwOps anx7510_tx_fw_ops = {
	.fw_image_name = "anx7510_tx.bin",
	.fw_hash_name = "anx7510_tx.hash",
	.check_hash = anx7510_check_hash,
	.update_image = anx7510_tx_update_image,
};

/* HDCP TX firmware update */
Anx7510 *new_anx7510_tx(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx7510 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx7510_tx_fw_ops;
	me->get_hash = anx7510_tx_get_hash;
	anx7510_init_params(me);

	return me;
}
