/*
 * Copyright 2017 Google Inc. All rights reserved.
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

#include <assert.h>
#include <pci/pci.h>
#include <stdbool.h>

#include "base/container_of.h"
#include "drivers/bus/spi/intel_gspi.h"

/* GSPI Memory Mapped Registers */
#define SSCR0			0x0	/* SSP Control Register 0 */
#define   SSCR0_EDSS_0		(0 << 20)
#define   SSCR0_EDSS_1		(1 << 20)
#define   SSCR0_SCR_SHIFT	(8)
#define   SSCR0_SCR_MASK	(0xFFF)
#define   SSCR0_SSE_DISABLE	(0 << 7)
#define   SSCR0_SSE_ENABLE	(1 << 7)
#define   SSCR0_ECS_ON_CHIP	(0 << 6)
#define   SSCR0_FRF_MOTOROLA	(0 << 4)
#define   SSCR0_DSS_SHIFT	(0)
#define   SSCR0_DSS_MASK	(0xF)
#define SSCR1			0x4	/* SSP Control Register 1 */
#define   SSCR1_IFS_LOW	(0 << 16)
#define   SSCR1_IFS_HIGH	(1 << 16)
#define   SSCR1_SPH_FIRST	(0 << 4)
#define   SSCR1_SPH_SECOND	(1 << 4)
#define   SSCR1_SPO_LOW	(0 << 3)
#define   SSCR1_SPO_HIGH	(1 << 3)
#define SSSR			0x8	/* SSP Status Register */
#define   SSSR_TUR		(1 << 21)  /* Tx FIFO underrun */
#define   SSSR_TINT		(1 << 19)  /* Rx Time-out interrupt */
#define   SSSR_PINT		(1 << 18)  /* Peripheral trailing byte
					      interrupt */
#define   SSSR_ROR		(1 << 7)   /* Rx FIFO Overrun */
#define   SSSR_BSY		(1 << 4)   /* SSP Busy */
#define   SSSR_RNE		(1 << 3)   /* Receive FIFO not empty */
#define   SSSR_TNF		(1 << 2)   /* Transmit FIFO not full */
#define SSDR			0x10	/* SSP Data Register */
#define SSTO			0x28	/* SSP Time out */
#define SITF			0x44	/* SPI Transmit FIFO */
#define   SITF_LEVEL_SHIFT	(16)
#define   SITF_LEVEL_MASK	(0x3f)
#define   SITF_LWM_SHIFT	(8)
#define   SITF_LWM_MASK	(0x3f)
#define   SITF_LWM(x)		((((x) - 1) & SITF_LWM_MASK) << SITF_LWM_SHIFT)
#define   SITF_HWM_SHIFT	(0)
#define   SITF_HWM_MASK	(0x3f)
#define   SITF_HWM(x)		((((x) - 1) & SITF_HWM_MASK) << SITF_HWM_SHIFT)
#define SIRF			0x48	/* SPI Receive FIFO */
#define   SIRF_LEVEL_SHIFT	(8)
#define   SIRF_LEVEL_MASK	(0x3f)
#define   SIRF_WM_SHIFT	(0)
#define   SIRF_WM_MASK		(0x3f)
#define   SIRF_WM(x)		((((x) - 1) & SIRF_WM_MASK) << SIRF_WM_SHIFT)

/* GSPI Additional Registers */
#define CLOCKS			0x200	/* Clocks */
#define   CLOCKS_UPDATE	(1 << 31)
#define   CLOCKS_N_SHIFT	(16)
#define   CLOCKS_N_MASK	(0x7fff)
#define   CLOCKS_M_SHIFT	(1)
#define   CLOCKS_M_MASK	(0x7fff)
#define   CLOCKS_DISABLE	(0 << 0)
#define   CLOCKS_ENABLE	(1 << 0)
#define RESETS			0x204	/* Resets */
#define   DMA_RESET		(0 << 2)
#define   DMA_ACTIVE		(1 << 2)
#define   CTRLR_RESET		(0 << 0)
#define   CTRLR_ACTIVE		(3 << 0)
#define ACTIVELTR_VALUE	0x210	/* Active LTR */
#define IDLELTR_VALUE		0x214	/* Idle LTR Value */
#define TX_BIT_COUNT		0x218	/* Tx Bit Count */
#define RX_BIT_COUNT		0x21c	/* Rx Bit Count */
#define SSP_REG		0x220	/* SSP Reg */
#define   DMA_FINISH_DISABLE	(1 << 0)
#define SPI_CS_CONTROL		0x224	/* SPI CS Control */
#define   CS_0_POL_SHIFT	(12)
#define   CS_0_POL_MASK		(1 << CS_0_POL_SHIFT)
#define   CS_POL_LOW		(0)
#define   CS_POL_HIGH		(1)
#define   CS_0			(0 << 8)
#define   CS_STATE_SHIFT	(1)
#define   CS_STATE_MASK		(1 << CS_STATE_SHIFT)
#define   CS_V1_STATE_LOW	(0)
#define   CS_V1_STATE_HIGH	(1)
#define   CS_MODE_HW		(0 << 0)
#define   CS_MODE_SW		(1 << 0)

#define GSPI_DATA_BIT_LENGTH		(8)

typedef struct intel_gspi {
	SpiOps ops;
	uintptr_t mmio_base;
} IntelGspi;

typedef struct intel_gspi_xfer_params {
	uint8_t *in;
	uint32_t bytesin;
	const uint8_t *out;
	uint32_t bytesout;
} IntelGspiXferParams;

static uint32_t gspi_read_mmio_reg(const IntelGspi *dev, uint32_t offset)
{
	assert(dev->mmio_base != 0);
	return read32((void *)(dev->mmio_base + offset));
}

static void gspi_write_mmio_reg(const IntelGspi *dev, uint32_t offset,
				uint32_t value)
{
	assert(dev->mmio_base != 0);
	write32((void *)(dev->mmio_base + offset), value);
}

enum cs_assert {
	CS_ASSERT,
	CS_DEASSERT,
};

/*
 * SPI_CS_CONTROL bit definitions based on GSPI_VERSION_x:
 *
 * VERSION_2 (CNL GSPI controller):
 * Polarity: Indicates inactive polarity of chip-select
 * State   : Indicates assert/de-assert of chip-select
 *
 * Default (SKL/KBL GSPI controller):
 * Polarity: Indicates active polarity of chip-select
 * State   : Indicates low/high output state of chip-select
 */
static uint32_t gspi_csctrl_state_v2(uint32_t pol, enum cs_assert cs_assert)
{
	return cs_assert;
}

static uint32_t gspi_csctrl_state_v1(uint32_t pol, enum cs_assert cs_assert)
{
	uint32_t state;

	if (pol == CS_POL_HIGH)
		state = (cs_assert == CS_ASSERT) ? CS_V1_STATE_HIGH :
			CS_V1_STATE_LOW;
	else
		state = (cs_assert == CS_ASSERT) ? CS_V1_STATE_LOW :
			CS_V1_STATE_HIGH;

	return state;
}

static uint32_t gspi_csctrl_state(uint32_t pol, enum cs_assert cs_assert)
{
	if (IS_ENABLED(CONFIG_DRIVER_BUS_SPI_INTEL_GSPI_VERSION_2))
		return gspi_csctrl_state_v2(pol, cs_assert);

	return gspi_csctrl_state_v1(pol, cs_assert);
}

static uint32_t gspi_csctrl_polarity_v2(enum spi_polarity active_pol)
{
	/* Polarity field indicates cs inactive polarity */
	if (active_pol == SPI_POLARITY_LOW)
		return CS_POL_HIGH;

	return CS_POL_LOW;
}

static uint32_t gspi_csctrl_polarity_v1(enum spi_polarity active_pol)
{
	/* Polarity field indicates cs active polarity */
	if (active_pol == SPI_POLARITY_LOW)
		return CS_POL_LOW;

	return CS_POL_HIGH;
}

static uint32_t gspi_csctrl_polarity(enum spi_polarity active_pol)
{
	if (IS_ENABLED(CONFIG_DRIVER_BUS_SPI_INTEL_GSPI_VERSION_2))
		return gspi_csctrl_polarity_v2(active_pol);

	return gspi_csctrl_polarity_v1(active_pol);
}

static void gspi_cs_change(const IntelGspi *dev, enum cs_assert cs_assert)
{
	uint32_t cs_ctrl, pol;
	cs_ctrl = gspi_read_mmio_reg(dev, SPI_CS_CONTROL);

	cs_ctrl &= ~CS_STATE_MASK;

	pol = !!(cs_ctrl & CS_0_POL_MASK);
	cs_ctrl |= gspi_csctrl_state(pol, cs_assert) << CS_STATE_SHIFT;

	gspi_write_mmio_reg(dev, SPI_CS_CONTROL, cs_ctrl);
}

#define DIV_ROUND_UP(x, y)  (((x) + (y) - 1) / (y))

static uint32_t gspi_get_clk_div(const IntelGspiSetupParams *p)
{
	assert (p->ref_clk_mhz != 0);
	assert (p->gspi_clk_mhz != 0);

	return (DIV_ROUND_UP(p->ref_clk_mhz, p->gspi_clk_mhz) - 1) &
		SSCR0_SCR_MASK;
}

static void gspi_setup(const IntelGspi *dev, const IntelGspiSetupParams *p)
{
	uint32_t cs_ctrl, sscr0, sscr1, clocks, sitf, sirf, pol;

	/* Take controller out of reset, keeping DMA in reset. */
	gspi_write_mmio_reg(dev, RESETS, CTRLR_ACTIVE | DMA_RESET);

	/* De-assert chip select. */
	gspi_cs_change(dev, CS_DEASSERT);

	/*
	 * CS control:
	 * - Set SW mode.
	 * - Set chip select to 0.
	 * - Set polarity based on device configuration.
	 * - Do not assert CS.
	 */
	cs_ctrl = CS_MODE_SW | CS_0;
	pol = gspi_csctrl_polarity(p->cs_polarity);
	cs_ctrl |= pol << CS_0_POL_SHIFT;
	cs_ctrl |= gspi_csctrl_state(pol, CS_DEASSERT);
	gspi_write_mmio_reg(dev, SPI_CS_CONTROL, cs_ctrl);

	/* Disable SPI controller. */
	gspi_write_mmio_reg(dev, SSCR0, SSCR0_SSE_DISABLE);

	/*
	 * SSCR0 configuration:
	 * clk_div - Based on reference clock and expected clock frequency.
	 * data bit length - assumed to be 8, hence EDSS = 0.
	 * ECS - Use on-chip clock
	 * FRF - Frame format set to Motorola SPI
	 */
	sscr0 = gspi_get_clk_div(p) << SSCR0_SCR_SHIFT;
	sscr0 |= ((GSPI_DATA_BIT_LENGTH - 1) << SSCR0_DSS_SHIFT) | SSCR0_EDSS_0;
	sscr0 |= SSCR0_ECS_ON_CHIP | SSCR0_FRF_MOTOROLA;
	gspi_write_mmio_reg(dev, SSCR0, sscr0);

	/*
	 * SSCR1 configuration:
	 * - Chip select polarity
	 * - Clock phase setting
	 * - Clock polarity
	 */
	sscr1 = (p->cs_polarity == SPI_POLARITY_LOW) ? SSCR1_IFS_LOW :
		SSCR1_IFS_HIGH;
	sscr1 |= (p->clk_phase == SPI_CLOCK_PHASE_FIRST) ? SSCR1_SPH_FIRST :
		SSCR1_SPH_SECOND;
	sscr1 |= (p->clk_polarity == SPI_POLARITY_LOW) ? SSCR1_SPO_LOW :
		SSCR1_SPO_HIGH;
	gspi_write_mmio_reg(dev, SSCR1, sscr1);

	/*
	 * Program m/n divider.
	 * Set m and n to 1, so that this divider acts as a pass-through.
	 */
	clocks = (1 << CLOCKS_N_SHIFT) | (1 << CLOCKS_M_SHIFT) | CLOCKS_ENABLE;
	gspi_write_mmio_reg(dev, CLOCKS, clocks);
	udelay(10);

	/*
	 * Tx FIFO Threshold.
	 * Low watermark threshold = 1
	 * High watermark threshold = 1
	 */
	sitf = SITF_LWM(1) | SITF_HWM(1);
	gspi_write_mmio_reg(dev, SITF, sitf);

	/* Rx FIFO Threshold (set to 1). */
	sirf = SIRF_WM(1);
	gspi_write_mmio_reg(dev, SIRF, sirf);

	/* Enable GSPI controller. */
	sscr0 |= SSCR0_SSE_ENABLE;
	gspi_write_mmio_reg(dev, SSCR0, sscr0);
}

static int gspi_cs_assert(SpiOps *me)
{
	const IntelGspi *dev = container_of(me, IntelGspi, ops);
	gspi_cs_change(dev, CS_ASSERT);
	return 0;
}

static int gspi_cs_deassert(SpiOps *me)
{
	const IntelGspi *dev = container_of(me, IntelGspi, ops);
	gspi_cs_change(dev, CS_DEASSERT);
	return 0;
}

static uint32_t gspi_read_status(const IntelGspi *dev)
{
	return gspi_read_mmio_reg(dev, SSSR);
}

static void gspi_clear_status(const IntelGspi *dev)
{
	const uint32_t sssr = SSSR_TUR | SSSR_TINT | SSSR_PINT | SSSR_ROR;
	gspi_write_mmio_reg(dev, SSSR, sssr);
}

static bool gspi_rx_fifo_empty(const IntelGspi *dev)
{
	return !(gspi_read_status(dev) & SSSR_RNE);
}

static bool gspi_tx_fifo_full(const IntelGspi *dev)
{
	return !(gspi_read_status(dev) & SSSR_TNF);
}

static bool gspi_rx_fifo_overrun(const IntelGspi *dev)
{
	if (gspi_read_status(dev) & SSSR_ROR) {
		printf("%s:GSPI receive FIFO overrun!", __func__);
		return true;
	}

	return false;
}

/* Read SSDR and return lowest byte. */
static uint8_t gspi_read_byte(const IntelGspi *dev)
{
	return gspi_read_mmio_reg(dev, SSDR) & 0xFF;
}

/* Write 32-bit word with "data" in lowest byte to SSDR. */
static void gspi_write_byte(const IntelGspi *dev, uint8_t data)
{
	return gspi_write_mmio_reg(dev, SSDR, data);
}

static void gspi_read_data(const IntelGspi *dev, IntelGspiXferParams *p)
{
	*(p->in) = gspi_read_byte(dev);
	p->in++;
	p->bytesin--;
}

static void gspi_write_data(const IntelGspi *dev, IntelGspiXferParams *p)
{
	gspi_write_byte(dev, *(p->out));
	p->out++;
	p->bytesout--;
}

static void gspi_read_dummy(const IntelGspi *dev, IntelGspiXferParams *p)
{
	gspi_read_byte(dev);
	p->bytesin--;
}

static void gspi_write_dummy(const IntelGspi *dev, IntelGspiXferParams *p)
{
	gspi_write_byte(dev, 0);
	p->bytesout--;
}

static int gspi_ctrlr_flush(const IntelGspi *dev)
{
	const uint64_t timeout_us = 500000;
	uint64_t start_timer = timer_us(0);

	while (!gspi_rx_fifo_empty(dev)) {
		if (timer_us(start_timer) > timeout_us) {
			printf("%s: Rx FIFO not empty after 500ms!", __func__);
			return -1;
		}

		gspi_read_byte(dev);
	}

	return 0;
}

static int __gspi_xfer(const IntelGspi *dev, IntelGspiXferParams *p)
{
	typedef void (*xfer_fn)(const IntelGspi *dev, IntelGspiXferParams *p);

	/*
	 * If in is non-NULL, then use gspi_read_data to perform byte-by-byte
	 * read of data from SSDR and save it to "in" buffer. Else discard the
	 * read data using gspi_read_dummy.
	 */
	xfer_fn fn_read = gspi_read_data;

	/*
	 * If out is non-NULL, then use gspi_write_data to perform byte-by-byte
	 * write of data from "out" buffer to SSDR. Else, use gspi_write_dummy
	 * to write dummy "0" data to SSDR in order to trigger read from slave.
	 */
	xfer_fn fn_write = gspi_write_data;

	if (!p->in)
		fn_read = gspi_read_dummy;

	if (!p->out)
		fn_write = gspi_write_dummy;

	while (p->bytesout || p->bytesin) {
		if (p->bytesout && !gspi_tx_fifo_full(dev))
			fn_write(dev, p);
		if (p->bytesin && !gspi_rx_fifo_empty(dev)) {
			if (gspi_rx_fifo_overrun(dev))
				return -1;
			fn_read(dev, p);
		}
	}

	return 0;
}

static int gspi_xfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	const IntelGspi *dev = container_of(me, IntelGspi, ops);
	IntelGspiXferParams params = {
		.in = in,
		.bytesin = size,
		.out = out,
		.bytesout = size,
	};

	/* Sanity-check to ensure both in and out are not NULL. */
	if (!in && !out) {
		printf("%s: ERROR! Both in and out can't be NULL!\n", __func__);
		return -1;
	}

	/* Flush out any stale data in Rx FIFO. */
	if (gspi_ctrlr_flush(dev))
		return -1;

	/* Clear status bits. */
	gspi_clear_status(dev);

	return __gspi_xfer(dev, &params);
}

SpiOps *new_intel_gspi(const IntelGspiSetupParams *p)
{
	IntelGspi *gspi = xzalloc(sizeof(*gspi));

	gspi->mmio_base = ALIGN_DOWN(pci_read_config32(p->dev,
						       PCI_BASE_ADDRESS_0), 16);
	if ((gspi->mmio_base == 0) || (gspi->mmio_base == ~(uintptr_t)0)) {
		printf("%s: GSPI base address not set or not accessible.\n",
			__func__);
		free(gspi);
		return NULL;
	}

	gspi_setup(gspi, p);
	gspi->ops.start = &gspi_cs_assert;
	gspi->ops.stop = &gspi_cs_deassert;
	gspi->ops.transfer = &gspi_xfer;

	return &gspi->ops;
}
