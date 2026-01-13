// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <libpayload.h>

#include "mtk_dsi.h"

void mtk_dsi_stop(struct mtk_dsi *dsi)
{
	/*
	 * Only stop primary dsi0 for single or dual channel mode.
	 * The secondary dsi1 is dual-enabled and does not require a separate stop.
	 * Stopping only the primary dsi0 ensures correct synchronization
	 * with secondary dsi1 if there is.
	 */
	if (dsi->reg_base[0])
		write32p(dsi->reg_base[0] + DSI_START, 0);
}

static void mtk_dsi_poll_idle(uintptr_t base, int idx, uint64_t timeout_us)
{
	uint64_t start = timer_us(0);
	while (read32p(base + DSI_INTSTA) & DSI_BUSY) {
		if (timer_us(start) > timeout_us) {
			printf("DSI[%d] wait for idle timeout!\n", idx);
			break;
		}
	}
}

void mtk_dsi_wait_for_idle(struct mtk_dsi *dsi)
{
	const uint64_t timeout_us = 2 * USECS_PER_SEC;

	for (int i = 0; i < ARRAY_SIZE(dsi->reg_base); i++) {
		uintptr_t base = dsi->reg_base[i];
		if (!base)
			break;

		mtk_dsi_poll_idle(base, i, timeout_us);
	}
}
