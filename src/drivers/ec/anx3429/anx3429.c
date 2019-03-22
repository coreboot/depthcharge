/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * MCU - microcontroller
 * OCM - on chip microcontroller
 * OTP - one time programmable (ROM)
 *	16K words of 64 bit data + 8 bit ECC
 *
 * fun fact - it's an 18MHz 8051
 */

#include <endian.h>
#include <libpayload.h>
#include <stddef.h>

#include "base/container_of.h"
#include "drivers/ec/anx3429/anx3429.h"

/*
 * enable debug code.
 * level 1 adds some logging.
 * level 2 forces FW update, even more logging.
 */
#define ANX3429_DEBUG	0

/*
 * set ANX3429_SCRATCH to 1 to program an unused region of the OTP
 * memory instead of the precious area that the OCM actually boots
 * from.
 */
#define ANX3429_SCRATCH	0

#if (ANX3429_DEBUG > 0)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#define dprintf(msg, args...)	printf(msg, ##args)
#else
#define debug(msg, args...)
#define dprintf(msg, args...)
#endif

#define ANX_TIMEOUT_US		(1 * USECS_PER_SEC)	/* 1s */
#define ANX_RESTART_MS		(1 * MSECS_PER_SEC)	/* 1s */

/* OTP memory layout */

/* these are word offsets */

#define OTP_DIR_SLOT0		2
#define OTP_UPDATE_MAX		100
#define OTP_WORDS_MAX		0x4000		/* 128KB + ECC */
#define OTP_WORD_SIZE		9		/* 8B + ECC */
#define OTP_PROG_CYCLES		6		/* OTP writes cycles */
#define OTP_PROG_FAILED_TRY_CNT	3		/* OTP program failed try count */
#define OTP_HEADER_0BIT_LIMIT	10		/* max 0's in inactive header */

#define OTP_SCRATCH_SLOT0	(0x3900 + OTP_DIR_SLOT0)

/* these are byte offsets */

#define ANX_UPD_LOC_BOFFSET	(2 * OTP_WORD_SIZE + 0)
#define ANX_UPD_SIZE_BOFFSET	(2 * OTP_WORD_SIZE + 2)
#define ANX_UPD_BLOB_WOFFSET	3
#define ANX_UPD_BLOB_BOFFSET	(ANX_UPD_BLOB_WOFFSET * OTP_WORD_SIZE)
#define ANX_UPD_VERSION_BOFFSET	0x49b

#define ANX_FW_I2C_ADDR		(0x50 >> 1)

/* this is surely a bogus number... */
#define ANALOGIX_VENDOR_ID		0xaaaa

#define TCPCI_VENDOR_ID_LOW		0x00

#define TCPC_REG_POWER_STATUS		0x1e

#define ANX3429_REG_FW_VERSION		0x44

#define RESET_CTRL_0			0x05
#define OCM_RESET			0x10

#define POWER_DOWN_CTRL			0x0d
#define POWER_DOWN_CTRL_DEFAULT		0x00
#define R_POWER_DOWN_CTRL_OCM		0x02

#define R_EE_CTL_1			0x60
#define R_EE_CTL_1_READ			0xca
#define R_EE_ADDR_L_BYTE		0x62
#define R_EE_ADDR_H_BYTE		0x63
#define R_EE_RD_DATA			0x65
#define R_EE_STATE			0x66
#define R_EE_DATA0			0x67	/* DATA0..7: 0x67..0x6e */
#define R_EE_STATE_RW_DONE		0x01

#define REG_0X0D			0x0d
#define R_OTP_ADDR_HIGH			0xd0
#define R_OTP_ADDR_LOW			0xd1
#define R_OTP_DATA_IN_0			0xd2
#define R_OTP_REPROG_MAX_NUM		0xda
#define R_OTP_ECC_IN			0xdb

#define R_OTP_CTL_1			0xe5
#define R_OTP_CTL_1_DEFAULT		0x00
#define R_OTP_CTL_1_MCU_ACC_DIS		0x80
#define R_OTP_CTL_1_OTP_WAKE		0x20
#define R_OTP_CTL_1_ECC_BYPASS		0x08
#define R_OTP_CTL_1_OTP_WRITE		0x02
#define R_OTP_CTL_1_OTP_READ		0x01

#define R_OTP_CTL_1_RW_OTP72ECC (		\
		R_OTP_CTL_1_MCU_ACC_DIS |	\
		R_OTP_CTL_1_OTP_WAKE)
#define R_OTP_CTL_1_RW_OTP72RAW	(		\
		R_OTP_CTL_1_RW_OTP72ECC |	\
		R_OTP_CTL_1_ECC_BYPASS)

#define xxx_R_OTP_CTL_1_READ_OTP72ECC (		\
		R_OTP_CTL_1_RW_OTP72ECC |	\
		R_OTP_CTL_1_OTP_READ)

#define xxx_R_OTP_CTL_1_READ_OTP72RAW (		\
		R_OTP_CTL_1_RW_OTP72RAW |	\
		R_OTP_CTL_1_OTP_READ)
#define R_OTP_CTL_1_WRITE_OTP72RAW (		\
		R_OTP_CTL_1_RW_OTP72RAW |	\
		R_OTP_CTL_1_OTP_WRITE)

#define R_OTP_DATA_OUT_0		0xdc	/* 0xdc..0xe4 (8B+ECC) */

#define R_OTP_STATE_1			0xed
#define R_OTP_STATE_1_READ_STATE_MASK	0x30
#define R_OTP_STATE_1_WRITE_STATE_MASK	0x0f

#define R_OTP_ACC_PROTECT		0xef
#define R_OTP_ACC_PROTECT_DEFAULT	0x00
#define R_OTP_ACCESS_PROTECT_UNLOCK	0x7a
#define R_OTP_ACCESS_PROTECT_LOCK	0x00

static int __must_check read_reg(Anx3429 *me, uint8_t reg, uint8_t *data)
{
	return i2c_readb(&me->bus->ops, ANX_FW_I2C_ADDR, reg, data);
}

static int __must_check anx3429_read_block(Anx3429 *me, uint8_t reg,
					   uint8_t *data, size_t len)
{
	return i2c_readblock(&me->bus->ops, ANX_FW_I2C_ADDR, reg, data, len);
}

static int __must_check write_reg(Anx3429 *me, uint8_t reg, uint8_t data)
{
	return i2c_writeb(&me->bus->ops, ANX_FW_I2C_ADDR, reg, data);
}

/**
 * issue a series of i2c writes to ANX_FW_I2C_ADDR
 *
 * @param me	device context
 * @param cmds	vector of i2c reg write commands
 * @param count	number of elements in ops vector
 * @return 0 if ok, -1 on error
 */

static int __must_check write_regs(Anx3429 *me,
				   const I2cWriteVec *cmds, const size_t count)
{
	return i2c_write_regs(&me->bus->ops, ANX_FW_I2C_ADDR, cmds, count);
}

static int __must_check write_block(Anx3429 *me, uint8_t reg,
				    const uint8_t *data, size_t len)
{
	return i2c_writeblock(&me->bus->ops, ANX_FW_I2C_ADDR, reg, data, len);
}

/*
 * note: assumes register pairs are big-endian
 *	lower numbered reg gets the MSB
 */

static int __must_check write_reg16(Anx3429 *me, uint8_t reg, uint16_t val)
{
	uint8_t buf[2];

	buf[0] = val >> 8;
	buf[1] = val;
	return write_block(me, reg, buf, sizeof(buf));
}

static const uint8_t inactive_word[OTP_WORD_SIZE] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const uint8_t _blank_word[OTP_WORD_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int is_inactive_word(const uint8_t *w)
{
	return memcmp(w, inactive_word, sizeof(inactive_word)) == 0;
}

static int is_blank_word(const uint8_t *w)
{
	return memcmp(w, _blank_word, sizeof(_blank_word)) == 0;
}

/*
 * determine if the 72 bit word at <from> can be changed to the 72 bit
 * word at <to>.  the only permitted operation is to change 0s to 1s.
 */

static int is_mutable_word(const uint8_t *from, const uint8_t *to)
{
	for (int i = 0; i < OTP_WORD_SIZE; ++i) {
		uint8_t zmask = ~to[i];
		if ((from[i] & zmask) != 0)
			return 0;
	}
	return 1;
}

static void print_otp_word(const uint8_t *otp_word)
{
	for (int i = 0; i < OTP_WORD_SIZE; ++i)
		printf(" %02x", otp_word[i]);
}

static const uint8_t hamming_table[] = {
	0x0b, 0x3b, 0x37, 0x07, 0x19, 0x29, 0x49, 0x89,
	0x16, 0x26, 0x46, 0x86, 0x13, 0x23, 0x43, 0x83,
	0x1c, 0x2c, 0x4c, 0x8c, 0x15, 0x25, 0x45, 0x85,
	0x1a, 0x2a, 0x4a, 0x8a, 0x0d, 0xcd, 0xce, 0x0e,
	0x70, 0x73, 0xb3, 0xb0, 0x51, 0x52, 0x54, 0x58,
	0xa1, 0xa2, 0xa4, 0xa8, 0x31, 0x32, 0x34, 0x38,
	0xc1, 0xc2, 0xc4, 0xc8, 0x61, 0x62, 0x64, 0x68,
	0x91, 0x92, 0x94, 0x98, 0xe0, 0xec, 0xdc, 0xd0
};

static int __must_check anx3429_otp_read72(
	Anx3429 *me,
	uint32_t offset,
	uint8_t *buf,
	uint8_t flags)
{
	uint64_t t0_us;

	if (write_reg16(me, R_OTP_ADDR_HIGH, offset) != 0)
		return -1;
	if (write_reg(me, R_OTP_CTL_1, flags|R_OTP_CTL_1_OTP_READ) != 0)
		return -1;

	t0_us = timer_us(0);
	for (;;) {
		uint8_t val;

		if (read_reg(me, R_OTP_STATE_1, &val) != 0)
			return -1;
		if ((val & R_OTP_STATE_1_READ_STATE_MASK) == 0)
			break;
		if (timer_us(t0_us) >= ANX_TIMEOUT_US) {
			printf("anx3429.%d: read timeout after %ums\n",
			       me->ec_pd_id, ANX_TIMEOUT_US / USECS_PER_MSEC);
			return -1;
		}
	}

	if (anx3429_read_block(me, R_OTP_DATA_OUT_0, buf, OTP_WORD_SIZE) != 0)
		return -1;
	return 0;
}

/* read OTP word, ECC bypassed */

static int __must_check anx3429_otp_read72raw(Anx3429 *me,
					      uint32_t offset, uint8_t *buf)
{
	return anx3429_otp_read72(me, offset, buf, R_OTP_CTL_1_RW_OTP72RAW);
}

/* read OTP word, ECC corrected */

static int __must_check anx3429_otp_read72ecc(Anx3429 *me,
					      uint32_t offset, uint8_t *buf)
{
	return anx3429_otp_read72(me, offset, buf, R_OTP_CTL_1_RW_OTP72ECC);
}

static VbError_t anx3429_ec_tunnel_status(const VbootAuxFwOps *vbaux,
					  int *protected)
{
	Anx3429 *const me = container_of(vbaux, Anx3429, fw_ops);

	if (cros_ec_tunnel_i2c_protect_status(me->bus, protected) < 0) {
		printf("anx3429.%d: could not get EC I2C tunnel status!\n",
		       me->ec_pd_id);
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

static int anx3429_ec_pd_suspend(Anx3429 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_SUSPEND);
	if (status == -EC_RES_BUSY)
		printf("anx3429.%d: PD_SUSPEND busy! Could be only power "
		       "source.\n",
		       me->ec_pd_id);
	else  if (status != 0)
		printf("anx3429.%d: PD_SUSPEND failed!\n", me->ec_pd_id);
	return status;
}

static int anx3429_ec_pd_resume(Anx3429 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_RESUME);
	if (status != 0)
		printf("anx3429.%d: PD_RESUME failed!\n", me->ec_pd_id);
	return status;
}

static int anx3429_ec_pd_powerup(Anx3429 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_CHIP_ON);
	if (status != 0)
		printf("anx3429.%d: PD_CHIP_ON failed!\n", me->ec_pd_id);
	return status;
}

/*
 * program OTP word.
 * programming is somewhat temperamenal even though
 * R_OTP_REPROG_MAX_NUM is 9.
 * must be performed OTP_PROG_CYCLES times.
 */

static int __must_check anx3429_otp_write72raw(Anx3429 *me,
					       uint16_t offset,
					       const uint8_t *word72)
{
	uint64_t t0_us;

	if (write_reg16(me, R_OTP_ADDR_HIGH, offset) != 0)
		return -1;

	if (write_block(me, R_OTP_DATA_IN_0, word72, 8) != 0)
		return -1;
	if (write_reg(me, R_OTP_ECC_IN, word72[8]) != 0)
		return -1;

	if (write_reg(me, R_OTP_CTL_1, R_OTP_CTL_1_WRITE_OTP72RAW) != 0)
		return -1;

	t0_us = timer_us(0);
	for (;;) {
		uint8_t val;

		if (read_reg(me, R_OTP_STATE_1, &val) != 0)
			return -1;
		if ((val & R_OTP_STATE_1_WRITE_STATE_MASK) == 0)
			break;
		if (timer_us(t0_us) >= ANX_TIMEOUT_US) {
			printf("anx3429.%d: write timeout after %ums\n",
			       me->ec_pd_id, ANX_TIMEOUT_US / USECS_PER_MSEC);
			return -1;
		}
	}
	return 0;
}

/*
 * program OTP word and verify.
 * if we get an exact match, we're good.
 * else, see if we get a match with ECC, good enough.
 * else, fail.
 *
 * returns 0 on success
 */

static int __must_check anx3429_otp_prog72verify(Anx3429 *me,
						 uint16_t offset,
						 const uint8_t *word72)
{
	uint8_t otp_word_raw[OTP_WORD_SIZE];
	uint8_t otp_word_ecc[OTP_WORD_SIZE];
	int cycles;

	for (cycles = 0; cycles < OTP_PROG_CYCLES; ++cycles) {
		if (anx3429_otp_write72raw(me, offset, word72))
			return -1;
	}
	if (anx3429_otp_read72raw(me, offset, otp_word_raw) != 0)
		return -1;
	if (memcmp(word72, otp_word_raw, sizeof(otp_word_raw)) == 0)
		return 0;

	/*
	 * programming failed, let's see if ECC saves the day
	 */
	if (anx3429_otp_read72ecc(me, offset, otp_word_ecc) != 0)
		return -1;
	if (memcmp(word72, otp_word_ecc, sizeof(otp_word_ecc)) == 0) {
		printf("anx3429.%d: programmed word at 0x%04x "
		       "with soft ECC error!\n",
		       me->ec_pd_id, offset);
		return 0;
	}
	printf("anx3429.%d: programming word at 0x%04x failed!\n",
	       me->ec_pd_id, offset);
	printf("write ");
	print_otp_word(word72);
	printf("\nread");
	print_otp_word(otp_word_raw);
	printf("\nrECC");
	print_otp_word(otp_word_ecc);
	printf("\n");
	return -1;
}

static uint8_t anx3429_hamming_encoder(const uint8_t *data)
{
	int i, j;
	uint8_t result = 0;
	uint8_t c;

	for (i = 0; i < 8; ++i) {
		c = data[i];
		for (j = 0; j < 8; ++j) {
			if (c & 0x01)
				result ^= hamming_table[(i << 3) + j];
			c >>= 1;
		}
	}
	return result;
}

static int zeros_in_byte(uint8_t u)
{
	int count;

	u = ~u;
	/* K&R method */
	for (count = 0; u; ++count)
		u &= u - 1;
	return count;
}

static int zeros_in_word(const uint8_t *w)
{
	int count = 0;

	for (int i = 0; i < OTP_WORD_SIZE; ++i)
		count += zeros_in_byte(w[i]);
	return count;
}

/*
 * checks for inactive header signature:  should be all 0xff.
 * there is no ECC protection on this signature, so we try to be
 * lenient and allow up to OTP_HEADER_0BIT_LIMIT bits of error.
 *
 * the OCM is strict and requires all 0xff to consider the header word
 * inactive, so we try to correct them by re-writing the header word.
 */

static int anx3429_likely_inactive_header(Anx3429 *me,
					  uint16_t offset,
					  const uint8_t *w)
{
	int flipped_bits;

	if (is_inactive_word(w))
		return 1;
	/*
	 * the inactive header pattern does not have a valid ECC,
	 * so allow up to OTP_HEADER_0BIT_LIMIT bits of error
	 */
	flipped_bits = zeros_in_word(w);
	if (flipped_bits > 0)
		dprintf("header word at 0x%04x has %u zeros\n",
			offset, flipped_bits);

	if (flipped_bits <= OTP_HEADER_0BIT_LIMIT) {
		/*
		 * partially erased (inactive) header
		 * if we don't correct this, the OCM gets confused
		 */
		printf("anx3429.%d: "
		       "fixing FW header at offset 0x%04x with %u zeros\n",
		       me->ec_pd_id, offset, flipped_bits);
		if (anx3429_otp_prog72verify(me, offset, inactive_word) == 0)
			return 1;
	}
	return 0;
}

/*
 * returns offset of active header in directory <dir> or next
 * usable header.  invalidates any obviously bogus headers.
 */

static int anx3429_find_active_header(Anx3429 *me,
				      const uint16_t dir,
				      uint16_t *act_start,
				      uint16_t *act_size)
{
	uint8_t word_raw[OTP_WORD_SIZE];
	uint8_t word_ecc[OTP_WORD_SIZE];
	uint16_t offset;
	uint16_t start;
	uint16_t size;

	for (offset = dir; offset < dir + OTP_UPDATE_MAX; ++offset) {
		if (anx3429_otp_read72raw(me, offset, word_raw) != 0)
			return -1;
		if (anx3429_likely_inactive_header(me, offset, word_raw))
			continue;
		if (anx3429_otp_read72ecc(me, offset, word_ecc) != 0)
			return -1;
		/*
		 * a blank word is ECC-correct, no need to special case
		 */
		start = le16dec(word_ecc);
		size = le16dec(word_ecc + 2);
		if (!is_blank_word(word_ecc) &&
		    (start < dir + OTP_UPDATE_MAX ||
		     start + size > OTP_WORDS_MAX)) {
			printf("anx3429.%d: fixing bogus FW header "
			       "at offset 0x%04x: ",
			       me->ec_pd_id, offset);
			print_otp_word(word_ecc);
			printf("\n");
			/* mask bogus header and skip */
			if (anx3429_otp_prog72verify(
				    me, offset, inactive_word) != 0)
				return -1;
			continue;
		}
		*act_start = start;
		*act_size = size;
		return offset;
	}
	printf("anx3429.%d: no valid FW header found!\n", me->ec_pd_id);
	return -1;
}

/*
 * return offset of fw_words contiguous words starting at offset
 * otp_start.
 *
 * normally, we'll only encounter blank words, but if the previous
 * update was interrutped, we can hopefully pick up where we left off.
 * if the previous programming corrupted the OTP, just move on to
 * pristine memory.
 */

static int anx3429_find_usable_otp(Anx3429 *me,
				   const uint16_t otp_start,
				   const uint8_t *fw_data,
				   const uint16_t fw_words)
{
	uint8_t word[OTP_WORD_SIZE];
	uint16_t otp_offset;
	uint16_t fw_offset;

	if (otp_start + fw_words > OTP_WORDS_MAX) {
		printf("anx3429.%d: no space for FW update!\n", me->ec_pd_id);
		return -1;
	}

	otp_offset = otp_start;
	fw_offset = 0;
	while (fw_offset < fw_words) {
		if (anx3429_otp_read72ecc(me, otp_offset, word) != 0)
			return -1;
		++otp_offset;
		if (!is_mutable_word(word,
				     &fw_data[fw_offset * OTP_WORD_SIZE])) {
			/* hit unusable word */
			if (otp_offset + fw_words > OTP_WORDS_MAX) {
				printf("anx3429.%d: no space for FW update!\n",
				       me->ec_pd_id);
				return -1;
			}
			fw_offset = 0;
		} else
			++fw_offset;
	}
	return otp_offset - fw_words;
}

static int __must_check anx3429_disable_mcu(Anx3429 *me)
{
	/*
	 * power down OCM
	 * disable OCM access to OTP, wake up OTP, bypass ECC
	 * unlock OTP access
	 */
	static const I2cWriteVec wc[] = {
		{ RESET_CTRL_0, OCM_RESET},
		{ POWER_DOWN_CTRL, R_POWER_DOWN_CTRL_OCM },
		{ R_OTP_ACC_PROTECT, R_OTP_ACCESS_PROTECT_UNLOCK },
	};

	debug("call...\n");

	if (write_regs(me, wc, ARRAY_SIZE(wc)) != 0)
		return -1;
	return 0;
}

static int __must_check anx3429_enable_mcu(Anx3429 *me)
{
	static const I2cWriteVec wc[] = {
		{ R_OTP_ACC_PROTECT, R_OTP_ACC_PROTECT_DEFAULT },
		{ R_OTP_CTL_1, R_OTP_CTL_1_DEFAULT },
		{ POWER_DOWN_CTRL, POWER_DOWN_CTRL_DEFAULT },
	};

	debug("call...\n");

	if (write_regs(me, wc, ARRAY_SIZE(wc)) != 0)
		return -1;
	mdelay(11);
	return 0;
}

/**
 * capture chip (vendor, product, device, rev) IDs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check anx3429_capture_device_id(Anx3429 *me)
{
	struct ec_params_pd_chip_info p;
	struct ec_response_pd_chip_info r;

	if (me->chip.vendor != 0)
		return 0;

	p.port = me->ec_pd_id;
	p.renew = 0;
	int status = ec_command(me->bus->ec, EC_CMD_PD_CHIP_INFO, 0,
				&p, sizeof(p), &r, sizeof(r));
	if (status < 0) {
		printf("anx3429.%d: could not get chip info!\n", me->ec_pd_id);
		return -1;
	}

	uint16_t vendor  = r.vendor_id;
	uint16_t product = r.product_id;
	uint16_t device  = r.device_id;
	uint8_t fw_rev = r.fw_version_number;

	printf("anx3429.%d: vendor 0x%04x product 0x%04x "
	       "device 0x%04x fw_rev 0x%02x\n",
	       me->ec_pd_id, vendor, product, device, fw_rev);

	if (vendor != ANALOGIX_VENDOR_ID) {
		printf("anx3429.%d: vendor 0x%04x, expected %04x\n",
		       me->ec_pd_id, vendor, ANALOGIX_VENDOR_ID);
		return -1;
	}

	me->chip.vendor = vendor;
	me->chip.product = product;
	me->chip.device = device;
	me->chip.fw_rev = fw_rev;
	return 0;
}

static void anx3429_clear_device_id(Anx3429 *me)
{
	memset(&me->chip, 0, sizeof(me->chip));
}

/*
 * update the firmware blob and update the next available header to
 * point to it.
 *
 * dir_start - 0 for the live directory, or a scratch pad area
 * fw_data - firmware update, including header(s)
 * fw_words - size of update in words
 */

static int anx3429_update_fw_at(Anx3429 *me,
				const uint32_t dir_start,
				const uint8_t *fw_data,
				const uint16_t fw_words)
{
	uint8_t header[OTP_WORD_SIZE];
	uint32_t act_dir;
	uint16_t act_start;
	uint16_t act_size;
	uint32_t upd_dir;
	uint16_t upd_start;
	int rval;
	uint8_t otp_word[OTP_WORD_SIZE];

	rval = anx3429_find_active_header(me, dir_start, &act_start, &act_size);
	if (rval < 0)
		return -2;
	if (act_start == 0 && act_size == 0) {
		printf("anx3429.%d: header was zero, retry", me->ec_pd_id);
		rval = anx3429_find_active_header(me, dir_start, &act_start,
						  &act_size);
		if (rval < 0)
			return -2;
		else if (act_start == 0 && act_size == 0)
			return -2;
	}
	act_dir = rval;

	printf("anx3429.%d: active FW header at "
	       "offset 0x%04x -> 0x%04x 0x%04x\n",
	       me->ec_pd_id, act_dir, act_start, act_size);

	if (act_dir >= dir_start + OTP_UPDATE_MAX - 1) {
		printf("anx3429.%d: all %u FW header slots already used!\n",
		       me->ec_pd_id, OTP_UPDATE_MAX);
		return -2;
	}

	if (act_start + act_size == 0) {
		/*
		 * this must be the first time we're
		 * programming a FW blob - put it right
		 * after the dir.
		 */
		upd_start = dir_start + OTP_UPDATE_MAX;
	} else {
		/* see if we can reuse current OTP */
		upd_start = act_start;
	}

	rval = anx3429_find_usable_otp(me, upd_start, fw_data, fw_words);
	if (rval < 0)
		return -2;

	upd_start = rval;
	printf("anx3429.%d: writing new FW to offset 0x%04x, size 0x%04x\n",
	       me->ec_pd_id, upd_start, fw_words);

	/* initial FW header */
	memset(header, 0, sizeof(header));
	header[0] = upd_start;
	header[1] = upd_start >> 8;
	header[2] = fw_words;
	header[3] = fw_words >> 8;
	header[8] = anx3429_hamming_encoder(header);

	rval = anx3429_find_usable_otp(me, act_dir, header, 1);
	if (rval < 0)
		return -1;
	upd_dir = rval;
	if (upd_dir >= dir_start +  OTP_UPDATE_MAX) {
		printf("anx3429.%d: no usable FW header slots!\n",
		       me->ec_pd_id);
		return -1;
	}

	printf("anx3429.%d: writing new FW header to offset 0x%04x.\n",
	       me->ec_pd_id, upd_dir);

	/*
	 * Step 1,  repeat 6 times: write firmware data,
	 * new header, and mask old header data.
	 */
	for (int repeat = OTP_PROG_CYCLES; repeat > 0; repeat--) {

		for (int i = 0; i < fw_words; ++i) {
			const uint8_t *w = &fw_data[i * OTP_WORD_SIZE];

			if (anx3429_otp_write72raw(me, upd_start + i, w) != 0) {
				printf("anx3429.%d: program data at 0x%04x on iteration %d with error!\n",
					me->ec_pd_id, upd_start + i, repeat);
				return -1;
			}
		}

		if (anx3429_otp_write72raw(me, upd_dir, header) != 0) {
			printf("anx3429.%d: program header at 0x%04x on iteration %d with error!\n",
				me->ec_pd_id, upd_dir, repeat);
			return -1;
		}

	}


	/* Step2, Verify OTP data*/
	for (int i = 0; i < fw_words; ++i) {
		const uint8_t *w = &fw_data[i * OTP_WORD_SIZE];
		if (anx3429_otp_read72raw(me, upd_start + i, otp_word) != 0)
			return -1;

		if (memcmp(w, otp_word, sizeof(otp_word)) != 0) {
			printf("anx3429.%d: programmed word at 0x%04x with error!\n",
			       me->ec_pd_id, upd_start + i);

			return -1;
		}
	}

	if (anx3429_otp_read72raw(me, upd_dir, otp_word) != 0)
		return -1;

	if (memcmp(header, otp_word, sizeof(otp_word)) != 0) {
		printf("anx3429.%d: OTP header programmed word at 0x%04x with error!\n",
		        me->ec_pd_id, upd_dir);
		return -1;
	}


	/* mask preceding FW headers */
	for (int repeat = OTP_PROG_CYCLES; repeat > 0; repeat--) {
		for (; act_dir < upd_dir; ++act_dir) {
			printf("anx3429.%d: masking FW header at 0x%04x.\n",
			       me->ec_pd_id, act_dir);

			/* Only write 0 bit to 1 bit for the old header,
			 * because OTP data can not be written as "1" too many times.
			 * For example, one byte of the old header: 0x01,
			 * should write 0xFE to mask it.
			 */
			if (anx3429_otp_read72raw(me, act_dir, otp_word) != 0) {
				debug("reading old header failed\n");
				return -1;
			}
			for (int i = 0; i < OTP_WORD_SIZE; i++) {
				otp_word[i] = ~otp_word[i];
			}
			if (anx3429_otp_write72raw(me, act_dir, otp_word) != 0) {
				debug("masking FW header failed\n");
				return -1;
			}
		}
	}



	for (; act_dir < upd_dir; ++act_dir) {
		if (anx3429_otp_read72raw(me, act_dir, otp_word) != 0) {
			debug("reading old header failed\n");
			return -1;
		}
		if (memcmp(inactive_word, otp_word, sizeof(otp_word)) != 0) {
			printf("anx3429.%d: OTP mask header at 0x%04x as invailed with error!\n",
			       me->ec_pd_id, act_dir);
			return -1;
		}
	}

	return 0;
}

static uint8_t test_img[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x11, 0x29, 0x34, 0x00, 0x00, 0x00, 0x00, 0x23,
	0x03, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x14, 0xde, 0xad, 0xbe, 0xef, 0xef, 0xbe, 0xad, 0x00,
	0x15, 0xad, 0xde, 0xad, 0xbe, 0xef, 0xef, 0xbe, 0x00,
	0x16, 0xbe, 0xad, 0xde, 0xad, 0xbe, 0xef, 0xef, 0x00,
	0x17, 0xef, 0xbe, 0xad, 0xde, 0xad, 0xbe, 0xef, 0x00,
};

static int __must_check anx3429_update_scratch(Anx3429 *me,
					       uint8_t *image,
					       const size_t image_size)
{
	const uint16_t image_words = image_size / OTP_WORD_SIZE;
	const uint8_t *blob;
	uint16_t blob_words;

	for (int i = 0; i < image_words; ++i) {
		uint8_t *w = &image[i * OTP_WORD_SIZE];

		w[8] = anx3429_hamming_encoder(w);
	}
	blob = image + ANX_UPD_BLOB_BOFFSET;
	blob_words = image_words - ANX_UPD_BLOB_WOFFSET;
	return anx3429_update_fw_at(me, OTP_SCRATCH_SLOT0, blob, blob_words);
}

static int __must_check anx3429_update_primary(Anx3429 *me,
					       const uint8_t *image,
					       const size_t image_size)

{
	const uint8_t *blob;
	uint16_t blob_words;
	int try_count;

	blob = image + ANX_UPD_BLOB_BOFFSET;
	blob_words = image_size / OTP_WORD_SIZE - ANX_UPD_BLOB_WOFFSET;
	for (try_count = OTP_PROG_FAILED_TRY_CNT; try_count > 0; try_count--) {
		if (anx3429_update_fw_at(me, OTP_DIR_SLOT0, blob, blob_words) == 0)
			return 0;
	}

	return -1;
}

static VbError_t anx3429_check_hash(const VbootAuxFwOps *vbaux,
				    const uint8_t *hash, size_t hash_size,
				    VbAuxFwUpdateSeverity_t *severity)
{
	Anx3429 *me = container_of(vbaux, Anx3429, fw_ops);
	VbError_t status = VBERROR_UNKNOWN;

	debug("call...\n");

	if (hash_size != sizeof(me->chip.fw_rev))
		return VBERROR_INVALID_PARAMETER;

	if (anx3429_capture_device_id(me) == 0)
		status = VBERROR_SUCCESS;

	if (status != VBERROR_SUCCESS)
		return status;

	if (hash[0] == me->chip.fw_rev) {
		if (ANX3429_DEBUG >= 2 && !me->debug_updated) {
			debug("forcing VB_AUX_FW_SLOW_UPDATE\n");
			*severity = VB_AUX_FW_SLOW_UPDATE;
		} else
			*severity = VB_AUX_FW_NO_UPDATE;
	} else
		*severity = VB_AUX_FW_SLOW_UPDATE;

	return VBERROR_SUCCESS;
}

/*
 * is slot likely to be in use
 * - all 0's means it is unused
 * - all 1's means it has been invalidated
 */

static int anx3429_is_slot_active(const uint8_t *buf)
{
	if (is_blank_word(buf))
		return 0;
	if (is_inactive_word(buf))
		return 0;
	return 1;
}

/*
 * dump one-time-programmable memory
 *
 * 1st word is all 0
 * 2nd word is the OTP header
 * next 100 slots are FW headers
 *   - all 0 means it's unused
 *   - all 1 means it's voided
 * first 2 bytes of FW header are word addr of current program
 * next 2 bytes of FW header are word-size of current program
 */

static void anx3429_dump_otp(Anx3429 *me)
{
	const uint32_t offset_max  = OTP_WORDS_MAX;
	uint8_t prev_word[OTP_WORD_SIZE];
	uint8_t word[OTP_WORD_SIZE];
	uint8_t corrected[OTP_WORD_SIZE];
	uint32_t offset;
	int dot3 = 0;
	uint32_t most_recent_blob = 0;
	uint8_t ecc;

	printf("================================"
	       "================================\ncurrent OTP:\n{\n");

	for (offset = 0; offset < offset_max; ++offset) {
		if (anx3429_otp_read72raw(me, offset, word) != 0)
			break;

		if (offset > 0 &&
		    memcmp(word, prev_word, sizeof(word)) == 0) {
			if (!dot3) {
				printf("...\n");
				dot3 = 1;
			}
			continue;
		}
		dot3 = 0;

		memcpy(prev_word, word, sizeof(prev_word));
		if (most_recent_blob != 0 && offset == most_recent_blob)
			printf("NOTE: active firmware:\n");

		printf("0x%04x:", offset);
		print_otp_word(word);
		ecc = anx3429_hamming_encoder(word);
		if (ecc != word[8])
			printf(" [computed %02x]", ecc);
		printf("\n");

		if (anx3429_otp_read72ecc(me, offset, corrected) != 0)
			break;
		if (memcmp(word, corrected, sizeof(word)) != 0) {
			printf("fixed: " );
			print_otp_word(corrected);
			printf("\n");
		}

		if (offset >= OTP_DIR_SLOT0 &&
		    offset < OTP_DIR_SLOT0 + OTP_UPDATE_MAX &&
		    anx3429_is_slot_active(word)) {
			uint16_t loc = le16dec(word);
			uint16_t sz = le16dec(word + 2);

			most_recent_blob = loc;
			printf("\toffset 0x%04x, size %u words\n", loc, sz);
		}
	}
	if (dot3)
		printf("0x%06x:\n", offset);

	printf("}\n================================"
	       "================================\n");
}

/*
 * verify that the proposed firmware update image looks reasonable.
 *
 * the update image layout is very similar to the OTP layout.  the
 * only difference seems to be that the OTP layout has a 100 entry
 * "directory" while the update image seems to only have 1.
 */

static int anx3429_verify_blob(Anx3429 *me,
			       const uint8_t *image,
			       const size_t image_size)
{
	const uint16_t image_words  = image_size / OTP_WORD_SIZE;
	const uint8_t *prev_word;
	const uint8_t *word;
	uint16_t woffset;
	int dot3 = 0;
	int i;
	uint8_t ecc;
	int status = 0;

	debug("proposed OTP update of %u words.\n", image_words);
	dprintf("================================"
		"================================\n{\n");

	for (woffset = 0; woffset < image_words; ++woffset) {
		word = image + woffset * OTP_WORD_SIZE;

		if (woffset > 0 &&
		    memcmp(word, prev_word, OTP_WORD_SIZE) == 0) {
			if (!dot3) {
				dprintf("...\n");
				dot3 = 1;
			}
			continue;
		}
		dot3 = 0;

		prev_word = word;
		if (woffset == ANX_UPD_BLOB_WOFFSET)
			dprintf("NOTE: proposed firmware:\n");

		dprintf("0x%04x:", woffset);
		for (i = 0; i < OTP_WORD_SIZE; ++i)
			dprintf(" %02x", word[i]);
		ecc = anx3429_hamming_encoder(word);
		if (ecc != word[8]) {
			dprintf(" [computed %02x]", ecc);
			status = -1;
		}
		dprintf("\n");

	}
	if (dot3)
		dprintf("0x%04x:\n", woffset);

	if (image_size > ANX_UPD_VERSION_BOFFSET) {
		debug("firmware update internal FW version is 0x%02x\n",
		      image[ANX_UPD_VERSION_BOFFSET]);
	} else {
		printf("anx3429.%d: firmware update "
		       "size %u smaller than expected!\n",
		       me->ec_pd_id, image_size);
		status = -1;
	}

	if (image_size >= ANX_UPD_BLOB_BOFFSET) {
		uint16_t loc = le16dec(image + ANX_UPD_LOC_BOFFSET);
		uint16_t sz = le16dec(image + ANX_UPD_SIZE_BOFFSET);

		debug("update blob offset 0x%04x, size %u words\n", loc, sz);
		if (loc != ANX_UPD_BLOB_WOFFSET) {
			printf("anx3429.%d: "
			       "firmware update blob expected at "
			       "offset 0x%04x instead of 0x%04x\n",
			       me->ec_pd_id, ANX_UPD_BLOB_WOFFSET, loc);
			status = -1;
		}
		if (loc + sz > image_words) {
			printf("anx3429.%d: firmware update blob larger "
			       "than update image (0x%04x > 0x%04x)\n",
			       me->ec_pd_id, loc + sz, image_words);
			status = -1;
		}
	}

	dprintf("}\n================================"
		"================================\n");

	return status;
}

static VbError_t anx3429_update_image(const VbootAuxFwOps *vbaux,
				      const uint8_t *image,
				      const size_t image_size)
{
	Anx3429 *me = container_of(vbaux, Anx3429, fw_ops);
	VbError_t status = VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	int protected;

	debug("call...\n");

	if (anx3429_verify_blob(me, image, image_size) != 0) {
		debug("verify blob failed\n");
		return status;
	}

	if (anx3429_ec_tunnel_status(vbaux, &protected) != 0) {
		debug("tunnel status failed\n");
		return status;
	}

	if (protected) {
		debug("already protected\n");
		return status;
	}

	switch (anx3429_ec_pd_suspend(me)) {
	case -EC_RES_BUSY:
		return VBERROR_PERIPHERAL_BUSY;
	case EC_RES_SUCCESS:
		/* Continue onward */
		break;
	default:
		debug("pd suspend failed\n");
		return status;
	}

	if (anx3429_ec_pd_powerup(me) != 0) {
		debug("pd powerup failed\n");
		return status;
	}
	if (anx3429_disable_mcu(me) != 0) {
		debug("pd disable failed\n");
		return status;
	}

	if (ANX3429_DEBUG >= 2)
		anx3429_dump_otp(me);

	if (ANX3429_SCRATCH > 0) {
		debug("update scratch area, size %u\n", sizeof(test_img));
		status = anx3429_update_scratch(me, test_img, sizeof(test_img));
	} else
		status = anx3429_update_primary(me, image, image_size);

	switch (status) {
	case -2:
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	case -1:
		/* This will trigger recovery mode */
		return VBERROR_UNKNOWN;
	}

	if (ANX3429_DEBUG >= 2) {
		if (status == 0)
			me->debug_updated = 1;
		anx3429_dump_otp(me);
	}

	if (anx3429_enable_mcu(me) != 0) {
		debug("enable mcu failed\n");
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}

	if (anx3429_ec_pd_resume(me) != 0) {
		debug("pd resume failed\n");
		status = VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}

	/* force re-read */
	anx3429_clear_device_id(me);
	/*
	 * wait for the TCPC and pd_task() to restart
	 *
	 * TODO(b/64696543): remove delay when the EC can delay TCPC
	 * host commands until it can service them.
	 */
	mdelay(ANX_RESTART_MS);

	/* Request EC reboot on successful update */
	if (status == VBERROR_SUCCESS)
		status = VBERROR_EC_REBOOT_TO_RO_REQUIRED;

	return status;
}

static VbError_t anx3429_protect(const VbootAuxFwOps *vbaux)
{
	Anx3429 *me = container_of(vbaux, Anx3429, fw_ops);

	debug("call...\n");

	if (cros_ec_tunnel_i2c_protect(me->bus) != 0) {
		printf("anx3429.%d: could not protect EC I2C tunnel\n",
		       me->ec_pd_id);
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

static const VbootAuxFwOps anx3429_fw_ops = {
	.fw_image_name = "anx3429_ocm.bin",
	.fw_hash_name = "anx3429_ocm.hash",
	.check_hash = anx3429_check_hash,
	.update_image = anx3429_update_image,
	.protect_status = anx3429_ec_tunnel_status,
	.protect = anx3429_protect,
};

Anx3429 *new_anx3429(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Anx3429 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = anx3429_fw_ops;

	return me;
}
