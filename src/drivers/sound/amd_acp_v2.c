// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright 2022 Google LLC.  */

#include "drivers/sound/amd_acp_v2.h"
#include "drivers/sound/amd_acp_private.h"

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
	acp_write32(acp, ACP_P1_SW_I2S_ERROR_REASON, 0x0);
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
		reason = acp_read32(acp, ACP_P1_SW_I2S_ERROR_REASON);
		printf("%s: Error: ACP_P1_SW_I2S_ERROR_REASON: %#08x\n",
			__func__, reason);

		return -1;
	}

	return 0;
}

static int amd_acp_write_tone_table_to_dsp_dram(AmdAcpPrivate *acp,
						int16_t *data,
						unsigned int size)
{
	unsigned int words;
	uint32_t *data32 = (uint32_t *)data;

	if (size % sizeof(uint32_t)) {
		printf("%s: ERROR: size %u is not a multiple of %zu\n",
		       __func__, size, sizeof(uint32_t));
		return -1;
	}

	words = size / sizeof(uint32_t);

	acp_write32(acp, ACP_SRBM_Targ_Idx_Addr,
		SRBM_Targ_Idx_addr_auto_increment_enable | ACP_DRAM);

	for (unsigned int i = 0; i < words; ++i) {
		acp_write32(acp, ACP_SRBM_Targ_Idx_Data, data32[i]);

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

static void amd_acp_start_master_clock(AmdAcpPrivate *acp)
{
	/* MCLK |Actual no  |Sample rate|word  |Required BCLK|  BCLK  |LRCLK
	 * (MHz)|of channels|  (Khz)    |length|Freq(KHz)    |DIVISION|DIVISION
	 * 196.6|     2     |    48     |  16  |    1536     |  128   |   32
	 */
	uint32_t temp = I2STDM2_MASTER_MODE | (128 << 11) | (32 << 2);
	acp_write32(acp, ACP_I2STDM2_MSTRCLKGEN, temp);
}

int amd_acp_start(SoundOps *me, uint32_t frequency)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);
	unsigned int table_size;
	int16_t *data __attribute__((cleanup(free))) = NULL;

	acp_clear_error_status(acp);

	table_size = amd_i2s_square_wave(acp, &data, frequency);
	acp_debug("%s: table_size = %d\n", __func__, table_size);

	if (!table_size) {
		printf("%s: ERROR: Failed to generate tone table\n", __func__);
		return -1;
	}

	if (amd_acp_write_tone_table_to_dsp_dram(acp, data, table_size) < 0)
		return -1;

	amd_acp_start_master_clock(acp);
	acp_check_error_status(acp);

	// Set to scratch memory address 0x00000000 to ACP_P1_HS_TX_FIFOADDR
	acp_write32(acp, ACP_P1_HS_TX_FIFOADDR, 0x00000000);
	// Set ACP_P1_HS_TX_FIFOSIZE to 256 bytes
	acp_write32(acp, ACP_P1_HS_TX_FIFOSIZE, 256);
	// Configure ACP_P1_HS_TX_RINGBUFADDR to 0x01000000 if ACP DRAM memory
	// is used, if system memory is used then set to 0x04000000
	acp_write32(acp, ACP_P1_HS_TX_RINGBUFADDR, ACP_DRAM);
	// Set ACP_P1_HS_TX_RINGBUFSIZE to the size of the ACP DRAM memory
	// buffer size or system memory buffer size of tone data
	acp_write32(acp, ACP_P1_HS_TX_RINGBUFSIZE, table_size);
	// Set ACP_P1_HS_TX_DMA_SIZE to 128 bytes
	acp_write32(acp, ACP_P1_HS_TX_DMA_SIZE, 128);
	// Set ACP_HSTDM_IER to 0x1
	acp_write32(acp, ACP_HSTDM_IER, 0x1);

	acp_write32(acp, ACP_HSTDM_ITER, ACP_HSTDM_ITER_HSTDM_TXEN |
		ACP_HSTDM_ITER_HSTDM_TX_SAMP_LEN_16b_20b);
	acp_write32(acp, ACP_HSTDM_TXFRMT, ACP_HSTDM_TXFRMT_HSTDM_SLOT_LEN_16b);

	acp_check_error_status(acp);

	return 0;
}

void save_pin_config(AmdAcpPrivate *acp)
{
	/* Save the PIN_CONFIG so we don't stomp on what coreboot defined */
	acp->original_pin_config = acp_read32(acp, ACP_PIN_CONFIG);
	acp_write32(acp, ACP_PIN_CONFIG, ACP_PIN_CONFIG_I2S);
}

void restore_pin_config(AmdAcpPrivate *acp)
{
	acp_write32(acp, ACP_PIN_CONFIG, acp->original_pin_config);
}
