/*
 * Copyright (C) 2025 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>

#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/bus/i2s/intel_common/i2s-dma.h"
#include "drivers/bus/i2s/intel_common/i2s-regs.h"
#include "drivers/sound/sound.h"
#include "drivers/sound/i2s.h"

#define TIMEOUT_MS 100
#define DMA_ALIGN 128

/* Status polling helper function */
static int audio_status_polling(void *base, uint32_t offset, uint32_t mask,
			        uint32_t expected_value)
{
	int timeout = TIMEOUT_MS;
	while (timeout-- > 0) {
		uint32_t current_value = read32(base + offset);
		if ((current_value & mask) == expected_value)
			return 0;
		mdelay(1);
	}
	printf("Audio Status Polling timeout at offset 0x%x\n", offset);
	return -1;
}

/* DMA Stream Allocation - Configure stream registers */
static void audio_dma_allocate(void *base, AUDIO_STREAM *stream)
{
	uint32_t data32;
	uint16_t data16;
	uint8_t data8;
	uint8_t bits_per_sample;

	/* Configure stream control byte 2 */
	data8 = stream->stream_id << N_OSDxCTL_B2_STRM;
	write8(base + R_OSDxCTL_B2(stream->stream_id), data8);

	/* Configure stream format */
	switch (stream->word_length) {
	case 16:
		bits_per_sample = V_OSDxFMT_BITS_16_IN_16;
		break;
	case 24:
		bits_per_sample = V_OSDxFMT_BITS_24_IN_32;
		break;
	case 32:
		bits_per_sample = V_OSDxFMT_BITS_32_IN_32;
		break;
	default:
		printf("ERROR: Unsupported bits per sample: %d\n", stream->word_length);
		bits_per_sample = V_OSDxFMT_BITS_16_IN_16;
	}

	data16 = ((bits_per_sample << N_OSDxFMT_BITS) | (stream->num_of_channels - 1));
	write16(base + R_OSDxFMT(stream->stream_id), data16);

	/* Configure Cyclic Buffer Length */
	data32 = stream->data_size;
	write32(base + R_OSDxCBL(stream->stream_id), data32);

	/* Configure R_OSDxLVI - Last Valid Index */
	data16 = stream->bdl_entries - 1;
	write16(base + R_OSDxLVI(stream->stream_id), data16);

	/* Configure Buffer Descriptor List (BDL) Lower Base Address */
	data32 = (uint32_t)stream->bdl_address;
	write32(base + R_OSDxBDLPLBA(stream->stream_id), data32);

	/* Configure Buffer Descriptor List (BDL) Upper Base Address */
	data32 = (uint32_t)(stream->bdl_address >> 32);
	write32(base + R_OSDxBDLPUBA(stream->stream_id), data32);

	/* Configure R_OSDxCTL_B0 - Enable interrupts */
	data8 = (B_OSDxCTL_B0_IOCE | B_OSDxCTL_B0_FEIE | B_OSDxCTL_B0_DEIE);
	write8(base + R_OSDxCTL_B0(stream->stream_id), data8);
}

/* DMA Stream Initialize - Reset position counters */
static void audio_dma_initialize(void *base, AUDIO_STREAM *stream)
{
	/* Reset Link Position in Buffer */
	write32(base + R_OSDxLPIB(stream->stream_id), 0);

	/* Reset Linear Link Position Lower */
	write32(base + R_OPPHCxLLPL(stream->stream_id), 0);

	/* Reset Linear Link Position Upper */
	write32(base + R_OPPHCxLLPU(stream->stream_id), 0);

	/* Reset Linear DMA Position Lower */
	write32(base + R_OPPHCxLDPL(stream->stream_id), 0);

	/* Reset Linear DMA Position Upper */
	write32(base + R_OPPHCxLDPU(stream->stream_id), 0);
}

/* DMA Stream Free - Reset and clear stream */
static void audio_dma_free(void *base, AUDIO_STREAM *stream)
{
	/* Clear all control bits first */
	write8(base + R_OSDxCTL_B0(stream->stream_id), 0);
	audio_status_polling(base, R_OSDxCTL_B0(stream->stream_id), 0xFF, 0x0);

	/* Set stream reset bit */
	write8(base + R_OSDxCTL_B0(stream->stream_id), B_OSDxCTL_B0_SRST);
	audio_status_polling(base, R_OSDxCTL_B0(stream->stream_id), 0xFF,
			     B_OSDxCTL_B0_SRST);

	/* Clear stream reset bit */
	write8(base + R_OSDxCTL_B0(stream->stream_id), 0);
	audio_status_polling(base, R_OSDxCTL_B0(stream->stream_id), 0xFF, 0x0);
}

/* DMA Stream Run - Start DMA operation */
static int audio_dma_run(void *base, AUDIO_STREAM *stream)
{
	/* Set the RUN bit to start DMA */
	uint8_t data8 = read8(base + R_OSDxCTL_B0(stream->stream_id)) | B_OSDxCTL_B0_RUN;
	write8(base + R_OSDxCTL_B0(stream->stream_id), data8);

	/* Wait for RUN bit to be set */
	return audio_status_polling(base, R_OSDxCTL_B0(stream->stream_id),
				    B_OSDxCTL_B0_RUN, B_OSDxCTL_B0_RUN);
}

/* DMA Stream Stop - Stop DMA operation */
static int audio_dma_stop(void *base, AUDIO_STREAM *stream)
{
	/* Clear the RUN bit to stop DMA */
	write8(base + R_OSDxCTL_B0(stream->stream_id), 0);

	/* Wait for RUN bit to be cleared */
	return audio_status_polling(base, R_OSDxCTL_B0(stream->stream_id),
				    B_OSDxCTL_B0_RUN, 0x0);
}

/* DMA Stream Clear - Clear all stream registers */
static void audio_dma_clear(void *base, AUDIO_STREAM *stream)
{
	write8(base + R_OSDxCTL_B2(stream->stream_id), 0);
	write16(base + R_OSDxFMT(stream->stream_id), 0);
	write32(base + R_OSDxCBL(stream->stream_id), 0);
	write16(base + R_OSDxLVI(stream->stream_id), 0);
	write32(base + R_OSDxBDLPLBA(stream->stream_id), 0);
	write32(base + R_OSDxBDLPUBA(stream->stream_id), 0);
}

static int enable_dsp_ssp_dma(I2s *bus)
{
	/* Power on audio controller */
	write32(bus->lpe_bar0 + POWER_OFFSET, 0x1);
	if (audio_status_polling(bus->lpe_bar0, POWER_OFFSET, 0x1, 0x1) != 0) {
		printf("DMA Boot Beep: ERROR - Failed to power on audio controller\n");
		return -1;
	}

	/* Disable PPCTL.GPROCEN (This bit was set before depthcharge) */
	clear_PPCTL_reg(bus->lpe_bar0, PROCEN);

	/* Power up I2S port(1) for SSP access */
	uint32_t i2slctl = read32(bus->lpe_bar0 + I2SLCTL);
	write32(bus->lpe_bar0 + I2SLCTL, i2slctl | I2SLCTL_SPA_MASK(1));

	/* Wait for I2S port power-up confirmation */
	if (audio_status_polling(bus->lpe_bar0, I2SLCTL, I2SLCTL_CPA_MASK(1), I2SLCTL_CPA_MASK(1)) != 0) {
		printf("DMA Boot Beep: ERROR - I2S port 1 power-up timeout\n");
		return -1;
	}
	return 0;
}

/* Enable I2s DMA Transfer */
static void i2s_dma_enable(I2sRegs *regs)
{
	/* Enable SSP (SSE bit in SSCR0) */
	set_SSCR0_reg(regs, SSE);

	/* Enable TSRE in SSMOD0CS */
	set_SSMODYCS_reg(regs, TSRE);
}

/* Disable I2s DMA transfer */
static void i2s_dma_disable(I2sRegs *regs)
{
	/* Disable TXEN and TSRE in SSMOD0CS */
	clear_SSMODYCS_reg(regs, TXEN);  /* Tx Disable */
	clear_SSMODYCS_reg(regs, TSRE);  /* Tx Service Request Disable */

	/* Disable SSP (SSE bit in SSCR0) */
	clear_SSCR0_reg(regs, SSE);
}

/* Clear specific audio interface stream validation registers */
static void clear_stream_validation_registers(void *base)
{
	/* Audio interface stream validation registers to clear */
	static const uint32_t reg_offsets[] = {
		R_SNDWLOSIDV,    /* SoundWire Link */
		R_HDALOSIDV,     /* HD Audio */
		R_IDALOSIDV,     /* Intel Display Audio */
		R_DMICLOSIDV,    /* Digital Mic */
		R_UAOLLOSIDV,    /* USB Audio Offload */
	};

	for (size_t i = 0; i < ARRAY_SIZE(reg_offsets); i++)
		write32(base + reg_offsets[i], 0);
}

/* I2S Stream Configuration */
static void i2s_dma_link_config(I2s *bus, AUDIO_STREAM *stream)
{
	/* Clear all other audio interface stream validations first */
	clear_stream_validation_registers(bus->lpe_bar0);

	/* Enable I2S Link Output Stream ID Validation for DMA stream */
	void *reg_addr = bus->lpe_bar0 + R_I2SLOSIDV;
	write32(reg_addr, read32(reg_addr) | (1 << stream->stream_id));

	/* Configure I2S PCM Stream Channel Mapping  */
	uint16_t data16;
	uint16_t current_value;
	uint8_t lowest_channel;
	uint8_t highest_channel;

	/* Channel mapping for stereo */
	lowest_channel = 0;                           /* Channel 0 (left) */
	highest_channel = stream->num_of_channels - 1;  /* Channel 1 (right) for stereo */

	/* Read current value to preserve reserved bits */
	reg_addr = bus->lpe_bar0 + I2S1PCMSxCM_OFFSET(8);
	current_value = read16(reg_addr); /* Always use PCM stream 8 for channel mapping */

	/* Define the mask for all fields being modified (STRM, HCHAN, LCHAN) */
	const uint16_t fields_mask = (I2S1PCMSxCM_STRM_MASK  << I2S1PCMSxCM_STRM_SHIFT) |
	                             (I2S1PCMSxCM_HCHAN_MASK << I2S1PCMSxCM_HCHAN_SHIFT) |
	                             (I2S1PCMSxCM_LCHAN_MASK << I2S1PCMSxCM_LCHAN_SHIFT);

	/* Clear the target fields in the current value */
	data16 = current_value & ~fields_mask;

	/* Calculate the new field values and merge them in */
	const uint16_t new_values = I2S1PCMSxCM_reg(STRM, stream->stream_id) |
	                            I2S1PCMSxCM_reg(HCHAN, highest_channel) |
	                            I2S1PCMSxCM_reg(LCHAN, lowest_channel);

	data16 |= new_values;

	/* Write the configuration */
	write16(reg_addr, data16);

	/* I2S link register configuration */
	i2s_dma_disable(bus->regs);
	i2s_set_ssp_hw(bus->regs, bus->settings, bus->bits_per_sample);
}

/* Create Buffer Descriptor List */
static BUFFER_DESCRIPTOR_LIST_ENTRY *create_bdl(const void *data, uint32_t buffer_size, size_t *bdl_count_out)
{
	uint32_t remaining = buffer_size;
	const uint8_t *buf = (const uint8_t *)data;

	/* Compute number of BDL entries */
	*bdl_count_out = MAX(DIV_ROUND_UP(remaining, MAX_BDLE_BYTES), 2);

	/* Allocate 128B-aligned BDL array */
	BUFFER_DESCRIPTOR_LIST_ENTRY *bdl = memalign(DMA_ALIGN, *bdl_count_out *
						     sizeof(BUFFER_DESCRIPTOR_LIST_ENTRY));
	if (!bdl) {
		printf("DMA Send: Failed to allocate BDL array (%zu entries)\n", *bdl_count_out);
		return NULL;
	}

	/* Reset remaining for filling entries */
	remaining = buffer_size;

	/* Fill BDLEs: keep 128-byte alignment except for last entry */
	for (size_t i = 0; i < *bdl_count_out; i++) {
		uint32_t len;
		if (i == (*bdl_count_out - 1)) {
			/* Last entry takes remaining bytes */
			len = remaining ? remaining : 128;
		} else {
			uint32_t chunk = MIN(remaining, MAX_BDLE_BYTES);
			/* Round down to 128-byte boundary */
			len = (chunk / DMA_ALIGN) * DMA_ALIGN;
			if (len == 0)
				len = (remaining >= DMA_ALIGN) ? DMA_ALIGN : remaining; /* Fallback when chunk < 128 */
		}

		bdl[i].address  = (uint64_t)(uintptr_t)buf;
		bdl[i].length   = len;
		bdl[i].ioc      = (i == (*bdl_count_out - 1)) ? 1 : 0;
		bdl[i].reserved = 0;

		buf += len;
		remaining -= MIN(remaining, len);
	}

	return bdl;
}

static int i2s_dma_send(I2sOps *me, unsigned int *data, unsigned int length)
{
	I2s *bus = container_of(me, I2s, ops);
	BUFFER_DESCRIPTOR_LIST_ENTRY *bdl;
	size_t bdl_count = 0;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t bits_per_sample;
	uint32_t duration_ms;

	/* Get audio parameters from bus configuration */
	sample_rate = bus->settings->fsync_rate;
	channels = bus->settings->frame_rate_divider_ctrl;
	bits_per_sample = bus->bits_per_sample;

	/* Calculate playback duration */
	uint32_t bytes = length * sizeof(*data);
	uint32_t samples = bytes / (channels * (bits_per_sample / 8));
	duration_ms = (samples * 1000) / sample_rate;

	/* Power up audio controller and I2S */
	if (enable_dsp_ssp_dma(bus) != 0) {
		printf("DMA Send: ERROR - Failed to power up Audio controller & "
		       "I2S/PCM link\n");
		return -1;
	}

	/* Configure PPCTL for direct DMA coupling */
	clear_PPCTL_reg(bus->lpe_bar0, PROCEN);

	/* Create Buffer Descriptor List (BDL) */
	bdl = create_bdl(data, bytes, &bdl_count);
	if (!bdl) {
		printf("DMA Send: ERROR - Failed to create BDL\n");
		return -1;
	}

	/* Initialize audio stream structure using our BDL array */
	AUDIO_STREAM audio_stream = {
		.stream_id = 1,				/* Use DMA stream 1 */
		.data_size = bytes,			/* Total buffer size */
		.bdl_entries = bdl_count,		/* Number of BDLEs */
		.bdl_address = (uint64_t)(uintptr_t)bdl,	/* Physical address of BDL array */
		.word_length = bits_per_sample,		/* bits per sample */
		.num_of_channels = channels		/* Number of channels */
	};

	/* DMA initialization sequence */
	audio_dma_free(bus->lpe_bar0, &audio_stream);
	audio_dma_allocate(bus->lpe_bar0, &audio_stream);
	audio_dma_initialize(bus->lpe_bar0, &audio_stream);

	/* Configure I2S PCM and SSP */
	i2s_dma_link_config(bus, &audio_stream);

	/* Enable speaker GPIO */
	if (bus->sdmode_gpio)
		gpio_set(bus->sdmode_gpio, 1);
	else
		printf("DMA Send: Warning - No speaker GPIO configured\n");

	/* Start I2S */
	i2s_dma_enable(bus->regs);

	/* Start DMA transfer */
	if (audio_dma_run(bus->lpe_bar0, &audio_stream) == 0) {
		while (extract_SSMODYCS_TFL(read_SSMODYCS(bus->regs)) == 0)
			mdelay(1);
		set_SSMODYCS_reg(bus->regs, TXEN);  /* Tx Enable */

		/* Wait for audio playback duration */
		mdelay(duration_ms);
	} else {
		printf("DMA Send: ERROR - Failed to start DMA\n");
	}

	/* Stop DMA transfer */
	audio_dma_stop(bus->lpe_bar0, &audio_stream);

	/* Stop I2S */
	i2s_dma_disable(bus->regs);

	/* Clean up DMA resources */
	audio_dma_free(bus->lpe_bar0, &audio_stream);
	audio_dma_clear(bus->lpe_bar0, &audio_stream);

	free(bdl);

	/* Disable speaker GPIO */
	if (bus->sdmode_gpio)
		gpio_set(bus->sdmode_gpio, 0);

	return 0;
}

I2s *new_i2s_dma_structure(const I2sSettings *settings, int bps,
			   GpioOps *sdmode, uintptr_t ssp_i2s_start_address)
{
	I2s *i2s = new_i2s_structure(settings, bps, sdmode, ssp_i2s_start_address);
	i2s->ops.send = &i2s_dma_send;
	return i2s;
}
