/*
 * Copyright (C) 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/i2s/broadwell/broadwell-regs.h"
#include "drivers/bus/i2s/broadwell/broadwell.h"

static void init_shim_csr(BdwI2s *bus)
{
	uint32_t shim_cs = readl(&bus->shim->csr);

	/*
	 * Select SSP clock
	 * Turn off low power clock
	 * Set PIO mode
	 * Stall DSP core
	 */
	shim_cs &= ~(SHIM_CS_S0IOCS | SHIM_CS_LPCS | SHIM_CS_DCS_MASK);
	shim_cs |= SHIM_CS_S1IOCS |
		SHIM_CS_SBCS_SSP1_24MHZ |
		SHIM_CS_SBCS_SSP0_24MHZ |
		SHIM_CS_SDPM_PIO_SSP1 |
		SHIM_CS_SDPM_PIO_SSP0 |
		SHIM_CS_STALL |
		SHIM_CS_DCS_DSP32_AF32;

	writel(shim_cs, &bus->shim->csr);
}

static void init_shim_clkctl(BdwI2s *bus)
{
	uint32_t clkctl = readl(&bus->shim->clkctl);

	/* Set 24mhz mclk, prevent local clock gating, enable SSP0 clock */
	clkctl &= SHIM_CLKCTL_RESERVED;
	clkctl |= SHIM_CLKCTL_MCLK_24MHZ | SHIM_CLKCTL_DCPLCG;

	/* Enable requested SSP interface */
	if (bus->ssp)
		clkctl |= SHIM_CLKCTL_SCOE_SSP1 | SHIM_CLKCTL_SFLCGB_SSP1_CGD;
	else
		clkctl |= SHIM_CLKCTL_SCOE_SSP0 | SHIM_CLKCTL_SFLCGB_SSP0_CGD;

	writel(clkctl, &bus->shim->clkctl);
}

static void init_sscr0(BdwI2s *bus)
{
	uint32_t sscr0;

	/* Set data size based on BPS */
	if (bus->bits_per_sample > 16)
		sscr0 = SSP_SSC0_DSS(bus->bits_per_sample - 16) |
			SSP_SSC0_EDSS;
	else
		sscr0 = SSP_SSC0_DSS(bus->bits_per_sample);

	/* Set network mode, Stereo PSP frame format */
	sscr0 |= SSP_SSC0_MODE_NETWORK |
		SSP_SSC0_FRDC_STEREO |
		SSP_SSC0_FRF_PSP |
		SSP_SSC0_TIM |
		SSP_SSC0_RIM |
		SSP_SSC0_ECS_PCH |
		SSP_SSC0_NCS_PCH |
		SSP_SSC0_ACS_PCH;

	/* Scale 24MHz MCLK */
	sscr0 |= SSP_SSC0_SCR(bus->settings->sclk_rate);

	writel(sscr0, &bus->regs->sscr0);
}

static void init_sscr1(BdwI2s *bus)
{
	uint32_t sscr1 = readl(&bus->regs->sscr1);

	sscr1 &= SSP_SSC1_RESERVED;

	/* Set as I2S master */
	sscr1 |= SSP_SSC1_SCLKDIR_MASTER | SSP_SSC1_SCLKDIR_MASTER;

	/* Enable TXD tristate behavior for PCH */
	sscr1 |= SSP_SSC1_TTELP | SSP_SSC1_TTE;

	/* Disable DMA Tx/Rx service request */
	sscr1 |= SSP_SSC1_TSRE | SSP_SSC1_RSRE;

	/* Clock on during transfer */
	sscr1 |= SSP_SSC1_SCFR;

	/* Set FIFO thresholds */
	sscr1 |= SSP_SSC1_RFT(SSP_FIFO_SIZE);
	sscr1 |= SSP_SSC1_TFT(SSP_FIFO_SIZE);

	/* Disable interrupts */
	sscr1 &= ~(SSP_SSC1_EBCEI | SSP_SSC1_TINTE | SSP_SSC1_PINTE);
	sscr1 &= ~(SSP_SSC1_LBM | SSP_SSC1_RWOT);

	writel(sscr1, &bus->regs->sscr1);
}

static void init_sspsp(BdwI2s *bus)
{
	uint32_t sspsp = readl(&bus->regs->sspsp);

	sspsp &= SSP_PSP_RESERVED;
	sspsp |= SSP_PSP_SCMODE(bus->settings->sclk_mode);
	sspsp |= SSP_PSP_DMYSTOP(bus->settings->sclk_dummy_stop);
	sspsp |= SSP_PSP_SFRMWDTH(bus->settings->sclk_frame_width);

	/* Frame Sync Relative Timing */
	if (bus->settings->sfrm_relative_timing == NEXT_FRMS_AFTER_END_OF_T4)
		sspsp |= SSP_PSP_FSRT;
	else
		sspsp &= ~SSP_PSP_FSRT;

	/* Serial Frame Polarity */
	if (bus->settings->sfrm_polarity == SSP_FRMS_ACTIVE_HIGH)
		sspsp |= SSP_PSP_SFRMP;
	else
		sspsp &= ~SSP_PSP_SFRMP;

	/* End Data Transfer State */
	if (bus->settings->end_transfer_state == SSP_END_TRANSFER_STATE_LOW)
		sspsp &= ~SSP_PSP_ETDS;
	else
		sspsp |= SSP_PSP_ETDS;

	writel(sspsp, &bus->regs->sspsp);
}

static void init_ssp_time_slot(BdwI2s *bus)
{
	writel(SSP_SSTSA(3), &bus->regs->sstsa);
	writel(SSP_SSTSA(3), &bus->regs->ssrsa);
}

static int bdw_i2s_init(BdwI2s *bus)
{
	init_shim_csr(bus);
	init_shim_clkctl(bus);
	init_sscr0(bus);
	init_sscr1(bus);
	init_sspsp(bus);
	init_ssp_time_slot(bus);

	return 0;
}

static void bdw_i2s_enable(BdwI2s *bus)
{
	uint32_t value;

	value = readl(&bus->regs->sscr0);
	value |= SSP_SSC0_SSE;
	writel(value, &bus->regs->sscr0);

	value = readl(&bus->regs->sstsa);
	value |= SSP_SSTSA_EN;
	writel(value, &bus->regs->sstsa);
}

static void bdw_i2s_disable(BdwI2s *bus)
{
	uint32_t value;

	value = readl(&bus->regs->sstsa);
	value &= ~SSP_SSTSA_EN;
	writel(value, &bus->regs->sstsa);

	value = readl(&bus->regs->sscr0);
	value &= ~SSP_SSC0_SSE;
	writel(value, &bus->regs->sscr0);
}

static int bdw_i2s_send(I2sOps *me, unsigned int *data, unsigned int length)
{
	BdwI2s *bus = container_of(me, BdwI2s, ops);

	if (!bus->initialized) {
		if (bdw_i2s_init(bus))
			return -1;
		bus->initialized = 1;
	}

	if (length < SSP_FIFO_SIZE) {
		printf("I2S Invalid data size\n");
		return -1;
	}

	/* Enable I2S interface */
	bdw_i2s_enable(bus);

	/* Transfer data */
	while (length > 0) {
		uint64_t start = timer_us(0);

		/* Write data if transmit FIFO has room */
		if (readl(&bus->regs->sssr) & SSP_SSS_TNF) {
			writel(*data++, &bus->regs->ssdr);
			length--;
		} else {
			if (timer_us(start) > 100000) {
				/* Disable I2S interface */
				bdw_i2s_disable(bus);
				printf("I2S Transfer Timeout\n");
				return -1;
			}
		}
	}

	/* Disable I2S interface */
	bdw_i2s_disable(bus);

	return 0;
}

/*
 * new_bdw_i2s - Allocate new I2s data structures.
 *
 * @base: memory-mapped base address for controller
 * @bps: Bits per sample
 * @ssp: SSP Interface
 * @settings: I2S interface settings
 */
BdwI2s *new_bdw_i2s(uintptr_t base, int ssp, int bps,
	const BdwI2sSettings *settings)
{
	BdwI2s *bus = xzalloc(sizeof(*bus));

	bus->ssp = ssp;
	bus->ops.send = &bdw_i2s_send;
	bus->shim = (BdwI2sShimRegs *)(base + BDW_SHIM_START_ADDRESS);

	switch (ssp) {
	case 0:
		bus->regs = (BdwI2sSspRegs *)(base + BDW_SSP0_START_ADDRESS);
		break;
	case 1:
		bus->regs = (BdwI2sSspRegs *)(base + BDW_SSP1_START_ADDRESS);
		break;
	default:
		printf("new_bdw_i2s: Unknown SSP interface %d\n", ssp);
		return NULL;
	}

	bus->settings = settings;
	bus->bits_per_sample = bps;

	return bus;
}
