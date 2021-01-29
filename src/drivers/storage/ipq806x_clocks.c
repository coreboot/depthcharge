/*
 * Copyright (C) 2014 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <libpayload.h>
#include "drivers/storage/ipq806x_clocks.h"
#include "drivers/storage/ipq806x_mmc.h"

void clock_init_mmc(unsigned instance)
{
	/* Enable h/w gating */
	write32(SDCn_HCLK_CTL_REG(instance), 1 << 6);
}

static void clock_set_m_n_d(uint32_t instance, uint32_t m,
				uint32_t n, uint32_t d,
				uint32_t s, uint32_t pre)
{
	uint32_t reg;

	reg = 2 * d;
	reg = ~reg;
	reg &= 0xFF;

	reg |= (m << 16);
	write32(SDCn_APPS_CLK_MD_REG(instance), reg);

	reg = (((~(n-m)) << 16) & 0xFFFF0000)|1<<11|s;
	write32(SDCn_APPS_CLK_NS_REG(instance), reg);

	/* enable m n d counters. */
	reg |= (2 << 5) | (pre << 3);
	write32(SDCn_APPS_CLK_NS_REG(instance), reg);

	reg |= (1 << 8);
	write32(SDCn_APPS_CLK_NS_REG(instance), reg);

	/* enable clock */
	write32(SDCn_APPS_CLK_NS_REG(instance), reg | 1 << 9);
}

void clock_config_mmc(MmcCtrlr *ctrlr, unsigned freq)
{
	uint32_t m, n, d, s, pre;
	uint32_t reg;
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);
	uint32_t instance = mmc_host->instance;

	/*Disable the clk */
	reg = read32(SDCn_APPS_CLK_NS_REG(instance));
	reg &= ~(1<<9);
	write32(SDCn_APPS_CLK_NS_REG(instance), reg);

	switch (freq) {
	case 144000:
		m = 2;
		n = 125;
		d = 3;
		s = 0;
		pre = 2;
		break;

	case 400000:
		m = 1;
		n = 240;
		d = 4;
		s = 3;
		pre = 3;
		break;

	case 48000000:
	case 52000000:
		m = 1;
		n = 2;
		d = 1;
		s = 3;
		pre = 3;
		break;

	default:
		printf("MMC clock speed not supported %d\n", freq);
		return;
	}

	clock_set_m_n_d(instance, m, n, d, s, pre);

	mmc_boot_mci_clk_enable(ctrlr);
}
