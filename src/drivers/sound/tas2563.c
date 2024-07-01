// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2024 Texas Instruments
 */

#include <libpayload.h>
#include <assert.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/tas2563.h"

/* Book Control Register (available in page0 of each book) */
/* TAS256X 32bit: 0(9bit) + BOOK(8bit) + PAGE(8bit) + reg(7bit) */
#define TAS256X_REG(book, page, reg)	(((book) & 0xFF) << 15 | \
					((page) & 0xFF) << 7 | ((reg) & 0x7F))
#define TAS256X_BOOK_ID(reg)		(((reg) >> 15) & 0xFF)
#define TAS256X_PAGE_ID(reg)		(((reg) >> 7) & 0xFF)
#define TAS256X_PAGE_REG(reg)		((reg) & 0x7F)
#define TAS256X_BOOKCTL_PAGE		0x0
#define TAS256X_BOOKCTL_REG		0x7F

/* Software Reset */
#define TAS2563_REG_SWRESET		TAS256X_REG(0x0, 0x0, 0x01)
#define TAS2563_REG_SWRESET_RESET	1

#define TAS2563_REG_RX_CHAN		TAS256X_REG(0x0, 0x0, 0x08)
#define TAS2563_REG_RX_CHAN_LFT_16BIT	0x10
#define TAS2563_REG_RX_CHAN_RT_16BIT	0x20
#define TAS2563_REG_RX_CHAN_LFT_32BIT	0x1E
#define TAS2563_REG_RX_CHAN_RT_32BIT	0x2E

#define TAS2563_REG_MISC_DSP			TAS256X_REG(0x0, 0x1, 0x02)
#define TAS2563_REG_MISC_DSP_ROM_MODE_MASK	(0xf << 1)
#define TAS2563_REG_MISC_DSP_ROM_MODE_BYPASS_MODE	(0x1 << 1)

struct cfg_reg {
	uint8_t offset;
	uint8_t value;
};

static const struct cfg_reg bypass_power_up[] = {
	/* BOOK0 PAGE0 */
	{ TAS256X_BOOKCTL_PAGE, 0x00 },
	{ TAS256X_BOOKCTL_REG, 0x00 },
	{ 0x0a, 0x13 },
	{ 0x0b, 0x02 }, /* disable Vsns */
	{ 0x0c, 0x00 }, /* disable Isns, Isns: slot 2 */
	{ 0x30, 0x99 },
	{ 0x03, 0x02 }, /* change AMP_LEVEL to lowest */
	{ 0x02, 0x0c }
};

static int i2c_read(Tas2563Codec *codec, uint8_t reg, uint8_t *data)
{
	return i2c_readb(codec->i2c, codec->chip, reg, data);
}

static int i2c_write(Tas2563Codec *codec, uint8_t reg, uint8_t data)
{
	return i2c_writeb(codec->i2c, codec->chip, reg, data);
}

static int tasdev_change_book_page(Tas2563Codec *codec, uint8_t book, uint8_t page)
{
	if (codec->cur_book != book) {
		if (i2c_write(codec, TAS256X_BOOKCTL_PAGE, 0))
			return -1;
		codec->cur_page = 0;
		if (i2c_write(codec, TAS256X_BOOKCTL_REG, book))
			return -1;
		codec->cur_book = book;
	}

	if (codec->cur_page != page) {
		if (i2c_write(codec, TAS256X_BOOKCTL_PAGE, page))
			return -1;
		codec->cur_page = page;
	}

	return 0;
}

static int tasdev_update_bits(Tas2563Codec *codec, uint32_t reg,
			      uint8_t mask, uint8_t value)
{
	uint8_t tmp, old;
	int ret;

	ret = tasdev_change_book_page(codec, TAS256X_BOOK_ID(reg),
				      TAS256X_PAGE_ID(reg));
	if (ret) {
		printf("%s: Failed to set book and page\n", __func__);
		return ret;
	}

	ret = i2c_read(codec, TAS256X_PAGE_REG(reg), &old);
	if (ret) {
		printf("%s: i2c_read error\n", __func__);
		return ret;
	}
	tmp = old & ~mask;
	tmp |= value & mask;
	if (tmp != old) {
		ret = i2c_write(codec, TAS256X_PAGE_REG(reg), tmp);
		if (ret) {
			printf("%s: i2c_write error\n", __func__);
			return ret;
		}
	}

	return 0;
}

static int tasdev_write(Tas2563Codec *codec, uint32_t reg, uint8_t value)
{
	int ret;

	ret = tasdev_change_book_page(codec, TAS256X_BOOK_ID(reg),
				      TAS256X_PAGE_ID(reg));
	if (ret) {
		printf("%s: Failed to set book and page\n", __func__);
		return ret;
	}

	ret = i2c_write(codec, TAS256X_PAGE_REG(reg), value);
	if (ret)
		printf("%s: i2c_write error\n", __func__);

	return 0;
}

static int tasdev_transmit_registers(Tas2563Codec *codec, const struct cfg_reg *regs,
				     size_t size)
{
	int i = 0;
	uint8_t new_book = codec->cur_book;
	uint8_t new_page = codec->cur_page;

	for (i = 0; i < size; i++) {
		assert(regs[i].offset <= TAS256X_BOOKCTL_REG);
		if (regs[i].offset == TAS256X_BOOKCTL_PAGE) {
			if (new_page == regs[i].value)
				continue;
			else
				new_page = regs[i].value;
		} else if (codec->cur_page == 0 && regs[i].offset == TAS256X_BOOKCTL_REG) {
			if (new_book == regs[i].value)
				continue;
			else
				new_book = regs[i].value;
		}

		if (i2c_write(codec, regs[i].offset, regs[i].value)) {
			printf("%s: Failed to set value, chip = %d offset = 0x%02x\n",
				__func__, codec->chip, regs[i].offset);
			return -1;
		}

		codec->cur_book = new_book;
		codec->cur_page = new_page;
	}

	return 0;
}

static int tas2563_enable(SoundRouteComponentOps *me)
{
	Tas2563Codec *codec = container_of(me, Tas2563Codec, component.ops);
	uint8_t pcm_fmt = TAS2563_REG_RX_CHAN_LFT_16BIT;
	int ret;

	ret = tasdev_write(codec, TAS2563_REG_SWRESET, TAS2563_REG_SWRESET_RESET);
	if (ret) {
		printf("%s: Failed to set RESET bits\n", __func__);
		return ret;
	}

	mdelay(1);

	ret = tasdev_update_bits(codec, TAS2563_REG_MISC_DSP,
				 TAS2563_REG_MISC_DSP_ROM_MODE_MASK,
				 TAS2563_REG_MISC_DSP_ROM_MODE_BYPASS_MODE);
	if (ret) {
		printf("%s: Failed to update ROM MODE bits\n", __func__);
		return ret;
	}

	if (codec->channel == PCM_LEFT_ONLY &&
	    codec->bitwidth == PCM_FMTBIT_S16_LE) {
		pcm_fmt = TAS2563_REG_RX_CHAN_LFT_16BIT;
	} else if (codec->channel == PCM_RIGHT_ONLY &&
		   codec->bitwidth == PCM_FMTBIT_S16_LE) {
		pcm_fmt = TAS2563_REG_RX_CHAN_RT_16BIT;
	} else if (codec->channel == PCM_LEFT_ONLY &&
		   (codec->bitwidth == PCM_FMTBIT_S24_LE ||
		    codec->bitwidth == PCM_FMTBIT_S32_LE)) {
		pcm_fmt = TAS2563_REG_RX_CHAN_LFT_32BIT;
	} else if (codec->channel == PCM_RIGHT_ONLY &&
		   (codec->bitwidth == PCM_FMTBIT_S24_LE ||
		    codec->bitwidth == PCM_FMTBIT_S32_LE)) {
		pcm_fmt = TAS2563_REG_RX_CHAN_RT_32BIT;
	} else {
		printf("%s: Unexpected codec channel %#x and bitwidth %#x\n",
			__func__,codec->channel, codec->bitwidth);
		return -1;
	}

	ret = tasdev_write(codec, TAS2563_REG_RX_CHAN, pcm_fmt);
	if (ret) {
		printf("%s: Failed to Set RESET bits\n", __func__);
		return ret;
	}

	ret = tasdev_transmit_registers(codec, bypass_power_up, ARRAY_SIZE(bypass_power_up));
	if (ret) {
		printf("%s: Failed to transmit register value\n", __func__);
		return ret;
	}

	return 0;
}

Tas2563Codec *new_tas2563_codec(I2cOps *i2c, uint8_t chip,
				enum pcm_channel channel,
				enum pcm_bitwidth bitwidth)
{
	Tas2563Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &tas2563_enable;
	codec->i2c = i2c;
	codec->chip = chip;
	codec->channel = channel;
	codec->bitwidth = bitwidth;

	/* After power up, TAS2563's default book and page are 0. */
	codec->cur_book = 0;
	codec->cur_page = 0;

	return codec;
}
