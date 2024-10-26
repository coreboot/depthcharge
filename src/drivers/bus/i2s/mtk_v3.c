// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <libpayload.h>

#include "drivers/bus/i2s/mtk_v3.h"

#ifdef MTK_I2S_DEBUG
#define I2S_LOG(args...)	printf(args)
#else
#define I2S_LOG(args...)
#endif

#define BUFFER_PADDING_LENGTH 2400	/* 50ms@48KHz stereo */

struct mtk_afe_rate {
	unsigned int rate;
	unsigned int reg_v_afe;
	unsigned int reg_v_etdm;
	unsigned int reg_v_etdm_relatch;
};

static const struct mtk_afe_rate mtk_rates[] = {
	{ .rate = 8000, .reg_v_afe = 0, .reg_v_etdm = 0, .reg_v_etdm_relatch = 0 },
	{ .rate = 12000, .reg_v_afe = 2, .reg_v_etdm = 1, .reg_v_etdm_relatch = 2 },
	{ .rate = 16000, .reg_v_afe = 4, .reg_v_etdm = 2, .reg_v_etdm_relatch = 4 },
	{ .rate = 24000, .reg_v_afe = 6, .reg_v_etdm = 3, .reg_v_etdm_relatch = 6 },
	{ .rate = 32000, .reg_v_afe = 8, .reg_v_etdm = 4, .reg_v_etdm_relatch = 8 },
	{ .rate = 48000, .reg_v_afe = 10, .reg_v_etdm = 5, .reg_v_etdm_relatch = 10 },
	{ .rate = 96000, .reg_v_afe = 14, .reg_v_etdm = 7, .reg_v_etdm_relatch = 14 },
	{ .rate = 192000, .reg_v_afe = 18, .reg_v_etdm = 9, .reg_v_etdm_relatch = 18 },
	{ .rate = 384000, .reg_v_afe = 22, .reg_v_etdm = 11, .reg_v_etdm_relatch = 22 },
	{ .rate = 11025, .reg_v_afe = 1, .reg_v_etdm = 16, .reg_v_etdm_relatch = 1 },
	{ .rate = 22050, .reg_v_afe = 5, .reg_v_etdm = 17, .reg_v_etdm_relatch = 5 },
	{ .rate = 44100, .reg_v_afe = 9, .reg_v_etdm = 18, .reg_v_etdm_relatch = 9 },
	{ .rate = 88200, .reg_v_afe = 13, .reg_v_etdm = 19, .reg_v_etdm_relatch = 13 },
	{ .rate = 176400, .reg_v_afe = 17, .reg_v_etdm = 20, .reg_v_etdm_relatch = 17 },
	{ .rate = 352800, .reg_v_afe = 21, .reg_v_etdm = 21, .reg_v_etdm_relatch = 21 },
};

const struct mtk_afe_rate *get_fs_timing(unsigned int rate)
{
	for (size_t i = 0; i < ARRAY_SIZE(mtk_rates); i++) {
		if (mtk_rates[i].rate == rate) {
			return &mtk_rates[i];
		}
	}
	return NULL;
}

static void mtk_afe_reg_dump(MtkI2sRegs *regs)
{
	I2S_LOG("REG Addr: xxx Value: xxx\n");
	I2S_LOG("audio_top_con0: %p, value: 0x%x\n",
		&regs->audio_top_con0, read32(&regs->audio_top_con0));
	I2S_LOG("audio_top_con1: %p, value: 0x%x\n",
		&regs->audio_top_con1, read32(&regs->audio_top_con1));
	I2S_LOG("audio_top_con2: %p, value: 0x%x\n",
		&regs->audio_top_con2, read32(&regs->audio_top_con2));
	I2S_LOG("audio_top_con3: %p, value: 0x%x\n",
		&regs->audio_top_con3, read32(&regs->audio_top_con3));
	I2S_LOG("audio_top_con4: %p, value: 0x%x\n",
		&regs->audio_top_con4, read32(&regs->audio_top_con4));
	I2S_LOG("audio_engen_con0: %p, value: 0x%x\n",
		&regs->audio_engen_con0, read32(&regs->audio_engen_con0));
	I2S_LOG("afe_apll1_tuner_cfg: %p, value: 0x%x\n",
		&regs->afe_apll1_tuner_cfg, read32(&regs->afe_apll1_tuner_cfg));
	I2S_LOG("afe_apll2_tuner_cfg: %p, value: 0x%x\n",
		&regs->afe_apll2_tuner_cfg, read32(&regs->afe_apll2_tuner_cfg));
	I2S_LOG("afe_spm_control_req: %p, value: 0x%x\n",
		&regs->afe_spm_control_req, read32(&regs->afe_spm_control_req));
	I2S_LOG("aud_top_cfg_vlp_rg: %p, value: 0x%x\n",
		&regs->aud_top_cfg_vlp_rg, read32(&regs->aud_top_cfg_vlp_rg));
	I2S_LOG("etdm_4_7_cowork_con0: %p, value: 0x%x\n",
		&regs->etdm_4_7_cowork_con0, read32(&regs->etdm_4_7_cowork_con0));
	I2S_LOG("etdm_in[0].con[0]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[0], read32(&regs->etdm_in[0].con[0]));
	I2S_LOG("etdm_in[0].con[1]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[1], read32(&regs->etdm_in[0].con[4]));
	I2S_LOG("etdm_in[0].con[2]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[2], read32(&regs->etdm_in[0].con[2]));
	I2S_LOG("etdm_in[0].con[3]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[3], read32(&regs->etdm_in[0].con[3]));
	I2S_LOG("etdm_in[0].con[4]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[4], read32(&regs->etdm_in[0].con[4]));
	I2S_LOG("etdm_in[0].con[9]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[9], read32(&regs->etdm_in[0].con[9]));
	I2S_LOG("etdm_out[0].con[0]: %p, value: 0x%x\n",
		&regs->etdm_out[0].con[0], read32(&regs->etdm_out[0].con[0]));
	I2S_LOG("etdm_out[0].con[4]: %p, value: 0x%x\n",
		&regs->etdm_out[0].con[4], read32(&regs->etdm_out[0].con[4]));
	I2S_LOG("afe_conn116_1: %p, value: 0x%x\n",
		&regs->afe_conn116_1, read32(&regs->afe_conn116_1));
	I2S_LOG("afe_conn117_1: %p, value: 0x%x\n",
		&regs->afe_conn117_1, read32(&regs->afe_conn117_1));
	I2S_LOG("afe_dl_24ch_con0: %p, value: 0x%x\n",
		&regs->afe_dl_24ch_con0, read32(&regs->afe_dl_24ch_con0));
}

static void mtk_etdmout_enable(MtkI2sRegs *regs, bool enable)
{
	uint32_t mask;
	uint32_t val;

	if (enable)
		val = BIT(0);
	else
		val = 0;

	I2S_LOG("enable: %d, val: %d\n",enable, val);

	/* enable etdm in4 */
	mask = REG_ETDM_IN_EN_MASK_SFT;
	clrsetbits_le32(&regs->etdm_in[0].con[0], mask, val << REG_ETDM_IN_EN_SFT);

	/* enable etdm out4 */
	mask = OUT_REG_ETDM_OUT_EN_MASK_SFT;
	clrsetbits_le32(&regs->etdm_out[0].con[0], mask, val << OUT_REG_ETDM_OUT_EN_SFT);

	I2S_LOG("etdm_in[0].con[0]: %p, value: 0x%x\n",
		&regs->etdm_in[0].con[0], read32(&regs->etdm_in[0].con[0]));
	I2S_LOG("etdm_out[0].con[0]: %p, value: 0x%x\n",
		&regs->etdm_out[0].con[0], read32(&regs->etdm_out[0].con[0]));
}

static int mtk_i2s_init(MtkI2s *bus)
{
	MtkI2sRegs *regs = bus->regs;
	uint32_t mask;
	uint32_t val;
	const struct mtk_afe_rate *entry;
	uint32_t afe_fs;
	uint32_t etdm_fs;
	uint32_t etdm_relatch_fs;

	I2S_LOG("%s initialized: %d\n", __func__, bus->initialized);

	if (bus->initialized)
		return 0;

	entry = get_fs_timing(bus->rate);
	if (!entry) {
		I2S_LOG("unsupported bus->rate: %d\n", bus->rate);
		return -1;
	}

	afe_fs = entry->reg_v_afe;
	etdm_fs = entry->reg_v_etdm;
	etdm_relatch_fs = entry->reg_v_etdm_relatch;

	I2S_LOG("bus->rate: %d, etdm_fs: %d, etdm_relatch_fs: %d, afe_fs: %d\n",
		bus->rate, etdm_fs, etdm_relatch_fs, afe_fs);

	bus->initialized = 1;

	/* disabled afe all pdn */
	write32(&regs->audio_top_con0, 0x00);
	write32(&regs->audio_top_con1, 0x00);
	write32(&regs->audio_top_con2, 0x00);
	write32(&regs->audio_top_con3, 0x00);
	write32(&regs->audio_top_con4, 0x00);

	/* enable 26M apll1 */
	setbits_le32(&regs->audio_engen_con0, BIT(0) | BIT(3));

	/* set 0x00 */
	write32(&regs->afe_apll1_tuner_cfg, 0x00);

	/* set 0x375 */
	write32(&regs->afe_apll2_tuner_cfg, 0x375);

	/* enable afe scrclkena req */
	setbits_le32(&regs->afe_spm_control_req, BIT(0));

	/* enable i2s4 pad top ck */
	setbits_le32(&regs->aud_top_cfg_vlp_rg, BIT(5));

	/* ETDM_4_7_COWORK_CON0 */
	/* set etdm out4 slave sel from etdmin4 master
	 * set etdm in4 slave sel from etdm out4 master
	 */
	mask = ETDM_OUT4_SLAVE_SEL_MASK_SFT | ETDM_IN4_SLAVE_SEL_MASK_SFT;
	val = (ETDM_OUT4_SLAVE_SEL_VALUE << ETDM_OUT4_SLAVE_SEL_SFT) |
	      (ETDM_IN4_SLAVE_SEL_VALUE << ETDM_IN4_SLAVE_SEL_SFT);
	clrsetbits_le32(&regs->etdm_4_7_cowork_con0, mask, val);

	/* ETDM IN4 */
	/* set etdm in4 bit_length and word_length 16bit */
	mask = REG_BIT_LENGTH_MASK_SFT | REG_WORD_LENGTH_MASK_SFT;
	val = (REG_BIT_LENGTH_VALUE << REG_BIT_LENGTH_SFT) |
	      (REG_WORD_LENGTH_VALUE << REG_WORD_LENGTH_SFT);
	clrsetbits_le32(&regs->etdm_in[0].con[0], mask, val);

	/* set reg initial point */
	mask = REG_INITIAL_POINT_MASK_SFT;
	val = REG_INITIAL_POINT_VAL << REG_INITIAL_POINT_SFT;
	clrsetbits_le32(&regs->etdm_in[0].con[1], mask, val);

	/* set reg clock source sel */
	mask = REG_CLOCK_SOURCE_SEL_MASK_SFT;
	val = REG_CLOCK_SOURCE_SEL_VALUE << REG_CLOCK_SOURCE_SEL_SFT;
	clrsetbits_le32(&regs->etdm_in[0].con[2], mask, val);

	/* set reg fs timing sel */
	mask = REG_FS_TIMING_SEL_MASK_SFT;
	val = etdm_fs << REG_FS_TIMING_SEL_SFT;
	clrsetbits_le32(&regs->etdm_in[0].con[3], mask, val);

	/* set reg relatch 1x en sel */
	mask = REG_RELATCH_1X_EN_SEL_MASK_SFT;
	val = etdm_relatch_fs << REG_RELATCH_1X_EN_SEL_SFT;
	clrsetbits_le32(&regs->etdm_in[0].con[4], mask, val);

	/* set reg relatch 1x en sel */
	mask = REG_OUT2LATCH_TIME_MASK_SFT;
	val = REG_OUT2LATCH_TIME_VALUE << REG_OUT2LATCH_TIME_SFT;
	clrsetbits_le32(&regs->etdm_in[0].con[9], mask, val);

	/* ETDM OUT4 */
	/* set etdm out4 relatch */
	mask = OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT;
	val = OUT_REG_RELATCH_DOMAIN_SEL_VALUE << OUT_REG_RELATCH_DOMAIN_SEL_SFT;
	clrsetbits_le32(&regs->etdm_out[0].con[0], mask, val);

	/* set etdm out4 bit_length and word_length 16bit */
	mask = OUT_REG_BIT_LENGTH_MASK_SFT | OUT_REG_WORD_LENGTH_MASK_SFT;
	val = (OUT_REG_BIT_LENGTH_VALUE << OUT_REG_BIT_LENGTH_SFT) |
	      (OUT_REG_WORD_LENGTH_VAL << OUT_REG_WORD_LENGTH_SFT);
	clrsetbits_le32(&regs->etdm_out[0].con[0], mask, val);

	/* set etdm out4 timing, clock source and relatch en */
	mask = OUT_REG_FS_TIMING_SEL_MASK_SFT | OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT |
	       OUT_REG_RELATCH_EN_SEL_MASK_SFT;
	val = (etdm_fs << OUT_REG_FS_TIMING_SEL_SFT) |
	      (OUT_REG_CLOCK_SOURCE_SEL_VALUE << OUT_REG_CLOCK_SOURCE_SEL_SFT) |
	      (etdm_relatch_fs << OUT_REG_RELATCH_EN_SEL_SFT);
	clrsetbits_le32(&regs->etdm_out[0].con[4], mask, val);

	/* AFE_CONN116_1 */
	/* I54 -> O116 */
	mask = I054_O116_S_MASK_SFT;
	val = I054_O116_S_VALUE << I054_O116_S_SFT;
	clrsetbits_le32(&regs->afe_conn116_1, mask, val);

	/* AFE_CONN117_1 */
	/* I55 -> O117 */
	mask = I055_O117_S_MASK_SFT;
	val = I055_O117_S_VALUE << I055_O117_S_SFT;
	clrsetbits_le32(&regs->afe_conn117_1, mask, val);

	/* AFE_DL_24CH_CON0 */
	mask = DL_24CH_NUM_MASK_SFT | DL_24CH_MINLEN_MASK_SFT |
	       DL_24CH_SEL_FS_MASK_SFT | DL_24CH_PBUF_SIZE_MASK_SFT;
	val = (DL_24CH_NUM_VALUE << DL_24CH_NUM_SFT) |
	      (DL_24CH_MINLEN_VALUE << DL_24CH_MINLEN_SFT) |
	      (afe_fs << DL_24CH_SEL_FS_SFT) |
	      (DL_24CH_PBUF_SIZE_VALUE << DL_24CH_PBUF_SIZE_SFT);
	clrsetbits_le32(&regs->afe_dl_24ch_con0, mask, val);

	mtk_etdmout_enable(regs, true);

	/* Dump afe reg */
	mtk_afe_reg_dump(regs);

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
		memset(buf + update_size, 0, size - update_size);

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

	mtk_etdmout_enable(regs, true);

	data_cur = data_base = 0;
	data_size = length * sizeof(*data);
	/* 0s are padded at the end of data in case the DMA rings to the
	 * beginning of the buffer before disabling DMA.
	 */
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

	write32(&regs->afe_dl_24ch_base, buf_base);
	write32(&regs->afe_dl_24ch_end, buf_end);

	/* enable DL_24CH */
	setbits_le32(&regs->afe_dl_24ch_con0, BIT(DL_24CH_ON_SFT));

	/*
	 * To improve overall bandwidth, divide the buffer into 2 halves and use
	 * them as ping-pong buffers. While one half is being consumed by DMA,
	 * refill the other half.
	 */
	while(1) {
		/* Wait for the 1st half of buffer to be consumed. */
		I2S_LOG("afe_dl_24ch_cur: %p, cur: 0x%x\n",
		&regs->afe_dl_24ch_cur, read32(&regs->afe_dl_24ch_cur));
		while ((data_cur = read32(&regs->afe_dl_24ch_cur)) <= buf_mid) {
			if (data_base + data_cur > data_end)
				break;
		}
		if (data_base + data_cur > data_end)
			break;

		if (pad_remain) {
			pad_size = copy_to_buf((uint8_t *)buf_base,
					       SRAM_SIZE / 2,
					       &cur, &data_size);
			pad_remain -= MIN(pad_remain, pad_size);
		}

		/* Wait for the 2nd half of buffer to be consumed. */
		while ((data_cur = read32(&regs->afe_dl_24ch_cur)) >= buf_mid)
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

	clrbits_le32(&regs->afe_dl_24ch_con0, BIT(DL_24CH_ON_SFT));
	mtk_etdmout_enable(regs, false);
	return 0;
}

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate,
		    uint32_t word_len, uint32_t bit_len)
{
	MtkI2s *bus = xzalloc(sizeof(*bus));

	bus->component.ops.enable = &mtk_i2s_enable;
	bus->ops.send = &mtk_i2s_send;
	bus->regs = (void *)base;
	bus->channels = channels;
	bus->rate = rate;
	bus->word_len = word_len;
	bus->bit_len = bit_len;

	return bus;
}
