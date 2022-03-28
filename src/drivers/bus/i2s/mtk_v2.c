// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2022 Mediatek Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2s/mtk_v2.h"

#ifdef MTK_I2S_DEBUG
#define I2S_LOG(args...)	printf(args)
#else
#define I2S_LOG(args...)
#endif

#define BUFFER_PADDING_LENGTH 2400	/* 50ms@48KHz stereo */

struct mtk_afe_rate {
	unsigned int rate;
	unsigned int reg_value;
};

static const struct mtk_afe_rate mtk_afe_rates[] = {
	{ .rate = 8000, .reg_value = 0, },
	{ .rate = 12000, .reg_value = 1, },
	{ .rate = 16000, .reg_value = 2, },
	{ .rate = 24000, .reg_value = 3, },
	{ .rate = 32000, .reg_value = 4, },
	{ .rate = 48000, .reg_value = 5, },
	{ .rate = 96000, .reg_value = 6, },
	{ .rate = 192000, .reg_value = 7, },
	{ .rate = 384000, .reg_value = 8, },
	{ .rate = 7350, .reg_value = 16, },
	{ .rate = 11025, .reg_value = 17, },
	{ .rate = 14700, .reg_value = 18, },
	{ .rate = 22050, .reg_value = 19, },
	{ .rate = 29400, .reg_value = 20, },
	{ .rate = 44100, .reg_value = 21, },
	{ .rate = 88200, .reg_value = 22, },
	{ .rate = 176400, .reg_value = 23, },
	{ .rate = 352800, .reg_value = 24, },
};

static int get_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_afe_rates); i++)
		if (mtk_afe_rates[i].rate == rate)
			return mtk_afe_rates[i].reg_value;

	return -1;
}

static const struct mtk_afe_rate mtk_etdm_rates[] = {
	{ .rate = 8000, .reg_value = 0, },
	{ .rate = 12000, .reg_value = 1, },
	{ .rate = 16000, .reg_value = 2, },
	{ .rate = 24000, .reg_value = 3, },
	{ .rate = 32000, .reg_value = 4, },
	{ .rate = 48000, .reg_value = 5, },
	{ .rate = 96000, .reg_value = 7, },
	{ .rate = 192000, .reg_value = 9, },
	{ .rate = 384000, .reg_value = 11, },
	{ .rate = 11025, .reg_value = 16, },
	{ .rate = 22050, .reg_value = 17, },
	{ .rate = 44100, .reg_value = 18, },
	{ .rate = 88200, .reg_value = 19, },
	{ .rate = 176400, .reg_value = 20, },
	{ .rate = 352800, .reg_value = 21, },
};

static int get_etdm_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_etdm_rates); i++)
		if (mtk_etdm_rates[i].rate == rate)
			return mtk_etdm_rates[i].reg_value;

	return -1;
}

static int mtk_i2s_init(MtkI2s *bus)
{
	MtkI2sRegs *regs = bus->regs;
	uint32_t mask;
	uint32_t val;
	uint32_t etdm_fs = get_etdm_fs_timing(bus->rate);
	uint32_t afe_fs = get_afe_fs_timing(bus->rate);

	if (bus->initialized)
		return 0;

	bus->initialized = 1;

	/* CLK_AFE */
	clrbits_le32(&regs->top_con0, BIT(2));

	/* CLK_AUD_A1SYS_HP */
	clrbits_le32(&regs->top_con1, BIT(2));

	/* CLK_AUD_I2S_IN, CLK_AUD_TDM_IN, CLK_AUD_I2S_OUT, CLK_AUD_TDM_OUT,
	CLK_AUD_A1SYS, CLK_AUD_A2SYS */
	clrbits_le32(&regs->top_con4,
		     BIT(0) | BIT(1) | BIT(6) | BIT(7) | BIT(21) | BIT(22));

	/* memif agent pd disable */
	clrbits_le32(&regs->top_con5, BIT(18));

	/* 26m, a1sys, a2sys */
	setbits_le32(&regs->asys_top_con, BIT(0) | BIT(1) | BIT(2));

	/* lp_mode : normal */
	clrbits_le32(&regs->asys_top_con, BIT(3));

	/* enable AFE */
	setbits_le32(&regs->dac_con0, BIT(0));

	assert(bus->channels == 1 || bus->channels == 2);
	clrsetbits_le32(&MTK_MEMIF_CHANNEL(regs),
			0x1f << MTK_MEMIF_CHANNEL_SFT,
			bus->channels << MTK_MEMIF_CHANNEL_SFT);

	/* set rate */
	clrsetbits_le32(&MTK_MEMIF_RATE(regs),
			0x1f << MTK_MEMIF_RATE_SFT,
			afe_fs << MTK_MEMIF_RATE_SFT);

	/* set bitwidth */
	clrsetbits_le32(&MTK_MEMIF_BITWIDTH(regs),
			0x1 << MTK_MEMIF_BIT_SFT,
			0 << MTK_MEMIF_BIT_SFT);

	switch (bus->i2s_num) {
	case AFE_I2S_I1O2:
		/* I70/I71 -> O48/O49 connection */
		setbits_le32(&regs->conn48_2, BIT(6));
		setbits_le32(&regs->conn49_2, BIT(7));

		clrsetbits_le32(&regs->etdm_in1_con2, ETDM_IN_CON2_CLOCK_MASK,
				ETDM_IN_CON2_CLOCK(1));

		/* config I2SI1 */
		mask = ETDM_CON0_BIT_LEN_MASK | ETDM_CON0_WORD_LEN_MASK |
		       ETDM_CON0_FORMAT_MASK | ETDM_CON0_CH_NUM_MASK |
		       ETDM_CON0_SLAVE_MODE;
		val = ETDM_CON0_BIT_LEN(bus->bit_len) |
		      ETDM_CON0_WORD_LEN(bus->word_len) |
		      ETDM_CON0_FORMAT(0) |
		      ETDM_CON0_CH_NUM(bus->channels);
		clrsetbits_le32(&regs->etdm_in1_con0, mask, val);


		mask = ETDM_IN_AFIFO_MODE_MASK | ETDM_IN_USE_AFIFO;
		val = ETDM_IN_AFIFO_MODE(afe_fs) | ETDM_IN_USE_AFIFO;
		clrsetbits_le32(&regs->in1_afifo_con, mask, val);

		mask = ETDM_IN_CON3_FS_MASK;
		val = ETDM_IN_CON3_FS(etdm_fs);
		clrsetbits_le32(&regs->etdm_in1_con3, mask, val);

		/* config I2SO2 */
		mask = ETDM_CON0_BIT_LEN_MASK | ETDM_CON0_WORD_LEN_MASK |
		       ETDM_CON0_FORMAT_MASK | ETDM_CON0_CH_NUM_MASK |
		       ETDM_CON0_SLAVE_MODE | ETDM_OUT_CON0_RELATCH_DOMAIN_MASK;

		val = ETDM_CON0_BIT_LEN(bus->bit_len) |
		      ETDM_CON0_WORD_LEN(bus->word_len) |
		      ETDM_CON0_FORMAT(0x0) |
		      ETDM_CON0_CH_NUM(bus->channels) |
		      ETDM_CON0_SLAVE_MODE |
		      ETDM_OUT_CON0_RELATCH_DOMAIN(0x0);
		clrsetbits_le32(&regs->etdm_out2_con0, mask, val);

		mask = ETDM_OUT_CON2_LRCK_DELAY_BCK_INV |
		       ETDM_OUT_CON2_LRCK_DELAY_0P5T_EN;
		val = ETDM_OUT_CON2_LRCK_DELAY_BCK_INV |
		      ETDM_OUT_CON2_LRCK_DELAY_0P5T_EN;
		clrsetbits_le32(&regs->etdm_out2_con2, mask, val);

		mask = ETDM_OUT_CON4_RELATCH_EN_MASK;
		val = ETDM_OUT_CON4_RELATCH_EN(0xa);
		clrsetbits_le32(&regs->etdm_out2_con4, mask, val);

		mask = ETDM_OUT2_SLAVE_SEL_MASK;
		val = ETDM_OUT2_SLAVE_SEL(0x2);
		clrsetbits_le32(&regs->etdm_cowork_con2, mask, val);

		/* enable I2SI1 */
		setbits_le32(&regs->etdm_in1_con0, ETDM_CON0_EN);

		/* enable I2SO2 */
		setbits_le32(&regs->etdm_out2_con0, ETDM_CON0_EN);

		break;
	default:
		printf("%s: Unrecognignized i2s type %d\n",
			__func__, bus->i2s_num);
		return -1;
	}

	return 0;
}

static int mtk_i2s_enable(SoundRouteComponentOps *me)
{
	MtkI2s *bus = container_of(me, MtkI2s, component.ops);

	return mtk_i2s_init(bus);
}

static size_t copy_to_buf(uint8_t *buf, size_t size,
			  const uint8_t **data, size_t *data_size)
{
	size_t update_size = MIN(size, *data_size);

	memcpy(buf, *data, update_size);
	*data_size -= update_size;
	*data += update_size;

	if (size > update_size)
		memset((void *)(buf + update_size), 0, size - update_size);

	return size - update_size;
}

static int mtk_i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	MtkI2s *bus = container_of(me, MtkI2s, ops);
	MtkI2sRegs *regs = bus->regs;
	uintptr_t buf_base, buf_end, buf_mid, data_cur, data_base, data_end;
	size_t data_size, pad_remain, pad_size;
	const uint8_t *cur = (uint8_t *)data;

	if (mtk_i2s_init(bus))
		return -1;

	data_cur = data_base = 0;
	data_size = length * sizeof(*data);
	/* 0s are padded at the end of data in case the DMA rings to the
	   beginning of the buffer before disabling DMA. */
	pad_remain = BUFFER_PADDING_LENGTH * sizeof(*data);
	buf_base =  (uintptr_t)SRAM_BASE;
	buf_end = buf_base + SRAM_SIZE - 1;
	buf_mid = buf_base + SRAM_SIZE / 2 - 1;
	data_end = buf_base + data_size - 1;

	I2S_LOG("len = %d base = %#lx end = %#lx data = %#lx\n",
		length, buf_base, buf_end, data_end);

	pad_size = copy_to_buf((uint8_t *)buf_base, SRAM_SIZE, &cur,
			       &data_size);
	pad_remain -= MIN(pad_remain, pad_size);

	write32(&regs->dl2_base, buf_base);
	write32(&regs->dl2_end, buf_end);

	/* enable DL2 */
	setbits_le32(&regs->dac_con0, BIT(MTK_MEMIF_ENABLE_SFT));

	/*
	 * To improve overall bandwidth, divide the buffer into 2 halves and use
	 * them as ping-pong buffers. While one half is being consumed by DMA,
	 * refill the other half.
	 */
	while(1) {
		/* Wait for the 1st half of buffer to be consumed. */
		while ((data_cur = read32(&regs->dl2_cur)) <= buf_mid)
			if (data_base + data_cur > data_end)
				break;

		if (data_base + data_cur > data_end)
			break;

		if (pad_remain) {
			pad_size = copy_to_buf((uint8_t *)buf_base,
					       SRAM_SIZE / 2,
					       &cur, &data_size);
			pad_remain -= MIN(pad_remain, pad_size);
		}

		/* Wait for the 2nd half of buffer to be consumed. */
		while ((data_cur = read32(&regs->dl2_cur)) >= buf_mid)
			if (data_base + data_cur > data_end)
				break;

		data_base += SRAM_SIZE;
		if (data_base + data_cur > data_end)
			break;

		if (pad_remain) {
			pad_size = copy_to_buf((uint8_t *)(buf_base +
					       SRAM_SIZE / 2),
					       SRAM_SIZE / 2,
					       &cur, &data_size);
			pad_remain -= MIN(pad_remain, pad_size);
		}
	}

	clrbits_le32(&regs->dac_con0, BIT(MTK_MEMIF_ENABLE_SFT));

	return 0;
}

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate,
		    uint32_t word_len, uint32_t bit_len, uint32_t i2s_num)
{
	MtkI2s *bus = xzalloc(sizeof(*bus));

	bus->component.ops.enable = &mtk_i2s_enable;
	bus->ops.send = &mtk_i2s_send;
	bus->regs = (void *)base;
	bus->channels = channels;
	bus->rate = rate;
	bus->i2s_num = i2s_num;
	bus->word_len = word_len;
	bus->bit_len = bit_len;

	return bus;
}
