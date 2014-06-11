/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/sound/tegra_ahub.h"

/*
 * Each TX CIF transmits data into the XBAR. Each RX CIF can receive audio
 * transmitted by a particular TX CIF.
 */

typedef struct TegraXbarRegs {
	uint32_t apbif_rx0;		/* AUDIO_APBIF_RX0, offset 0x00 */
	uint32_t apbif_rx1;		/* AUDIO_APBIF_RX1, offset 0x04 */
	uint32_t apbif_rx2;		/* AUDIO_APBIF_RX2, offset 0x08 */
	uint32_t apbif_rx3;		/* AUDIO_APBIF_RX3, offset 0x0C */

	uint32_t i2s0_rx0;		/* AUDIO_I2S0_RX0,  offset 0x10 */
	uint32_t i2s1_rx0;		/* AUDIO_I2S1_RX0,  offset 0x14 */
	uint32_t i2s2_rx0;		/* AUDIO_I2S2_RX0,  offset 0x18 */
	uint32_t i2s3_rx0;		/* AUDIO_I2S3_RX0,  offset 0x1C */
	uint32_t i2s4_rx0;		/* AUDIO_I2S4_RX0,  offset 0x20 */

	uint32_t dam0_rx0;		/* AUDIO_DAM0_RX0,  offset 0x24 */
	uint32_t dam0_rx1;		/* AUDIO_DAM0_RX1,  offset 0x28 */
	uint32_t dam1_rx0;		/* AUDIO_DAM1_RX0,  offset 0x2C */
	uint32_t dam1_rx1;		/* AUDIO_DAM1_RX1,  offset 0x30 */
	uint32_t dam2_rx0;		/* AUDIO_DAM2_RX0,  offset 0x34 */
	uint32_t dam2_rx1;		/* AUDIO_DAM2_RX1,  offset 0x38 */

	uint32_t spdif_rx0;		/* AUDIO_SPDIF_RX0, offset 0x3C */
	uint32_t spdif_rx1;		/* AUDIO_SPDIF_RX1, offset 0x40 */

	uint32_t apbif_rx4;		/* AUDIO_APBIF_RX4, offset 0x44 */
	uint32_t apbif_rx5;		/* AUDIO_APBIF_RX4, offset 0x48 */
	uint32_t apbif_rx6;		/* AUDIO_APBIF_RX4, offset 0x4C */
	uint32_t apbif_rx7;		/* AUDIO_APBIF_RX4, offset 0x50 */
	uint32_t apbif_rx8;		/* AUDIO_APBIF_RX4, offset 0x54 */
	uint32_t apbif_rx9;		/* AUDIO_APBIF_RX4, offset 0x58 */

	uint32_t amx0_rx0;		/* AUDIO_AMX0_RX0,  offset 0x5C */
	uint32_t amx0_rx1;		/* AUDIO_AMX0_RX1,  offset 0x60 */
	uint32_t amx0_rx2;		/* AUDIO_AMX0_RX2,  offset 0x64 */
	uint32_t amx0_rx3;		/* AUDIO_AMX0_RX3,  offset 0x68 */

	uint32_t adx0_rx0;		/* AUDIO_ADX0_RX0,  offset 0x6C */
} TegraXbarRegs;

typedef struct TegraApbifRegs {
	uint32_t channel0_ctrl;		/* APBIF_CHANNEL0_CTRL */
	uint32_t channel0_clr;		/* APBIF_CHANNEL0_CLEAR */
	uint32_t channel0_stat;		/* APBIF_CHANNEL0_STATUS */
	uint32_t channel0_txfifo;	/* APBIF_CHANNEL0_TXFIFO */
	uint32_t channel0_rxfifo;	/* APBIF_CHANNEL0_RXFIFO */
	uint32_t channel0_cif_tx0_ctrl;	/* APBIF_AUDIOCIF_TX0_CTRL */
	uint32_t channel0_cif_rx0_ctrl;	/* APBIF_AUDIOCIF_RX0_CTRL */
	uint32_t channel0_reserved0;	/* RESERVED, offset 0x1C */
	/* ahub_channel1_ctrl/clr/stat/txfifo/rxfifl/ciftx/cifrx ... here */
	/* ahub_channel2_ctrl/clr/stat/txfifo/rxfifl/ciftx/cifrx ... here */
	/* ahub_channel3_ctrl/clr/stat/txfifo/rxfifl/ciftx/cifrx ... here */
	uint32_t reserved123[3*8];
	uint32_t config_link_ctrl;	/* APBIF_CONFIG_LINK_CTRL_0, off 0x80 */
	uint32_t misc_ctrl;		/* APBIF_MISC_CTRL_0, offset 0x84 */
	uint32_t apbdma_live_stat;	/* APBIF_APBDMA_LIVE_STATUS_0 */
	uint32_t i2s_live_stat;		/* APBIF_I2S_LIVE_STATUS_0 */
	uint32_t dam0_live_stat;	/* APBIF_DAM0_LIVE_STATUS_0 */
	uint32_t dam1_live_stat;	/* APBIF_DAM0_LIVE_STATUS_0 */
	uint32_t dam2_live_stat;	/* APBIF_DAM0_LIVE_STATUS_0 */
	uint32_t spdif_live_stat;	/* APBIF_SPDIF_LIVE_STATUS_0 */
	uint32_t i2s_int_mask;		/* APBIF_I2S_INT_MASK_0, offset 0xB0 */
	uint32_t dam_int_mask;		/* APBIF_DAM_INT_MASK_0 */
	uint32_t reserved_int_mask;	/* RESERVED, offset 0xB8 */
	uint32_t spdif_int_mask;	/* APBIF_SPDIF_INT_MASK_0 */
	uint32_t apbif_int_mask;	/* APBIF_APBIF_INT_MASK_0, off 0xC0 */
	uint32_t reserved2_int_mask;	/* RESERVED, offset 0xC4 */
	uint32_t i2s_int_stat;		/* APBIF_I2S_INT_STATUS_0, offset 0xC8 */
	uint32_t dam_int_stat;		/* APBIF_DAM_INT_STATUS_0 */
	uint32_t reserved_int_stat;	/* RESERVED, offset 0xD0 */
	uint32_t spdif_int_stat;	/* APBIF_SPDIF_INT_STATUS_0 */
	uint32_t apbif_int_stat;	/* APBIF_APBIF_INT_STATUS_0 */
	uint32_t reserved2_int_stat;	/* RESERVED, offset 0xDC */
	uint32_t i2s_int_src;		/* APBIF_I2S_INT_SOURCE_0, offset 0xE0 */
	uint32_t dam_int_src;		/* APBIF_DAM_INT_SOURCE_0 */
	uint32_t reserved_int_src;	/* RESERVED, offset 0xE8 */
	uint32_t spdif_int_src;		/* APBIF_SPDIF_INT_SOURCE_0 */
	uint32_t apbif_int_src;		/* APBIF_APBIF_INT_SOURCE_0, off 0xF0 */
	uint32_t reserved2_int_src;	/* RESERVED, offset 0xF4 */
	uint32_t i2s_int_set;		/* APBIF_I2S_INT_SET_0, offset 0xF8 */
	uint32_t dam_int_set;		/* APBIF_DAM_INT_SET_0, offset 0xFC */
	uint32_t spdif_int_set;		/* APBIF_SPDIF_INT_SET_0, off 0x100 */
	uint32_t apbif_int_set;		/* APBIF_APBIF_INT_SET_0, off 0x104 */
} TegraApbifRegs;

// XBAR functions

static int tegra_ahub_xbar_enable_i2s(TegraAudioHubXbar *xbar, int i2s_id)
{
	// Enables I2S as the receiver of APBIF, by writing APBIF_TX0 (0x01) to
	// i2s?_rx0.
	switch (i2s_id) {
	case 0:
		writel(1, &xbar->regs->i2s0_rx0);
		break;
	case 1:
		writel(1, &xbar->regs->i2s1_rx0);
		break;
	case 2:
		writel(1, &xbar->regs->i2s2_rx0);
		break;
	case 3:
		writel(1, &xbar->regs->i2s3_rx0);
		break;
	case 4:
		writel(1, &xbar->regs->i2s4_rx0);
		break;
	default:
		printf("%s: Invalid I2S component id: %d\n", __func__, i2s_id);
		return 1;
	}
	return 0;
}

// APBIF FIFO functions

static size_t tegra_ahub_apbif_capacity(TxFifoOps *me)
{
	TegraAudioHubApbif *apbif = container_of(me, TegraAudioHubApbif, ops);
	// The TxFifoOps.capacity returns capacity in "bytes" while APBIF
	// measures capacity in "words".
	return apbif->capacity_in_word * sizeof(uint32_t);
}

static int tegra_ahub_apbif_is_full(TxFifoOps *me)
{
	TegraAudioHubApbif *apbif = container_of(me, TegraAudioHubApbif, ops);
	/* FIFO_FULL bit may not be set even when the buffer is full. You may
	 * consider using _is_empty() instead. */
	return readl(&apbif->regs->i2s_live_stat) & apbif->full_mask;
}

static int tegra_ahub_apbif_is_empty(TxFifoOps *me)
{
	TegraAudioHubApbif *apbif = container_of(me, TegraAudioHubApbif, ops);
	return readl(&apbif->regs->i2s_live_stat) & apbif->empty_mask;
}

static ssize_t tegra_ahub_apbif_send(TxFifoOps *me, const void *buf, size_t len)
{
	const uint32_t *data = (const uint32_t *)buf;
	ssize_t written = 0;
	TegraApbifRegs *regs = container_of(me, TegraAudioHubApbif, ops)->regs;
	if (len % sizeof(*data)) {
		printf("%s: Data size (%d) must be aligned to %d.\n", __func__,
		       len, sizeof(*data));
		return -1;
	}
	while (written < len) {
		while (!tegra_ahub_apbif_is_empty(me));
		writel(*data++, &regs->channel0_txfifo);
		written += sizeof(*data);
	}
	return written;
}

static void tegra_ahub_apbif_set_cif(TegraAudioHubApbif *apbif, uint32_t value)
{
	writel(value, &apbif->regs->channel0_cif_tx0_ctrl);
}

static void tegra_ahub_apbif_enable_channel0(TegraAudioHubApbif *apbif,
					    int fifo_threshold)
{
	uint32_t ctrl = (TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_EN |
			TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_16 |
			TEGRA_AHUB_CHANNEL_CTRL_TX_EN);
	fifo_threshold--; // fifo_threshold starts from 1.
	ctrl |= (fifo_threshold << TEGRA_AHUB_CHANNEL_CTRL_TX_THRESHOLD_SHIFT);
	writel(ctrl, &apbif->regs->channel0_ctrl);
}

static int tegra_ahub_apbif_set_fifo_mask(TegraAudioHubApbif *apbif,
					       int i2s_id)
{
	if (i2s_id > 4) {
		printf("%s: Invalid I2S component id: %d\n", __func__, i2s_id);
		return 1;
	}
	apbif->full_mask = TEGRA_AHUB_I2S_LIVE_STATUS_I2S0_TX_FIFO_FULL <<
			(2 * i2s_id);
	apbif->empty_mask = TEGRA_AHUB_I2S_LIVE_STATUS_I2S0_TX_FIFO_EMPTY <<
			(2 * i2s_id);
	return 0;
}

// Audio hub functions

static uint32_t tegra_ahub_get_cif(int is_receive, uint32_t channels,
				   uint32_t bits_per_sample,
				   uint32_t fifo_threshold)
{
	uint32_t audio_bits = (bits_per_sample >> 2) - 1;
	channels--;  // Channels in CIF starts from 1.
	fifo_threshold--;  // FIFO threshold starts from 1.
	// Assume input and output are always using same channel / bits.
	return channels << TEGRA_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT |
	       channels << TEGRA_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT |
	       audio_bits << TEGRA_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT |
	       audio_bits << TEGRA_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT |
	       fifo_threshold << TEGRA_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT |
	       (is_receive ? TEGRA_AUDIOCIF_DIRECTION_RX <<
			     TEGRA_AUDIOCIF_CTRL_DIRECTION_SHIFT : 0);
}

static int tegra_audio_hub_enable(SoundRouteComponentOps *me)
{
	TegraAudioHub *ahub = container_of(me, TegraAudioHub, component.ops);
	uint32_t cif_ctrl = 0;

	// FIFO is inactive until (fifo_threshold) of words are sent.
	// For better performance, we want to set it to half of capacity.
	uint32_t fifo_threshold = ahub->apbif->capacity_in_word / 2;

	// Setup audio client interface (ACIF): APBIF (channel0) as sender and
	// I2S as receiver.
	cif_ctrl = tegra_ahub_get_cif(1, ahub->i2s->channels,
				      ahub->i2s->bits_per_sample,
				      fifo_threshold);
	tegra_i2s_set_cif_tx_ctrl(ahub->i2s, cif_ctrl);

	cif_ctrl = tegra_ahub_get_cif(0, ahub->i2s->channels,
				      ahub->i2s->bits_per_sample,
				      fifo_threshold);
	tegra_ahub_apbif_set_cif(ahub->apbif, cif_ctrl);
	tegra_ahub_apbif_enable_channel0(ahub->apbif, fifo_threshold);

	if (tegra_ahub_apbif_set_fifo_mask(ahub->apbif, ahub->i2s->id) ||
	    tegra_ahub_xbar_enable_i2s(ahub->xbar, ahub->i2s->id))
		return 1;
	return 0;
}

// Constructors

TegraAudioHubXbar *new_tegra_audio_hub_xbar(uintptr_t regs)
{
	TegraAudioHubXbar *xbar = xzalloc(sizeof(*xbar));
	xbar->regs = (TegraXbarRegs *)regs;
	return xbar;
}

TegraAudioHubApbif *new_tegra_audio_hub_apbif(uintptr_t regs,
					      size_t capacity_in_word)
{
	TegraAudioHubApbif *apbif = xzalloc(sizeof(*apbif));
	apbif->ops.send = &tegra_ahub_apbif_send;
	apbif->ops.is_full = &tegra_ahub_apbif_is_full;
	apbif->ops.capacity = &tegra_ahub_apbif_capacity;
	apbif->regs = (TegraApbifRegs *)regs;
	apbif->capacity_in_word = capacity_in_word;
	return apbif;
}

TegraAudioHub *new_tegra_audio_hub(TegraAudioHubXbar *xbar,
				   TegraAudioHubApbif *apbif,
				   TegraI2s *i2s)
{
	TegraAudioHub *ahub = xzalloc(sizeof(*ahub));
	ahub->xbar = xbar;
	ahub->apbif = apbif;
	ahub->i2s = i2s;
	ahub->component.ops.enable = &tegra_audio_hub_enable;
	return ahub;
}
