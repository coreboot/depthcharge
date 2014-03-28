/*
 * (C) Copyright 2009 SAMSUNG Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Jaehoon Chung <jh80.chung@samsung.com>
 * Portions Copyright 2011-2013 NVIDIA Corporation
 *
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/storage/tegra_mmc.h"

enum {
	// For card identification, and also the highest low-speed SDOI card
	// frequency (actually 400Khz).
	TegraMmcMinFreq = 375000,

	// Highest HS eMMC clock as per the SD/MMC spec (actually 52MHz).
	TegraMmcMaxFreq = 48000000,

	// Source clock configured by loader in previous stage.
	TegraMmcSourceClock = 48000000,

	TegraMmcVoltages = (MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195),

	// MSB of the mmc.voltages. Current value is made by MMC_VDD_33_34.
	TegraMmcVoltagesMsb = 22,
};


static void tegra_mmc_set_power(TegraMmcHost *host, uint16_t power)
{
	uint8_t pwr = 0;
	mmc_debug("%s: power = %x\n", __func__, power);

	if (power != (uint16_t)-1) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = TEGRA_MMC_PWRCTL_SD_BUS_VOLTAGE_V1_8;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = TEGRA_MMC_PWRCTL_SD_BUS_VOLTAGE_V3_0;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = TEGRA_MMC_PWRCTL_SD_BUS_VOLTAGE_V3_3;
			break;
		}
	}
	mmc_debug("%s: pwr = %X\n", __func__, pwr);

	// Set the bus voltage first (if any)
	writeb(pwr, &host->reg->pwrcon);
	if (pwr == 0)
		return;

	// Now enable bus power
	pwr |= TEGRA_MMC_PWRCTL_SD_BUS_POWER;
	writeb(pwr, &host->reg->pwrcon);
}

static void tegra_mmc_prepare_data(TegraMmcHost *host, MmcData *data,
				   struct bounce_buffer *bbstate)
{
	uint8_t ctrl;

	mmc_debug("%s: buf: %p (%p), data->blocks: %u, data->blocksize: %u\n",
		  __func__, bbstate->bounce_buffer, bbstate->user_buffer,
		  data->blocks, data->blocksize);

	writel((uint32_t)bbstate->bounce_buffer, &host->reg->sysad);
	/*
	 * DMASEL[4:3]
	 * 00 = Selects SDMA
	 * 01 = Reserved
	 * 10 = Selects 32-bit Address ADMA2
	 * 11 = Selects 64-bit Address ADMA2
	 */
	ctrl = readb(&host->reg->hostctl);
	ctrl &= ~TEGRA_MMC_HOSTCTL_DMASEL_MASK;
	ctrl |= TEGRA_MMC_HOSTCTL_DMASEL_SDMA;
	writeb(ctrl, &host->reg->hostctl);

	// We do not handle DMA boundaries, so set it to max (512 KiB)
	writew((7 << 12) | (data->blocksize & 0xFFF), &host->reg->blksize);
	writew(data->blocks, &host->reg->blkcnt);
}

static void tegra_mmc_set_transfer_mode(TegraMmcHost *host, MmcData *data)
{
	uint16_t mode;
	mmc_debug("%s called\n", __func__);
	/*
	 * TRNMOD
	 * MUL1SIN0[5]	: Multi/Single Block Select
	 * RD1WT0[4]	: Data Transfer Direction Select
	 *	1 = read
	 *	0 = write
	 * ENACMD12[2]	: Auto CMD12 Enable
	 * ENBLKCNT[1]	: Block Count Enable
	 * ENDMA[0]	: DMA Enable
	 */
	mode = (TEGRA_MMC_TRNMOD_DMA_ENABLE |
		TEGRA_MMC_TRNMOD_BLOCK_COUNT_ENABLE);

	if (data->blocks > 1)
		mode |= TEGRA_MMC_TRNMOD_MULTI_BLOCK_SELECT;

	if (data->flags & MMC_DATA_READ)
		mode |= TEGRA_MMC_TRNMOD_DATA_XFER_DIR_SEL_READ;

	writew(mode, &host->reg->trnmod);
}

static int tegra_mmc_wait_inhibit(TegraMmcHost *host,
				  MmcCommand *cmd,
				  MmcData *data,
				  unsigned int timeout)
{
	/*
	 * PRNSTS
	 * CMDINHDAT[1] : Command Inhibit (DAT)
	 * CMDINHCMD[0] : Command Inhibit (CMD)
	 */
	unsigned int mask = TEGRA_MMC_PRNSTS_CMD_INHIBIT_CMD;

	/*
	 * We shouldn't wait for data inhibit for stop commands, even
	 * though they might use busy signaling
	 */
	if ((data == NULL) && (cmd->resp_type & MMC_RSP_BUSY))
		mask |= TEGRA_MMC_PRNSTS_CMD_INHIBIT_DAT;

	while (readl(&host->reg->prnsts) & mask) {
		if (timeout == 0) {
			mmc_error("%s: timeout error\n", __func__);
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	return 0;
}

static int tegra_mmc_send_cmd_bounced(MmcCtrlr *ctrlr, MmcCommand *cmd,
			MmcData *data, struct bounce_buffer *bbstate)
{
	TegraMmcHost *host = container_of(ctrlr, TegraMmcHost, mmc);
	int flags, i;
	int result;
	unsigned int mask = 0;
	unsigned int retry = 0x100000;
	mmc_debug("%s called\n", __func__);

	result = tegra_mmc_wait_inhibit(host, cmd, data, 10 /* ms */);

	if (result < 0)
		return result;

	if (data)
		tegra_mmc_prepare_data(host, data, bbstate);

	mmc_debug("cmd->arg: %08x\n", cmd->cmdarg);
	writel(cmd->cmdarg, &host->reg->argument);

	if (data)
		tegra_mmc_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY))
		return -1;

	/*
	 * CMDREG
	 * CMDIDX[13:8]	: Command index
	 * DATAPRNT[5]	: Data Present Select
	 * ENCMDIDX[4]	: Command Index Check Enable
	 * ENCMDCRC[3]	: Command CRC Check Enable
	 * RSPTYP[1:0]
	 *	00 = No Response
	 *	01 = Length 136
	 *	10 = Length 48
	 *	11 = Length 48 Check busy after response
	 */
	if (!(cmd->resp_type & MMC_RSP_PRESENT))
		flags = TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_NO_RESPONSE;
	else if (cmd->resp_type & MMC_RSP_136)
		flags = TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_136;
	else if (cmd->resp_type & MMC_RSP_BUSY)
		flags = TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_48_BUSY;
	else
		flags = TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_48;

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= TEGRA_MMC_TRNMOD_CMD_CRC_CHECK;
	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= TEGRA_MMC_TRNMOD_CMD_INDEX_CHECK;
	if (data)
		flags |= TEGRA_MMC_TRNMOD_DATA_PRESENT_SELECT_DATA_TRANSFER;

	mmc_debug("cmd: %d\n", cmd->cmdidx);

	writew((cmd->cmdidx << 8) | flags, &host->reg->cmdreg);

	for (i = 0; i < retry; i++) {
		mask = readl(&host->reg->norintsts);
		// Command Complete
		if (mask & TEGRA_MMC_NORINTSTS_CMD_COMPLETE) {
			if (!data)
				writel(mask, &host->reg->norintsts);
			break;
		}
	}

	if (i == retry) {
		mmc_error("%s: waiting for status update\n", __func__);
		writel(mask, &host->reg->norintsts);
		return MMC_TIMEOUT;
	}

	if (mask & TEGRA_MMC_NORINTSTS_CMD_TIMEOUT) {
		// Timeout Error
		mmc_debug("timeout: %08x cmd %d\n", mask, cmd->cmdidx);
		writel(mask, &host->reg->norintsts);
		return MMC_TIMEOUT;
	} else if (mask & TEGRA_MMC_NORINTSTS_ERR_INTERRUPT) {
		// Error Interrupt
		mmc_error("%08x cmd %d\n", mask, cmd->cmdidx);
		writel(mask, &host->reg->norintsts);
		return -1;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			// CRC is stripped so we need to do some shifting.
			for (i = 0; i < 4; i++) {
				uint32_t *offset = &host->reg->rspreg3 - i;
				cmd->response[i] = readl(offset) << 8;

				if (i != 3) {
					cmd->response[i] |=
						readb((uint8_t *)offset - 1);
				}
				mmc_debug("cmd->resp[%d]: %08x\n",
						i, cmd->response[i]);
			}
		} else if (cmd->resp_type & MMC_RSP_BUSY) {
			for (i = 0; i < retry; i++) {
				// PRNTDATA[23:20] : DAT[3:0] Line Signal
				if (readl(&host->reg->prnsts)
					& (1 << 20))	// DAT[0]
					break;
			}

			if (i == retry) {
				mmc_error("%s: card is still busy\n", __func__);
				writel(mask, &host->reg->norintsts);
				return MMC_TIMEOUT;
			}

			cmd->response[0] = readl(&host->reg->rspreg0);
			mmc_debug("cmd->resp[0]: %08x\n", cmd->response[0]);
		} else {
			cmd->response[0] = readl(&host->reg->rspreg0);
			mmc_debug("cmd->resp[0]: %08x\n", cmd->response[0]);
		}
	}

	if (data) {
		uint64_t start = timer_us(0);
		uint64_t timeout_ms = 2000;

		while (1) {
			mask = readl(&host->reg->norintsts);

			if (mask & TEGRA_MMC_NORINTSTS_ERR_INTERRUPT) {
				// Error Interrupt
				writel(mask, &host->reg->norintsts);
				mmc_error("%s: error during transfer: 0x%08x\n",
						__func__, mask);
				return -1;
			} else if (mask & TEGRA_MMC_NORINTSTS_DMA_INTERRUPT) {
				/*
				 * DMA Interrupt, restart the transfer where
				 * it was interrupted.
				 */
				unsigned int address = readl(&host->reg->sysad);

				mmc_debug("DMA end\n");
				writel(TEGRA_MMC_NORINTSTS_DMA_INTERRUPT,
				       &host->reg->norintsts);
				writel(address, &host->reg->sysad);
			} else if (mask & TEGRA_MMC_NORINTSTS_XFER_COMPLETE) {
				// Transfer Complete
				mmc_debug("r/w is done\n");
				break;
			} else if (timer_us(start) / 1000 > timeout_ms) {
				writel(mask, &host->reg->norintsts);
				mmc_error("%s: MMC Timeout\n"
				       "    Interrupt status        0x%08x\n"
				       "    Interrupt status enable 0x%08x\n"
				       "    Interrupt signal enable 0x%08x\n"
				       "    Present status          0x%08x\n",
				       __func__, mask,
				       readl(&host->reg->norintstsen),
				       readl(&host->reg->norintsigen),
				       readl(&host->reg->prnsts));
				return -1;
			}
		}
		writel(mask, &host->reg->norintsts);
	}

	udelay(1000);
	return 0;
}

static int tegra_mmc_send_cmd(MmcCtrlr *ctrlr, MmcCommand *cmd, MmcData *data)
{
	void *buf;
	unsigned int bbflags;
	size_t len;
	struct bounce_buffer bbstate;
	int ret;

	if (data) {
		if (data->flags & MMC_DATA_READ) {
			buf = data->dest;
			bbflags = GEN_BB_WRITE;
		} else {
			buf = (void *)data->src;
			bbflags = GEN_BB_READ;
		}
		len = data->blocks * data->blocksize;

		bounce_buffer_start(&bbstate, buf, len, bbflags);
	}

	ret = tegra_mmc_send_cmd_bounced(ctrlr, cmd, data, &bbstate);

	if (data)
		bounce_buffer_stop(&bbstate);

	return ret;
}

static void tegra_mmc_change_clock(TegraMmcHost *host, uint32_t clock)
{
	int div;
	uint16_t clk;
	unsigned long timeout;

	mmc_debug("%s called\n", __func__);

	// Change clock only if necessary.
	if (clock == 0 || clock == host->clock) {
		host->clock = clock;
		return;
	}

	// Clear clock settings and stop SD clock.
	writew(0, &host->reg->clkcon);

	// Try to change clock by SD clock divisor (base clock is already
	// specified in src_hz).
	assert(host->src_hz >= clock);
	div = host->src_hz / clock;

	/*
	 * CLKCON
	 * SELFREQ[15:8]	: base clock divided by value
	 * ENSDCLK[2]		: SD Clock Enable
	 * STBLINTCLK[1]	: Internal Clock Stable
	 * ENINTCLK[0]		: Internal Clock Enable
	 */
	div >>= 1;
	clk = ((div << TEGRA_MMC_CLKCON_SDCLK_FREQ_SEL_SHIFT) |
	       TEGRA_MMC_CLKCON_INTERNAL_CLOCK_ENABLE);
	writew(clk, &host->reg->clkcon);

	// Wait max 10 ms
	timeout = 10;
	while (!(readw(&host->reg->clkcon) &
		 TEGRA_MMC_CLKCON_INTERNAL_CLOCK_STABLE)) {
		if (timeout == 0) {
			mmc_error("%s: timeout error\n", __func__);
			return;
		}
		timeout--;
		udelay(1000);
	}

	clk |= TEGRA_MMC_CLKCON_SD_CLOCK_ENABLE;
	writew(clk, &host->reg->clkcon);

	mmc_debug("%s: clkcon = %08X\n", __func__, clk);

	host->clock = clock;
}

static void tegra_mmc_set_ios(MmcCtrlr *ctrlr)
{
	TegraMmcHost *host = container_of(ctrlr, TegraMmcHost, mmc);
	uint8_t ctrl;

	mmc_debug("%s: called, bus_width: %x, clock: %d -> %d\n", __func__,
		  ctrlr->bus_width, host->clock, ctrlr->bus_hz);

	// Change clock first
	tegra_mmc_change_clock(host, ctrlr->bus_hz);
	ctrl = readb(&host->reg->hostctl);

	/*
	 * WIDE8[5]
	 * 0 = Depend on WIDE4
	 * 1 = 8-bit mode
	 * WIDE4[1]
	 * 1 = 4-bit mode
	 * 0 = 1-bit mode
	 */
	if (ctrlr->bus_width == 8)
		ctrl |= (1 << 5);
	else if (ctrlr->bus_width == 4)
		ctrl |= (1 << 1);
	else
		ctrl &= ~(1 << 1);

	writeb(ctrl, &host->reg->hostctl);
	mmc_debug("%s: hostctl = %08X\n", __func__, ctrl);
}

static void tegra_mmc_reset(TegraMmcHost *host)
{
	unsigned int timeout;
	mmc_debug("%s called\n", __func__);

	/*
	 * RSTALL[0] : Software reset for all
	 * 1 = reset
	 * 0 = work
	 */
	writeb(TEGRA_MMC_SWRST_SW_RESET_FOR_ALL, &host->reg->swrst);

	host->clock = 0;

	// Wait max 100 ms
	timeout = 100;

	// hw clears the bit when it's done
	while (readb(&host->reg->swrst) & TEGRA_MMC_SWRST_SW_RESET_FOR_ALL) {
		if (timeout == 0) {
			mmc_error("%s: timeout error\n", __func__);
			return;
		}
		timeout--;
		udelay(1000);
	}

	// Set SD bus voltage & enable bus power
	tegra_mmc_set_power(host, TegraMmcVoltagesMsb - 1);
	mmc_debug("%s: power control = %02X, host control = %02X\n", __func__,
		readb(&host->reg->pwrcon), readb(&host->reg->hostctl));
}

static int tegra_mmc_init(BlockDevCtrlrOps *me)
{
	TegraMmcHost *host = container_of(me, TegraMmcHost, mmc.ctrlr.ops);
	unsigned int mask;
	mmc_debug("%s called\n", __func__);

	tegra_mmc_reset(host);

	mmc_debug("host version = %x\n", readw(&host->reg->hcver));

	// mask all
	writel(0xffffffff, &host->reg->norintstsen);
	writel(0xffffffff, &host->reg->norintsigen);

	writeb(0xe, &host->reg->timeoutcon);	// TMCLK * 2^27
	/*
	 * NORMAL Interrupt Status Enable Register init
	 * [5] ENSTABUFRDRDY : Buffer Read Ready Status Enable
	 * [4] ENSTABUFWTRDY : Buffer write Ready Status Enable
	 * [3] ENSTADMAINT   : DMA boundary interrupt
	 * [1] ENSTASTANSCMPLT : Transfre Complete Status Enable
	 * [0] ENSTACMDCMPLT : Command Complete Status Enable
	*/
	mask = readl(&host->reg->norintstsen);
	mask &= ~(0xffff);
	mask |= (TEGRA_MMC_NORINTSTSEN_CMD_COMPLETE |
		 TEGRA_MMC_NORINTSTSEN_XFER_COMPLETE |
		 TEGRA_MMC_NORINTSTSEN_DMA_INTERRUPT |
		 TEGRA_MMC_NORINTSTSEN_BUFFER_WRITE_READY |
		 TEGRA_MMC_NORINTSTSEN_BUFFER_READ_READY);
	writel(mask, &host->reg->norintstsen);

	/*
	 * NORMAL Interrupt Signal Enable Register init
	 * [1] ENSTACMDCMPLT : Transfer Complete Signal Enable
	 */
	mask = readl(&host->reg->norintsigen);
	mask &= ~(0xffff);
	mask |= TEGRA_MMC_NORINTSIGEN_XFER_COMPLETE;
	writel(mask, &host->reg->norintsigen);

	return 0;
}

// TODO(hungte) Generalize MMC update procedure.
static int tegra_mmc_update(BlockDevCtrlrOps *me)
{
	TegraMmcHost *host = container_of(me, TegraMmcHost, mmc.ctrlr.ops);
	if (!host->initialized && tegra_mmc_init(me))
		return -1;
	host->initialized = 1;

	if (host->removable) {
		int present = host->cd_gpio->get(host->cd_gpio);
		if (present && !host->mmc.media) {
			// A card is present and not set up yet. Get it ready.
			if (mmc_setup_media(&host->mmc))
				return -1;
			host->mmc.media->dev.name = "removable tegra_mmc";
			host->mmc.media->dev.removable = 1;
			host->mmc.media->dev.ops.read = &block_mmc_read;
			host->mmc.media->dev.ops.write = &block_mmc_write;
			list_insert_after(&host->mmc.media->dev.list_node,
					  &removable_block_devices);
		} else if (!present && host->mmc.media) {
			// A card was present but isn't any more. Get rid of it.
			list_remove(&host->mmc.media->dev.list_node);
			free(host->mmc.media);
			host->mmc.media = NULL;
		}
	} else {
		if (mmc_setup_media(&host->mmc))
			return -1;
		host->mmc.media->dev.name = "tegra_mmc";
		host->mmc.media->dev.removable = 0;
		host->mmc.media->dev.ops.read = &block_mmc_read;
		host->mmc.media->dev.ops.write = &block_mmc_write;
		list_insert_after(&host->mmc.media->dev.list_node,
				  &fixed_block_devices);
		host->mmc.ctrlr.need_update = 0;
	}
	return 0;
}

TegraMmcHost *new_tegra_mmc_host(uintptr_t ioaddr, int bus_width,
				 int removable, GpioOps *card_detect)
{
	TegraMmcHost *ctrlr = xzalloc(sizeof(*ctrlr));

	ctrlr->mmc.ctrlr.ops.update = &tegra_mmc_update;
	ctrlr->mmc.ctrlr.need_update = 1;

	ctrlr->mmc.voltages = TegraMmcVoltages;
	ctrlr->mmc.f_min = TegraMmcMinFreq;
	ctrlr->mmc.f_max = TegraMmcMaxFreq;

	ctrlr->mmc.bus_width = bus_width;
	ctrlr->mmc.bus_hz = ctrlr->mmc.f_min;
	ctrlr->mmc.b_max = 65535; // Some controllers use 16-bit regs.
	if (bus_width == 8) {
		ctrlr->mmc.caps |= MMC_MODE_8BIT;
		ctrlr->mmc.caps &= ~MMC_MODE_4BIT;
	} else {
		ctrlr->mmc.caps |= MMC_MODE_4BIT;
		ctrlr->mmc.caps &= ~MMC_MODE_8BIT;
	}
	ctrlr->mmc.caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_HC;
	ctrlr->mmc.send_cmd = &tegra_mmc_send_cmd;
	ctrlr->mmc.set_ios = &tegra_mmc_set_ios;

	ctrlr->src_hz = TegraMmcSourceClock;
	ctrlr->reg = (TegraMmcReg *)ioaddr;
	ctrlr->removable = removable;

	ctrlr->cd_gpio = card_detect;

	return ctrlr;
}
