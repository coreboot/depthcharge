// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright 2022 Google LLC.  */

#include "drivers/sound/amd_acp_v1.h"
#include "drivers/sound/amd_acp_private.h"
#include "drivers/timer/timer.h"

#define ACP_DEBUG 0

#if ACP_DEBUG
#define acp_debug(...) printf(__VA_ARGS__)
#else
#define acp_debug(...)                                                         \
	do { } while (0)
#endif

void acp_clear_error_status(AmdAcpPrivate *acp)
{
	acp_write32(acp, ACP_ERROR_STATUS, 0x0);
	acp_write32(acp, ACP_SW_I2S_ERROR_REASON, 0x0);
}

int acp_check_error_status(AmdAcpPrivate *acp)
{
	uint32_t err;

	err = acp_read32(acp, ACP_ERROR_STATUS);
	if (err) {
		uint32_t reason;

		printf("%s: Error: ACP_ERROR_STATUS: %#08x\n", __func__, err);
		reason = acp_read32(acp, ACP_SW_I2S_ERROR_REASON);
		printf("%s: Error: ACP_SW_I2S_ERROR_REASON: %#08x\n",
			__func__, reason);

		return -1;
	}

	return 0;
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
	unsigned int words;
	uint32_t *data32 = (uint32_t *)data;

	if (size % sizeof(uint32_t)) {
		printf("%s: ERROR: size %u is not a multiple of %zu\n",
		       __func__, size, sizeof(uint32_t));
		return -1;
	}

	words = size / sizeof(uint32_t);

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

int amd_acp_start(SoundOps *me, uint32_t frequency)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);
	unsigned int table_size;
	int16_t *data __attribute__((cleanup(free))) = NULL;

	acp_clear_error_status(acp);

	table_size = amd_i2s_square_wave(acp, &data, frequency);

	if (!table_size) {
		printf("%s: ERROR: Failed to generate tone table\n", __func__);
		return -1;
	}

	if (amd_acp_write_tone_table_to_scratch(acp, data, table_size) < 0)
		return -1;

	if (amd_acp_dma_copy(acp, ACP_SCRATCH_TONE, ACP_DRAM, table_size))
		return -1;

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

void save_pin_config(AmdAcpPrivate *acp)
{
	/* Save the PIN_CONFIG so we don't stomp on what coreboot defined */
	acp->original_pin_config = acp_read32(acp, ACP_I2S_PIN_CONFIG);
	acp_write32(acp, ACP_I2S_PIN_CONFIG, ACP_I2S_PIN_CONFIG_I2S_TDM);
}

void restore_pin_config(AmdAcpPrivate *acp)
{
	acp_write32(acp, ACP_I2S_PIN_CONFIG, acp->original_pin_config);
}
