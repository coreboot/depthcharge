/*
 * Copyright 2026 Google LLC.
 * Depthcharge Driver for Qualcomm WSA8845 Smart Amplifier over SoundWire
 */

#include <delay.h>
#include <drivers/bus/soundwire/qcom/qcom_swrm.h>
#include <drivers/bus/soundwire/soundwire.h>
#include <drivers/soc/x1p42100.h>
#include <drivers/sound/wsa8845_sndw.h>
#include <libpayload.h>

/* MIPI ID for WSA8845: Version: 0x21, ManufID: 0x0217 (Qualcomm), PartID: 0x0204 */
const sndw_codec_id wsa8845_id = {
	.codec = {
		.version   = 0x21,
		.mfgid1    = 0x02,
		.mfgid2    = 0x17,
		.partid1   = 0x02,
		.partid2   = 0x04,
		.sndwclass = 0x00,
	}
};

static volatile lpass_wsa_intrctrl_cdcrx * const LPASS_WSA_CDC_INTR_CTRL = (volatile lpass_wsa_intrctrl_cdcrx *)LPASS_WSA_CDC_INTR_CTRL_BASE;
static volatile lpass_wsa_lpaifirq * const LPASS_WSA_LPAIF_IRQ = (volatile lpass_wsa_lpaifirq *)LPASS_WSA_LPAIF_IRQ_BASE;
static volatile lpass_wsa_clkrst_6b * const LPASS_WSA_CLK_RST = (volatile lpass_wsa_clkrst_6b *)LPASS_WSA_CLK_RST_BASE;
static volatile lpass_wsa_lpaifdmardregb_0 * const LPASS_WSA_LPAIF_RD_DMA_B0 = (volatile lpass_wsa_lpaifdmardregb_0 *)LPASS_WSA_LPAIF_RD_DMA_B0_BASE;
static volatile CPUn * const LPASS_WSA_CPUN = (volatile CPUn *)LPASS_WSA_CPUN_BASE;

#define BEEP_SAMPLE_RATE 48000
#define BEEP_FREQUENCY 375
#define BEEP_AMPLITUDE 0x5000
#define BEEP_LUT_SIZE 256

static const int16_t sine_lut[BEEP_LUT_SIZE] = {
       0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
   12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
   18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
   23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
   27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
   30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
   32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
   32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
   32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
   30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
   27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
   23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
   18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
   12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,
    6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
       0,  -804, -1608, -2410, -3212, -4011, -4808, -5602,
   -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
  -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,
  -18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
  -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
  -27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
  -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,
  -32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
  -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
  -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
  -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,
  -27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
  -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,
  -18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
  -12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179,
   -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804,
};

static void generate_sine_wave(int16_t *buffer, size_t num_frames, uint32_t freq,
	uint32_t sample_rate, int16_t amplitude)
{
	for (size_t i = 0; i < num_frames; i++) {
		uint32_t phase_idx = (i * freq * BEEP_LUT_SIZE / sample_rate) & 0xFF;
		buffer[i] = (int16_t)((sine_lut[phase_idx] * amplitude) >> 15);
	}
}

static void wsa8845_play_beep(void)
{
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_base = WSA_RDDMA_B0_BASE_ADDR;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_buf_len = WSA_RDDMA_BUF_LEN;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_buf_per_len = WSA_RDDMA_BUF_PER_LEN;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_ctl = WSA_RDDMA_CTL_PREPARE;
	LPASS_WSA_LPAIF_RD_DMA_B0->codec_intf = WSA_CODEC_INTF_B0_BASE;
	LPASS_WSA_LPAIF_RD_DMA_B0->codec_intf = WSA_CODEC_INTF_B0_ENABLE;
	mdelay(20);

	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CTL = RX_PATH_CTL_ENABLE_MIX;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_MIX_CTL = RX_PATH_MIX_ENABLE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_MIX_CTL = RX_PATH_CTL_ENABLE_MIX;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_MIX_CTL = RX_PATH_MIX_COMBINE;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CFG2 = RX_PATH_CFG2_DEFAULT;

	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ_CLEARa = LPAIF_IRQ_CLEAR_MASK_MISC;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ_ENa = LPAIF_IRQ_DISABLE_ALL;

	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CFG1 = RX_PATH_CFG1_TUNED;
	LPASS_WSA_CDC_INTR_CTRL->LPASS_WSA_CDC_RX0_RX_PATH_CTL = RX_PATH_CTL_ENABLE_MIX;

	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;

	LPASS_WSA_LPAIF_RD_DMA_B0->codec_intf = WSA_CODEC_INTF_B0_FLUSH;
	udelay(50);

	size_t total_samples = (WSA_RDDMA_BUF_LEN + 1) * sizeof(uint32_t) / sizeof(uint16_t);
	generate_sine_wave((int16_t *)WSA_RDDMA_B0_BASE_ADDR, total_samples, BEEP_FREQUENCY,
		BEEP_SAMPLE_RATE, BEEP_AMPLITUDE);
	dcache_clean_by_mva((void *)WSA_RDDMA_B0_BASE_ADDR, total_samples * sizeof(uint16_t));

	LPASS_WSA_LPAIF_RD_DMA_B0->codec_intf = WSA_CODEC_INTF_B0_ENABLE;
	udelay(50);
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_ctl = WSA_RDDMA_CTL_ENABLE;
	LPASS_WSA_LPAIF_RD_DMA_B0->codec_intf = WSA_CODEC_INTF_B0_RUN;
}

static void wsa8845_stop_beep(void)
{
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ2_ENa = LPAIF_IRQ_DISABLE;
	LPASS_WSA_LPAIF_IRQ->LPAIF_IRQ3_ENa = LPAIF_IRQ_DISABLE;

	LPASS_WSA_CPUN->CPUn_CMD_FIFO_WR_CMD = CPUN_CMD_FIFO_WR_STOP_B0;
	LPASS_WSA_CPUN->CPUn_CMD_FIFO_RD_CMD = CPUN_CMD_FIFO_RD_STOP_B0;

	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_ctl = WSA_RDDMA_CTL_STOP;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_ctl = WSA_RDDMA_DISABLE;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_base = WSA_RDDMA_DISABLE;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_buf_len = WSA_RDDMA_MIN_BUF_LEN;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_buf_per_len = WSA_RDDMA_DISABLE;
	LPASS_WSA_LPAIF_RD_DMA_B0->rddma_ctl = WSA_RDDMA_DISABLE;

	LPASS_WSA_CLK_RST->rst_ctrl_mclk = WSA_MCLK_ENABLE;
	LPASS_WSA_CLK_RST->rst_ctrl_swr = WSA_SWR_RESET_ENABLE;
}

/* Perform a beep */
static int wsa8845_beep(SndwOps *sndw, void *sndwlinkaddr, uint32_t deviceindex,
			uint32_t beep_duration)
{
	printf("%s: Generating beep on WSA8845 at index %u\n", __func__, deviceindex);
	wsa8845_play_beep();

	/* Wait for the duration */
	mdelay(beep_duration);

	wsa8845_stop_beep();

	printf("%s: Done\n", __func__);
	return 0;
}

/* Top-level play operation called by the sound subsystem */
static int wsa8845sndw_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	SoundDevice_sndw *codec = container_of(me, SoundDevice_sndw, ops);
	sndw_codec_info info = {.deviceindex = codec->device_index};

	/* Initialize the Master Bus */
	if (codec->sndw->sndw_enable(codec->sndw, &info)) {
		printf("WSA8845: Failed to enable SoundWire master.\n");
		return -1;
	}

	/* Verify Codec ID matches WSA8845 */
	for (int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if (wsa8845_id.id[i] != info.codecid.id[i]) {
			printf("WSA8845: Codec ID mismatch at byte %d.\n", i);
			return -1;
		}
	}

	/* Execute the beep/enable sequence */
	if (wsa8845_beep(codec->sndw, info.sndwlinkaddr, info.deviceindex, msec))
		return -1;

	/* Shutdown the Master Bus */
	if (codec->sndw->sndw_disable(codec->sndw)) {
		printf("WSA8845: Failed to disable master.\n");
		return -1;
	}

	return 0;
}

/* new_wsa8845_sndw - Allocate and initialize the WSA8845 Soundwire device */
SoundDevice_sndw *new_wsa8845_sndw(SndwOps *sndw, uint32_t device_index, uint32_t beep_duration)
{
	SoundDevice_sndw *codec = xzalloc(sizeof(*codec));
	codec->ops.play = &wsa8845sndw_play;
	codec->sndw = sndw;
	codec->device_index= device_index;
	codec->beep_duration = beep_duration;

	return codec;
}