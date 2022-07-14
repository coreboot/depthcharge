/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2022 Google LLC.  */

#ifndef __DRIVERS_SOUND_AMD_ACP_PRIVATE__H__
#define __DRIVERS_SOUND_AMD_ACP_PRIVATE__H__

#include <libpayload.h>

#include "drivers/sound/amd_acp.h"

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

void acp_write32(AmdAcpPrivate *acp, uint32_t reg, uint32_t val);
uint32_t acp_read32(AmdAcpPrivate *acp, uint32_t reg);
unsigned int amd_i2s_square_wave(AmdAcpPrivate *acp, int16_t **data,
				 unsigned int frequency);

/* SOC-specific implementations of necessary AMD ACP functions */
void acp_clear_error_status(AmdAcpPrivate *acp);
int acp_check_error_status(AmdAcpPrivate *acp);
int amd_acp_start(SoundOps *me, uint32_t frequency);
void save_pin_config(AmdAcpPrivate *acp);
void restore_pin_config(AmdAcpPrivate *acp);

#endif /* __DRIVERS_SOUND_AMD_ACP_PRIVATE__H__ */
