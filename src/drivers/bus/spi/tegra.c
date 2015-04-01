/*
 * NVIDIA Tegra SPI controller (T114 and later)
 *
 * Copyright (c) 2010-2013 NVIDIA Corporation
 * Copyright (C) 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <arch/cache.h>
#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/spi/spi.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/dma/tegra_apb.h"

/*
 * 64 packets in FIFO mode, BLOCK_SIZE packets in DMA mode. Packets can vary
 * in size from 4 to 32 bits. To keep things simple we'll use 8-bit packets.
 */
enum {
	SPI_PACKET_LOG_SIZE_BYTES = 2,
	SPI_PACKET_SIZE_BYTES = (1 << SPI_PACKET_LOG_SIZE_BYTES),
	SPI_MAX_XFER_BYTES_DMA = 65535 * SPI_PACKET_SIZE_BYTES,
	SPI_MAX_XFER_BYTES_FIFO = 64
};

typedef struct {
	uint32_t command1;	// 0x000: SPI_COMMAND1
	uint32_t command2;	// 0x004: SPI_COMMAND2
	uint32_t timing1;	// 0x008: SPI_CS_TIM1
	uint32_t timing2;	// 0x00c: SPI_CS_TIM2
	uint32_t trans_status;	// 0x010: SPI_TRANS_STATUS
	uint32_t fifo_status;	// 0x014: SPI_FIFO_STATUS
	uint32_t tx_data;	// 0x018: SPI_TX_DATA
	uint32_t rx_data;	// 0x01c: SPI_RX_DATA
	uint32_t dma_ctl;	// 0x020: SPI_DMA_CTL
	uint32_t dma_blk;	// 0x024: SPI_DMA_BLK
	uint8_t _rsv0[0xe0];	// 0x028-0x107: reserved
	uint32_t tx_fifo;	// 0x108: SPI_FIFO1
	uint8_t _rsv2[0x7c];	// 0x10c-0x187 reserved
	uint32_t rx_fifo;	// 0x188: SPI_FIFO2
	uint32_t spare_ctl;	// 0x18c: SPI_SPARE_CTRL
} __attribute__((packed)) TegraSpiRegs;

// COMMAND1
enum {
	SPI_CMD1_GO = 1 << 31,
	SPI_CMD1_M_S = 1 << 30,
	SPI_CMD1_MODE_SHIFT = 28,
	SPI_CMD1_MODE_MASK = 0x3 << SPI_CMD1_MODE_SHIFT,
	SPI_CMD1_CS_SEL_SHIFT = 26,
	SPI_CMD1_CS_SEL_MASK = 0x3 << SPI_CMD1_CS_SEL_SHIFT,
	SPI_CMD1_CS_POL_INACTIVE3 = 1 << 25,
	SPI_CMD1_CS_POL_INACTIVE2 = 1 << 24,
	SPI_CMD1_CS_POL_INACTIVE1 = 1 << 23,
	SPI_CMD1_CS_POL_INACTIVE0 = 1 << 22,
	SPI_CMD1_CS_SW_HW = 1 << 21,
	SPI_CMD1_CS_SW_VAL = 1 << 20,
	SPI_CMD1_IDLE_SDA_SHIFT = 18,
	SPI_CMD1_IDLE_SDA_MASK = 0x3 << SPI_CMD1_IDLE_SDA_SHIFT,
	SPI_CMD1_BIDIR = 1 << 17,
	SPI_CMD1_LSBI_FE = 1 << 16,
	SPI_CMD1_LSBY_FE = 1 << 15,
	SPI_CMD1_BOTH_EN_BIT = 1 << 14,
	SPI_CMD1_BOTH_EN_BYTE = 1 << 13,
	SPI_CMD1_RX_EN = 1 << 12,
	SPI_CMD1_TX_EN = 1 << 11,
	SPI_CMD1_PACKED = 1 << 5,
	SPI_CMD1_BIT_LEN_SHIFT = 0,
	SPI_CMD1_BIT_LEN_MASK = 0x1f << SPI_CMD1_BIT_LEN_SHIFT
};

// SPI_TRANS_STATUS
enum {
	SPI_STATUS_RDY = 1 << 30,
	SPI_STATUS_SLV_IDLE_COUNT_SHIFT = 16,
	SPI_STATUS_SLV_IDLE_COUNT_MASK =
		0xff << SPI_STATUS_SLV_IDLE_COUNT_SHIFT,
	SPI_STATUS_BLOCK_COUNT_SHIFT = 0,
	SPI_STATUS_BLOCK_COUNT = 0xffff << SPI_STATUS_BLOCK_COUNT_SHIFT
};

// SPI_FIFO_STATUS
enum {
	SPI_FIFO_STATUS_CS_INACTIVE = 1 << 31,
	SPI_FIFO_STATUS_FRAME_END = 1 << 30,
	SPI_FIFO_STATUS_RX_FIFO_FULL_COUNT_SHIFT = 23,
	SPI_FIFO_STATUS_RX_FIFO_FULL_COUNT_MASK =
		0x7f << SPI_FIFO_STATUS_RX_FIFO_FULL_COUNT_SHIFT,
	SPI_FIFO_STATUS_TX_FIFO_EMPTY_COUNT_SHIFT = 16,
	SPI_FIFO_STATUS_TX_FIFO_EMPTY_COUNT_MASK =
		0x7f << SPI_FIFO_STATUS_TX_FIFO_EMPTY_COUNT_SHIFT,
	SPI_FIFO_STATUS_RX_FIFO_FLUSH = 1 << 15,
	SPI_FIFO_STATUS_TX_FIFO_FLUSH = 1 << 14,
	SPI_FIFO_STATUS_ERR = 1 << 8,
	SPI_FIFO_STATUS_TX_FIFO_OVF = 1 << 7,
	SPI_FIFO_STATUS_TX_FIFO_UNR = 1 << 6,
	SPI_FIFO_STATUS_RX_FIFO_OVF = 1 << 5,
	SPI_FIFO_STATUS_RX_FIFO_UNR = 1 << 4,
	SPI_FIFO_STATUS_TX_FIFO_FULL = 1 << 3,
	SPI_FIFO_STATUS_TX_FIFO_EMPTY = 1 << 2,
	SPI_FIFO_STATUS_RX_FIFO_FULL = 1 << 1,
	SPI_FIFO_STATUS_RX_FIFO_EMPTY = 1 << 0
};

// SPI_DMA_CTL
enum {
	SPI_DMA_CTL_DMA = 1 << 31,
	SPI_DMA_CTL_CONT = 1 << 30,
	SPI_DMA_CTL_IE_RX = 1 << 29,
	SPI_DMA_CTL_IE_TX = 1 << 28,
	SPI_DMA_CTL_RX_TRIG_SHIFT = 19,
	SPI_DMA_CTL_RX_TRIG_MASK = 0x3 << SPI_DMA_CTL_RX_TRIG_SHIFT,
	SPI_DMA_CTL_TX_TRIG_SHIFT = 15,
	SPI_DMA_CTL_TX_TRIG_MASK = 0x3 << SPI_DMA_CTL_TX_TRIG_SHIFT
};

static void flush_fifos(TegraSpiRegs *regs)
{
	const uint32_t flush_mask = SPI_FIFO_STATUS_RX_FIFO_FLUSH |
				    SPI_FIFO_STATUS_TX_FIFO_FLUSH;
	uint32_t status = readl(&regs->fifo_status);

	status |= flush_mask;
	writel(status, &regs->fifo_status);

	while (status & flush_mask)
		status = readl(&regs->fifo_status);
}

static int tegra_spi_init(TegraSpi *bus)
{
	TegraSpiRegs *regs = bus->reg_addr;

	uint32_t command1 = readl(&regs->command1);
	// Software drives chip-select, set value to high.
	command1 |= SPI_CMD1_CS_SW_HW | SPI_CMD1_CS_SW_VAL;

	// 8-bit transfers, unpacked mode, most significant bit first.
	command1 &= ~(SPI_CMD1_BIT_LEN_MASK | SPI_CMD1_PACKED);
	command1 |= (7 << SPI_CMD1_BIT_LEN_SHIFT);

	writel(command1, &regs->command1);

	flush_fifos(regs);

	bus->initialized = 1;
	return 0;
}

static int tegra_spi_start(SpiOps *me)
{
	TegraSpi *bus = container_of(me, TegraSpi, ops);
	TegraSpiRegs *regs = bus->reg_addr;

	if (!bus->initialized && tegra_spi_init(bus))
		return -1;

	if (bus->started) {
		printf("%s: Bus already started.\n", __func__);
		return -1;
	}

	// Force chip select 0 for now.
	int cs = 0;

	uint32_t command1 = readl(&regs->command1);

	// Select appropriate chip-select line.
	command1 &= ~SPI_CMD1_CS_SEL_MASK;
	command1 |= (cs << SPI_CMD1_CS_SEL_SHIFT);

	// Drive chip-select low.
	command1 &= ~SPI_CMD1_CS_SW_VAL;

	writel(command1, &regs->command1);

	bus->started = 1;

	return 0;
}

static void clear_fifo_status(TegraSpiRegs *regs)
{
	uint32_t status = readl(&regs->fifo_status);
	status &= ~(SPI_FIFO_STATUS_ERR |
		    SPI_FIFO_STATUS_TX_FIFO_OVF |
		    SPI_FIFO_STATUS_TX_FIFO_UNR |
		    SPI_FIFO_STATUS_RX_FIFO_OVF |
		    SPI_FIFO_STATUS_RX_FIFO_UNR);
	writel(status, &regs->fifo_status);
}

static int tegra_spi_dma_config(TegraApbDmaChannel *dma, void *ahb_ptr,
				void *apb_ptr, uint32_t size, int to_apb,
				uint32_t slave_id)
{
	TegraApbDmaRegs *regs = dma->regs;

	uint32_t apb_seq = readl(&regs->apb_seq);
	uint32_t ahb_seq = readl(&regs->ahb_seq);
	uint32_t csr = readl(&regs->csr);

	// Set APB bus width, address wrap for each word.
	uint32_t new_apb_seq = apb_seq;
	new_apb_seq &= ~APBDMACHAN_APB_SEQ_APB_BUS_WIDTH_MASK;
	new_apb_seq |= SPI_PACKET_LOG_SIZE_BYTES <<
		       APBDMACHAN_APB_SEQ_APB_BUS_WIDTH_SHIFT;

	// AHB 1 word burst, bus width = 32 bits (fixed in hardware),
	// no address wrapping.
	uint32_t new_ahb_seq = ahb_seq;
	new_ahb_seq &= ~APBDMACHAN_AHB_SEQ_AHB_BURST_MASK;
	new_ahb_seq &= ~APBDMACHAN_AHB_SEQ_WRAP_MASK;
	new_ahb_seq |= (0x4 << APBDMACHAN_AHB_SEQ_AHB_BURST_SHIFT);

	// Set ONCE mode to transfer one "block" at a time (64KB).
	uint32_t new_csr = csr | APBDMACHAN_CSR_ONCE;
	// Set DMA direction for AHB = DRAM, APB = SPI.
	if (to_apb)
		new_csr |= APBDMACHAN_CSR_DIR;
	else
		new_csr &= ~APBDMACHAN_CSR_DIR;

	// Set up flow control.
	new_csr &= ~APBDMACHAN_CSR_REQ_SEL_MASK;
	new_csr |= slave_id << APBDMACHAN_CSR_REQ_SEL_SHIFT;
	new_csr |= APBDMACHAN_CSR_FLOW;

	if (apb_seq != new_apb_seq)
		writel(new_apb_seq, &regs->apb_seq);
	if (ahb_seq != new_ahb_seq)
		writel(new_ahb_seq, &regs->ahb_seq);
	if (csr != new_csr)
		writel(new_csr, &regs->csr);

	writel((uintptr_t)ahb_ptr, &regs->ahb_ptr);
	writel((uintptr_t)apb_ptr, &regs->apb_ptr);

	writel(size - 1, &regs->wcount);

	return 0;
}

static void wait_for_transfer(TegraSpiRegs *regs, uint32_t packets)
{
	while ((readl(&regs->trans_status) & SPI_STATUS_RDY) != SPI_STATUS_RDY);
}

static int tegra_spi_dma_transfer(TegraSpi *bus, void *in, const void *out,
				  uint32_t size)
{
	TegraSpiRegs *regs = bus->reg_addr;

	if (!size)
		return 0;

	flush_fifos(regs);

	TegraApbDmaChannel *cin = NULL;
	TegraApbDmaChannel *cout = NULL;

	uint32_t command1 = readl(&regs->command1);

	// Set transfer width.
	command1 &= ~SPI_CMD1_BIT_LEN_MASK;
	command1 |= ((SPI_PACKET_SIZE_BYTES * 8 - 1) << SPI_CMD1_BIT_LEN_SHIFT);
	writel(command1, &regs->command1);

	// Specify BLOCK_SIZE in SPI_DMA_BLK.
	writel((size >> SPI_PACKET_LOG_SIZE_BYTES) - 1, &regs->dma_blk);

	// Write to SPI_TRANS_STATUS RDY bit to clear it.
	uint32_t trans_status = readl(&regs->trans_status);
	writel(trans_status | SPI_STATUS_RDY, &regs->trans_status);

	if (out) {
		assert(((SPI_PACKET_SIZE_BYTES - 1) & (uintptr_t)out) == 0);
		cout = bus->dma_controller->claim(bus->dma_controller);
		if (!cout || tegra_spi_dma_config(cout, (void *)out,
						  &regs->tx_fifo, size, 1,
						  bus->dma_slave_id))
			return -1;

		command1 |= SPI_CMD1_TX_EN;
		writel(command1, &regs->command1);
	}
	if (in) {
		assert(((SPI_PACKET_SIZE_BYTES - 1) & (uintptr_t)in) == 0);
		cin = bus->dma_controller->claim(bus->dma_controller);
		if (!cin || tegra_spi_dma_config(cin, in, &regs->rx_fifo,
						 size, 0, bus->dma_slave_id)) {
			cout->finish(cout);
			bus->dma_controller->release(bus->dma_controller, cout);
			return -1;
		}

		command1 |= SPI_CMD1_RX_EN;
		writel(command1, &regs->command1);
	}

	if (cout)
		cout->start(cout);

	// Set DMA bit in SPI_DMA_CTL to start.
	uint32_t dma_ctl = readl(&regs->dma_ctl) | SPI_DMA_CTL_DMA;
	dma_ctl &= ~(SPI_DMA_CTL_RX_TRIG_MASK | SPI_DMA_CTL_TX_TRIG_MASK);
	dma_ctl |= (0 << SPI_DMA_CTL_RX_TRIG_SHIFT);
	dma_ctl |= (0 << SPI_DMA_CTL_TX_TRIG_SHIFT);
	writel(dma_ctl, &regs->dma_ctl);

	if (cin)
		cin->start(cin);

	wait_for_transfer(regs, size >> SPI_PACKET_LOG_SIZE_BYTES);

	if (cout) {
		cout->finish(cout);
		bus->dma_controller->release(bus->dma_controller, cout);
	}
	if (cin) {
		cin->finish(cin);
		bus->dma_controller->release(bus->dma_controller, cin);
	}

	command1 &= ~(SPI_CMD1_TX_EN | SPI_CMD1_RX_EN);
	writel(command1, &regs->command1);

	uint32_t status = readl(&regs->fifo_status);
	if (status & SPI_FIFO_STATUS_ERR) {
		printf("%s: Error in fifo status %#x.\n", __func__, status);
		clear_fifo_status(regs);
		return -1;
	}

	return 0;
}

static int tegra_spi_pio_transfer(TegraSpi *bus, uint8_t *in,
				  const uint8_t *out, uint32_t size)
{
	TegraSpiRegs *regs = bus->reg_addr;

	if (!size)
		return 0;

	flush_fifos(regs);

	uint32_t command1 = readl(&regs->command1);

	// Set transfer width.
	command1 &= ~SPI_CMD1_BIT_LEN_MASK;
	command1 |= (7 << SPI_CMD1_BIT_LEN_SHIFT);
	writel(command1, &regs->command1);

	// Specify BLOCK_SIZE in SPI_DMA_BLK.
	writel(size - 1, &regs->dma_blk);

	// Write to SPI_TRANS_STATUS RDY bit to clear it.
	uint32_t trans_status = readl(&regs->trans_status);
	writel(trans_status | SPI_STATUS_RDY, &regs->trans_status);

	if (out)
		command1 |= SPI_CMD1_TX_EN;
	if (in)
		command1 |= SPI_CMD1_RX_EN;
	writel(command1, &regs->command1);

	uint32_t out_bytes = out ? size : 0;
	while (out_bytes) {
		uint32_t data = *out++;
		writel(data, &regs->tx_fifo);
		out_bytes--;
	}

	/*
	 * Need to stabilize other reg bit before GO bit set.
	 *
	 * From IAS:
	 * For successful operation at various freq combinations, min of 4-5
	 * spi_clk cycle delay might be required before enabling PIO or DMA bit.
	 * This is needed to overcome the MCP between core and pad_macro.
	 * The worst case delay calculation can be done considering slowest
	 * qspi_clk as 1 MHz. based on that 1 us delay should be enough before
	 * enabling pio or dma.
	 */
	udelay(2);

	writel(command1 | SPI_CMD1_GO, &regs->command1);

	/* Need to wait a few cycles before command1 register is read */
	udelay(1);

	// Make sure the write to command1 completes.
	readl(&regs->command1);
	wait_for_transfer(regs, size);

	command1 &= ~(SPI_CMD1_TX_EN | SPI_CMD1_RX_EN);
	writel(command1, &regs->command1);

	uint32_t status = readl(&regs->fifo_status);
	if (status & SPI_FIFO_STATUS_ERR) {
		printf("%s: Error in fifo status %#x.\n", __func__, status);
		clear_fifo_status(regs);
		return -1;
	}

	uint32_t in_bytes = in ? size : 0;
	while (in_bytes) {
		uint32_t data = readl(&regs->rx_fifo);
		*in++ = data;
		in_bytes--;
	}

	return 0;
}

static int tegra_spi_transfer(SpiOps *me, void *in, const void *out,
			      uint32_t req_size)
{
	uintptr_t start;
	TegraSpi *bus = container_of(me, TegraSpi, ops);
	const size_t line_size = dcache_line_bytes();
	size_t size = req_size;

	/* Can only handly one direction at a time. */
	assert(!(in != NULL && out != NULL));
	/*
	 * DMA transfers are done only on cacheline aligned pointers, however
	 * the DMA engine expects pointers to be aligned to
	 * SPI_PACKET_SIZE_BYTES. Therefore, validate our assumptions.
	 */
	assert(line_size >= SPI_PACKET_SIZE_BYTES);

	start = in ? (uintptr_t)in : (uintptr_t)out;

	while (size) {
		size_t xfer_size;
		void *sin;
		const void *sout;
		int pio = 0;

		/*
		 * PIO mode for non-cacheline aligned pointers as well as
		 * requests with size < line_size.
		 */
		if (ALIGN_DOWN(start, line_size) != start || size < line_size) {
			pio = 1;

			/*
			 * Determine how many bytes to transfer to align
			 * start to a cacheline. Watch out for start already
			 * beign aligned to a cacheline.
			 */
			xfer_size = ALIGN_UP(start, line_size) - start;
			/* Catch smaller transfers */
			xfer_size = MIN(xfer_size, size);

			if (xfer_size == 0)
				xfer_size = size;

			/* PIO fifo lengths need to be enforced. */
			xfer_size = MIN(xfer_size, SPI_MAX_XFER_BYTES_FIFO);
		} else { /* start is aligned and size >= line_size */
			xfer_size = MIN(size, SPI_MAX_XFER_BYTES_DMA);
			/* Ensure DMA packet sizes are aligned. */
			xfer_size = ALIGN_DOWN(xfer_size, line_size);
		}

		if (in) {
			sin = (void *)start;
			sout = NULL;
			if (!pio)
				dcache_invalidate_by_mva((void *)start,
							xfer_size);
		} else {
			sout = (void *)start;
			sin = NULL;
			if (!pio)
				dcache_clean_by_mva((void *)start, xfer_size);
		}

		if (pio) {
			if (tegra_spi_pio_transfer(bus, sin, sout, xfer_size))
				return -1;
		} else {
			if (tegra_spi_dma_transfer(bus, sin, sout, xfer_size))
				return -1;
		}

		size -= xfer_size;
		start += xfer_size;
	}

	return 0;
}

static int tegra_spi_stop(SpiOps *me)
{
	TegraSpi *bus = container_of(me, TegraSpi, ops);
	TegraSpiRegs *regs = bus->reg_addr;

	if (!bus->started) {
		printf("%s: Bus not yet started.\n", __func__);
		return -1;
	}

	writel(readl(&regs->command1) ^ SPI_CMD1_CS_SW_VAL, &regs->command1);

	bus->started = 0;

	return 0;
}

TegraSpi *new_tegra_spi(uintptr_t reg_addr,
			TegraApbDmaController *dma_controller,
			uint32_t dma_slave_id)
{
	TegraSpi *bus = xzalloc(sizeof(*bus));
	bus->ops.start = &tegra_spi_start;
	bus->ops.transfer = &tegra_spi_transfer;
	bus->ops.stop = &tegra_spi_stop;
	bus->reg_addr = (void *)reg_addr;
	bus->dma_slave_id = dma_slave_id;
	bus->dma_controller = dma_controller;
	return bus;
}
