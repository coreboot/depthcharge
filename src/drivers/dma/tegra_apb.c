/*
 * (C) Copyright 2010,2011
 * NVIDIA Corporation <www.nvidia.com>
 * Copyright 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libpayload.h>

#include "drivers/dma/tegra_apb.h"

static int tegra_apb_dma_busy(TegraApbDmaChannel *me)
{
	/*
	 * In continuous mode, the BSY_n bit in APB_DMA_STATUS and
	 * BSY in APBDMACHAN_CHANNEL_n_STA_0 will remain set as '1' so long
	 * as the channel is enabled. So for this function we'll use the
	 * DMA_ACTIVITY bit.
	 */
	return readl(&me->regs->sta) & APBDMACHAN_STA_DMA_ACTIVITY ? 1 : 0;
}

static TegraApbDmaChannel *tegra_apb_dma_claim(TegraApbDmaController *me)
{
	writel(readl(&me->regs->command) | APBDMA_COMMAND_GEN,
	       &me->regs->command);

	for (int i = 0; i < me->count; i++) {
		TegraApbDmaChannel *channel = &me->channels[i];
		if (channel->claimed)
			continue;

		channel->claimed = 1;
		return channel;
	}
	printf("%s: No DMA channels available.\n", __func__);
	return NULL;
}

static int tegra_apb_dma_release(TegraApbDmaController *me,
				 TegraApbDmaChannel *channel)
{
	if (!channel->claimed) {
		printf("%s: Channel wasn't claimed.\n", __func__);
		return -1;
	}

	while (channel->busy(channel))
	{;}

	channel->claimed = 0;
	return 0;
}

static int tegra_apb_dma_start(TegraApbDmaChannel *me)
{
	uint32_t csr = readl(&me->regs->csr);
	writel(csr | APBDMACHAN_CSR_ENB, &me->regs->csr);
	return 0;
}

static int tegra_apb_dma_finish(TegraApbDmaChannel *me)
{
	uint32_t csr = readl(&me->regs->csr);
	writel(csr | APBDMACHAN_CSR_HOLD, &me->regs->csr);
	writel(csr & ~APBDMACHAN_CSR_ENB, &me->regs->csr);
	return 0;
}

TegraApbDmaController *new_tegra_apb_dma(void *main, void *bases[], int count)
{
	TegraApbDmaController *controller = xzalloc(sizeof(*controller));

	controller->claim = &tegra_apb_dma_claim;
	controller->release = &tegra_apb_dma_release;
	controller->regs = main;

	controller->channels = xzalloc(count * sizeof(TegraApbDmaChannel));
	for (int i = 0; i < count; i++) {
		TegraApbDmaChannel *channel = &controller->channels[i];
		channel->start = &tegra_apb_dma_start;
		channel->finish = &tegra_apb_dma_finish;
		channel->busy = &tegra_apb_dma_busy;

		channel->regs = bases[i];
	}

	controller->count = count;

	return controller;
}
