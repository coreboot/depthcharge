// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright 2021 Google LLC.  */

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/sound/amd_acp.h"
#include "drivers/timer/timer.h"
#include "pci.h"

#define ACP_DEBUG 0

#if ACP_DEBUG
#define acp_debug(...) printf(__VA_ARGS__)
#else
#define acp_debug(...)                                                         \
	do { } while (0)
#endif

/* ACP pin control registers */
#define ACP_SOFT_RESET			0x1000
#define  ACP_SOFT_RESET_AUD		BIT(0)
#define  ACP_SOFT_RESET_DMA		BIT(1)
#define  ACP_SOFT_RESET_DSP		BIT(2)
#define  ACP_SOFT_RESET_AUD_DONE	BIT(16)
#define  ACP_SOFT_RESET_DMA_DONE	BIT(17)
#define  ACP_SOFT_RESET_DSP_DONE	BIT(18)
#define ACP_CONTROL			0x1004
#define   ACP_CONTROL_CLK_EN		BIT(0)
#define ACP_STATUS			0x1008
#define   ACP_CONTROL_CLK_ON		BIT(0)

#define ACP_I2S_PIN_CONFIG		0x1400
#define  ACP_I2S_PIN_CONFIG_I2S_TDM	0x4
#define  ACP_I2S_PIN_CONFIG_DEFAULT	0x7
#define ACP_PAD_PULLUP_PULLDOWN_CTRL	0x1404
#define ACP_PGFSM_CONTROL		0x141c
#define  ACP_PGFSM_CTRL			BIT(0)
#define ACP_PGFSM_STATUS		0x1420
#define  ACP_PGFSM_POWER_ON		0x0
#define  ACP_PGFSM_POWER_ON_PENDING	0x1
#define  ACP_PGFSM_POWER_OFF		0x2
#define  ACP_PGFSM_POWER_OFF_PENDING	0x3
#define  ACP_PGFSM_POWER_MASK		0x3

#define ACP_ERROR_STATUS		0x18C4
#define ACP_SW_I2S_ERROR_REASON		0x18C8

#define ACP_I2S_TX_RINGBUFADDR		0x2024
#define ACP_I2S_TX_RINGBUFSIZE		0x2028
#define ACP_I2S_TX_FIFOADDR		0x2030
#define ACP_I2S_TX_FIFOSIZE		0x2034
#define ACP_I2S_TX_DMA_SIZE		0x2038

#define ACP_BT_TX_RINGBUFADDR		0x206C
#define ACP_BT_TX_RINGBUFSIZE		0x2070
#define ACP_BT_TX_FIFOADDR		0x2078
#define ACP_BT_TX_FIFOSIZE		0x207C
#define ACP_BT_TX_DMA_SIZE		0x2080

#define ACP_I2STDM_IER			0x2400
#define  ACP_I2STDM_IEN			BIT(0)
#define ACP_I2STDM_ITER			0x240C
#define  ACP_I2STDM_TXEN		BIT(0)
#define  ACP_I2STDM_TX_SAMP_LEN_8	(0 << 3)
#define  ACP_I2STDM_TX_SAMP_LEN_12	(1 << 3)
#define  ACP_I2STDM_TX_SAMP_LEN_16	(2 << 3)
#define  ACP_I2STDM_TX_SAMP_LEN_24	(4 << 3)
#define  ACP_I2STDM_TX_SAMP_LEN_32	(5 << 3)
#define  ACP_I2STDM_TX_STATUS		BIT(6)

#define ACP_BTTDM_IER			0x2800
#define ACP_BTTDM_ITER			0x280C

#define ACP_SCRATCH_REG(n)		(0x10000 + (sizeof(uint32_t) * (n)))
#define ACP_SCRATCH_REG_MAX		6143 /* 24,572 bytes */

#define ACP_DMA_CNTL_0			0x000
#define  ACP_DMA_CH_RST			BIT(0)
#define  ACP_DMA_CH_RUN			BIT(1)
#define ACP_DMA_DSCR_STRT_IDX_0		0x020
#define ACP_DMA_DSCR_CNT_0		0x040
#define ACP_DMA_ERR_STS_0		0x0C0
#define  ACP_DMA_CH_ERR			BIT(0)
#define  ACP_DMA_CH_CNT_ERR		BIT(1)
#define  ACP_DMA_CH_DEST_ERR		BIT(2)
#define  ACP_DMA_CH_SRC_ERR		BIT(3)
#define  ACP_DMA_CH_DESC_ERR		BIT(4)
#define ACP_DMA_DESC_BASE_ADDR		0x0E0
#define ACP_DMA_DESC_MAX_NUM_DSCR	0x0E4
#define ACP_DMA_CH_STS			0x0E8
#define ACP_DMA_CH_STS_CH(chan)	(BIT(chan))

struct __packed acp_audio_buffer {
	uint32_t ring_buf_addr;
	uint32_t ring_buf_size;
	uint32_t link_position_cntr;
	uint32_t fifo_addr;
	uint32_t fifo_size;
	uint32_t dma_size;
	uint32_t linear_position_cntr_high;
	uint32_t linear_position_cntr_low;
	uint32_t intr_watermark_size;
};

struct __packed acp_i2s_ctrl {
	uint32_t ier; /* I2S Enable Register */
	uint32_t irer; /* I2S Receiver Enable Register */
	uint32_t rx_frmt;
	uint32_t iter; /* I2S Transmitter Enable Register */
	uint32_t tx_frmt;
};

/* Our ACP memory map */
#define ACP_DRAM		0x01000000
#define ACP_SCRATCH_FIFO	0x02050000 /* ACP_SCRATCH_REG(0) */
#define ACP_DMA_DESC		0X02050200 /* ACP_SCRATCH_REG(128) */
#define ACP_DMA_DESC_IDX	128
#define ACP_SCRATCH_TONE	0x02051000 /* ACP_SCRATCH_REG(1024) */
#define ACP_SCRATCH_TONE_IDX	1024
#define ACP_DMA_BLOCK_SIZE	64

typedef struct AmdAcpPrivate {
	struct AmdAcp ext;

	pcidev_t pci_dev;
	uint8_t *pci_bar;

	struct acp_audio_buffer *audio_buffer;
	struct acp_i2s_ctrl *audio_buffer_ctrl;

	enum acp_output_port port;
	uint32_t lrclk;
	uint8_t channels;
	int16_t volume;

	uint32_t original_pin_config;
} AmdAcpPrivate;

/*
 * Generates a square wave tone table that is a multiple of block_size. This
 * allows the tone table to be used in a ring buffer and play for an arbitrary
 * amount of time.
 */
static unsigned int amd_i2s_square_wave(AmdAcpPrivate *acp, int16_t **data,
					unsigned int frequency,
					unsigned int block_size)
{
	unsigned int samples_per_period;
	unsigned int first_half, second_half;
	unsigned int max_hz = acp->lrclk / 2;
	unsigned int bytes_per_period;
	unsigned int periods;
	unsigned int total_bytes;

	if (frequency > max_hz) {
		printf("ERROR: frequency %u > max_hz %d\n", frequency, max_hz);
		return 0;
	}

	samples_per_period = acp->lrclk / frequency;
	first_half = samples_per_period /
		     2; /* Integer division will round this down */
	second_half = samples_per_period - first_half;

	bytes_per_period =
		samples_per_period * acp->channels * sizeof(uint16_t);

	/* Find the least common multiple of bytes_per_period and block_size */
	for (periods = 1; (bytes_per_period * periods) % block_size; ++periods)
		;

	total_bytes = bytes_per_period * periods;

	acp_debug("%s: lrclk: %u, frequency: %u, samples_per_period: %u, "
		  "first_half: %u, second_half: %u, bytes_per_period: %u, "
		  "periods: %u, total_bytes: %u, volume: %#x\n",
		  __func__, acp->lrclk, frequency, samples_per_period,
		  first_half, second_half, bytes_per_period, periods,
		  total_bytes, acp->volume);

	int16_t *samples = xmalloc(total_bytes);
	*data = samples;

	for (unsigned int period = 0; period < periods; ++period) {
		for (unsigned int sample = 0; sample < first_half; sample++) {
			for (unsigned int channel = 0; channel < acp->channels;
			     ++channel) {
				*samples++ = acp->volume;
			}
		}

		for (unsigned int sample = 0; sample < second_half; sample++) {
			for (unsigned int channel = 0; channel < acp->channels;
			     ++channel) {
				*samples++ = -acp->volume;
			}
		}
	}

	return total_bytes;
}

static void acp_write32(AmdAcpPrivate *acp, uint32_t reg, uint32_t val)
{
	write32(acp->pci_bar + reg, val);
}

static uint32_t acp_read32(AmdAcpPrivate *acp, uint32_t reg)
{
	return read32(acp->pci_bar + reg);
}

static int amd_acp_dma_copy(AmdAcpPrivate *acp, uint32_t acp_src,
			    uint32_t acp_dest, uint32_t size)
{
	struct stopwatch sw;

	acp_write32(acp, ACP_SCRATCH_REG(ACP_DMA_DESC_IDX + 0), acp_src);
	acp_write32(acp, ACP_SCRATCH_REG(ACP_DMA_DESC_IDX + 1), acp_dest);
	acp_write32(acp, ACP_SCRATCH_REG(ACP_DMA_DESC_IDX + 2), size);

	acp_write32(acp, ACP_DMA_DESC_BASE_ADDR, ACP_DMA_DESC);
	acp_write32(acp, ACP_DMA_DESC_MAX_NUM_DSCR, 1);
	acp_write32(acp, ACP_DMA_DSCR_CNT_0, 1);
	acp_write32(acp, ACP_DMA_DSCR_STRT_IDX_0, 0);

	acp_write32(acp, ACP_DMA_CNTL_0, ACP_DMA_CH_RUN);

	stopwatch_init_usecs_expire(&sw, 1000);
	while (acp_read32(acp, ACP_DMA_CH_STS) & ACP_DMA_CH_STS_CH(0)) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out waiting for DMA to "
			       "complete\n",
			       __func__);
			return -1;
		}
	}

	uint32_t err = acp_read32(acp, ACP_DMA_ERR_STS_0);
	if (err & ACP_DMA_CH_ERR) {
		printf("%s: ERROR: DMA Error: %#x\n", __func__, err);
		return -1;
	}

	return 0;
}

static int amd_acp_write_tone_table_to_scratch(AmdAcpPrivate *acp,
					       int16_t *data, unsigned int size)
{
	unsigned int words = size / sizeof(uint32_t);
	uint32_t *data32 = (uint32_t *)data;

	if (size % sizeof(uint32_t)) {
		printf("%s: ERROR: size %u is not a multiple of %zu\n",
		       __func__, size, sizeof(uint32_t));
		return -1;
	}

	if (ACP_SCRATCH_TONE_IDX + words > ACP_SCRATCH_REG_MAX) {
		printf("%s: ERROR: size %u would overflow scratch register "
		       "region\n",
		       __func__, size);
		return -1;
	}

	for (unsigned int i = 0; i < words; ++i) {
		acp_write32(acp, ACP_SCRATCH_REG(ACP_SCRATCH_TONE_IDX + i),
			    data32[i]);

		/* Print out part of tone table for manual inspection. */
		if (i < 48) {
			acp_debug("%#08x", data32[i]);
			if (i > 0 && i % 4 == 0)
				acp_debug("\n");
			else
				acp_debug(" ");
		}
	}
	acp_debug("\n");

	return 0;
}

static int amd_acp_start(SoundOps *me, uint32_t frequency)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);
	unsigned int table_size;
	int16_t *data __attribute__((cleanup(free))) = NULL;

	table_size =
		amd_i2s_square_wave(acp, &data, frequency, ACP_DMA_BLOCK_SIZE);

	if (!table_size) {
		printf("%s: ERROR: Failed to generate tone table\n", __func__);
		return -1;
	}

	if (amd_acp_write_tone_table_to_scratch(acp, data, table_size) < 0)
		return -1;

	if (amd_acp_dma_copy(acp, ACP_SCRATCH_TONE, ACP_DRAM, table_size))
		return -1;

	/* Clear errors */
	acp_write32(acp, ACP_ERROR_STATUS, 0xFFFFFFFF);
	acp_write32(acp, ACP_SW_I2S_ERROR_REASON, 0xFFFFFFFF);

	write32(&acp->audio_buffer->ring_buf_addr, ACP_DRAM);
	write32(&acp->audio_buffer->ring_buf_size, table_size);

	write32(&acp->audio_buffer->fifo_addr, ACP_SCRATCH_FIFO);
	write32(&acp->audio_buffer->fifo_size, 256);
	write32(&acp->audio_buffer->dma_size, ACP_DMA_BLOCK_SIZE);

	write32(&acp->audio_buffer_ctrl->ier, ACP_I2STDM_IEN);
	write32(&acp->audio_buffer_ctrl->iter,
		ACP_I2STDM_TX_SAMP_LEN_16 | ACP_I2STDM_TXEN);

	return 0;
}

static int amd_acp_stop(SoundOps *me)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);
	uint32_t reg, err, reason;
	struct stopwatch sw;

	reg = read32(&acp->audio_buffer_ctrl->iter);

	write32(&acp->audio_buffer_ctrl->iter, reg & ~ACP_I2STDM_TXEN);

	stopwatch_init_usecs_expire(&sw, 1000);
	while (read32(&acp->audio_buffer_ctrl->iter) & ACP_I2STDM_TX_STATUS) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out waiting channel to stop\n",
			       __func__);
			return -1;
		}
	}

	write32(&acp->audio_buffer_ctrl->iter, 0);
	write32(&acp->audio_buffer_ctrl->ier, 0);

	err = acp_read32(acp, ACP_ERROR_STATUS);
	if (err) {
		reason = acp_read32(acp, ACP_SW_I2S_ERROR_REASON);

		printf("%s: Error: %#x, Reason: %#x\n", __func__, err, reason);

		return -1;
	}

	return 0;
}

static int amd_acp_set_volume(struct SoundOps *me, uint32_t volume)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);

	/* Convert volume to percentage of full-scale int16_t. */
	acp->volume = (volume * 0x7fff) / 100;

	return 0;
}

static uint8_t amd_acp_get_power_status(AmdAcpPrivate *acp)
{
	uint32_t reg = acp_read32(acp, ACP_PGFSM_STATUS);
	return reg & ACP_PGFSM_POWER_MASK;
}

static int amd_acp_power_on(AmdAcpPrivate *acp)
{
	uint32_t reg;
	struct stopwatch sw;

	reg = acp_read32(acp, ACP_PGFSM_CONTROL);

	if ((reg & ACP_PGFSM_CTRL) == 0) {
		acp_debug("%s: ACP is powered off, power on\n", __func__);
		acp_write32(acp, ACP_PGFSM_CONTROL, reg | ACP_PGFSM_CTRL);
	}

	stopwatch_init_usecs_expire(&sw, 1000);
	while (amd_acp_get_power_status(acp) != ACP_PGFSM_POWER_ON) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out while power on ACP\n",
			       __func__);
			return -1;
		}
	}

	return 0;
}

static int amd_acp_clk_power_on(AmdAcpPrivate *acp)
{
	uint32_t reg;
	struct stopwatch sw;
	reg = acp_read32(acp, ACP_CONTROL);

	if ((reg & ACP_CONTROL_CLK_EN) == 0) {
		acp_debug("%s: ACP CLK is disabled, enable\n", __func__);
		acp_write32(acp, ACP_CONTROL, reg | ACP_CONTROL_CLK_EN);
	}

	stopwatch_init_usecs_expire(&sw, 1000);
	while (!(acp_read32(acp, ACP_STATUS) & ACP_CONTROL_CLK_ON)) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out starting clock", __func__);
			return -1;
		}
	}

	return 0;
}

static int amd_acp_deassert_reset(AmdAcpPrivate *acp)
{
	uint32_t reg;
	struct stopwatch sw;

	reg = acp_read32(acp, ACP_SOFT_RESET);

	if (reg) {
		acp_debug("%s: ACP is in reset, enable: %#x\n", __func__, reg);
		acp_write32(acp, ACP_SOFT_RESET, 0);
	}

	stopwatch_init_usecs_expire(&sw, 1000);
	while (acp_read32(acp, ACP_SOFT_RESET) != 0) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out de-asserting reset",
			       __func__);
			return -1;
		}
	}

	return 0;
}

static int amd_acp_enable(SoundRouteComponentOps *me)
{
	uintptr_t bar;
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.component.ops);

	acp_debug("%s: start\n", __func__);

	if (get_pci_bar(acp->pci_dev, &bar)) {
		printf("%s: ERROR: Failed to get BAR for PCI %d.%d.%d",
		       __func__, PCI_BUS(acp->pci_dev), PCI_SLOT(acp->pci_dev),
		       PCI_FUNC(acp->pci_dev));
		return -1;
	}

	acp->pci_bar = (uint8_t *)bar;

	switch (acp->port) {
	case ACP_OUTPUT_PLAYBACK:
		acp->audio_buffer =
			(void *)(acp->pci_bar + ACP_I2S_TX_RINGBUFADDR);
		acp->audio_buffer_ctrl =
			(void *)(acp->pci_bar + ACP_I2STDM_IER);
		break;
	case ACP_OUTPUT_BT:
		acp->audio_buffer =
			(void *)(acp->pci_bar + ACP_BT_TX_RINGBUFADDR);
		acp->audio_buffer_ctrl = (void *)(acp->pci_bar + ACP_BTTDM_IER);
		break;
	default:
		printf("%s: ERROR: Unknown port %u\n", __func__, acp->port);
	}

	if (amd_acp_power_on(acp))
		return -1;

	if (amd_acp_clk_power_on(acp))
		return -1;

	if (amd_acp_deassert_reset(acp))
		return -1;

	/* Save the PIN_CONFIG so we don't stomp on what coreboot defined */
	acp->original_pin_config = acp_read32(acp, ACP_I2S_PIN_CONFIG);

	acp_write32(acp, ACP_I2S_PIN_CONFIG, ACP_I2S_PIN_CONFIG_I2S_TDM);

	return 0;
}

static int amd_acp_disable(SoundRouteComponentOps *me)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.component.ops);

	acp_debug("%s: start\n", __func__);

	acp_write32(acp, ACP_I2S_PIN_CONFIG, acp->original_pin_config);

	return 0;
}

AmdAcp *new_amd_acp(pcidev_t pci_dev, enum acp_output_port port, uint32_t lrclk,
		    int16_t volume)
{
	AmdAcpPrivate *acp = xzalloc(sizeof(*acp));

	acp->ext.component.ops.enable = &amd_acp_enable;
	acp->ext.component.ops.disable = &amd_acp_disable;

	acp->ext.sound_ops.start = &amd_acp_start;
	acp->ext.sound_ops.stop = &amd_acp_stop;
	acp->ext.sound_ops.set_volume = &amd_acp_set_volume;

	acp->port = port;
	acp->pci_dev = pci_dev;
	acp->lrclk = lrclk;
	acp->channels = 2;
	acp->volume = volume;

	return &acp->ext;
}
