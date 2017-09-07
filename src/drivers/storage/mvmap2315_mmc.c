/*
 * (C) Copyright 2009 SAMSUNG Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Jaehoon Chung <jh80.chung@samsung.com>
 * Portions Copyright 2011-2013 NVIDIA Corporation
 * Portions Copyright 2016 Marvell Corporation
 *
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
 */

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <stdint.h>
#include "drivers/storage/mvmap2315_mmc.h"

static void mvmap2315_mmc_set_power(Mvmap2315MmcHost *host, u16 power)
{
	u16 pwr = 0;
	u16 control_reg;

	mmc_debug("%s: power = %x\n", __func__, power);

	if (power != (u16)-1) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = MVMAP2315_MMC_PWRCTL_SD_BUS_VOLTAGE_V1_8;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = MVMAP2315_MMC_PWRCTL_SD_BUS_VOLTAGE_V3_0;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = MVMAP2315_MMC_PWRCTL_SD_BUS_VOLTAGE_V3_3;
			break;
		}
	}
	mmc_debug("%s: pwr = %X\n", __func__, pwr);

	/* Set the bus voltage first (if any) */
	control_reg = readw(&host->reg->SD_HOST_CTRL0);
	control_reg = control_reg && 0x00ff;
	control_reg |= pwr;
	writew(control_reg, &host->reg->SD_HOST_CTRL0);
	if (pwr == 0)
		return;

	/* Now enable bus power */
	control_reg |= MVMAP2315_MMC_PWRCTL_SD_BUS_POWER;
	writew(control_reg, &host->reg->SD_HOST_CTRL0);
}

static void mvmap2315_mmc_prepare_data(Mvmap2315MmcHost *host, MmcData *data,
				       struct bounce_buffer *bbstate)
{
	u8 ctrl;

	mmc_debug("%s: buf: %p (%p), data->blocks: %u, data->blocksize: %u\n",
		  __func__, bbstate->bounce_buffer, bbstate->user_buffer,
		  data->blocks, data->blocksize);

	writel((uintptr_t)bbstate->bounce_buffer, &host->reg->SD_SYS_ADDR_LOW0);

	ctrl = readb(&host->reg->SD_HOST_CTRL0);
	ctrl &= ~MVMAP2315_MMC_HOSTCTL_DMASEL_MASK;
	ctrl |= MVMAP2315_MMC_HOSTCTL_DMASEL_SDMA;
	writeb(ctrl, &host->reg->SD_HOST_CTRL0);

	/* We do not handle DMA boundaries, so set it to max (512 KiB) */
	writew((7 << 12) | (data->blocksize & 0xFFF),
	       &host->reg->SD_BLOCK_SIZE0);
	writew(data->blocks, &host->reg->SD_BLOCK_COUNT0);

	mmc_debug("block size: 0x%08x\n", readl(&host->reg->SD_BLOCK_SIZE0));
}

static void mvmap2315_mmc_set_transfer_mode(Mvmap2315MmcHost *host,
					    MmcData *data)
{
	u16 mode;

	mmc_debug("%s called\n", __func__);

	mode = (MVMAP2315_MMC_TRNMOD_DMA_ENABLE |
		MVMAP2315_MMC_TRNMOD_BLOCK_COUNT_ENABLE);

	if (data->blocks > 1)
		mode |= MVMAP2315_MMC_TRNMOD_MULTI_BLOCK_SELECT;

	if (data->flags & MMC_DATA_READ)
		mode |= MVMAP2315_MMC_TRNMOD_DATA_XFER_DIR_SEL_READ;

	writew(mode, &host->reg->SD_TRANSFER_MODE0);
}

static int mvmap2315_mmc_wait_inhibit(Mvmap2315MmcHost *host,
				      MmcCommand *cmd,
				      MmcData *data,
				      unsigned int timeout)
{
	unsigned int mask = MVMAP2315_MMC_PRNSTS_CMD_INHIBIT_CMD;

	/*
	 * We shouldn't wait for data inhibit for stop commands, even
	 * though they might use busy signaling
	 */
	if ((data == NULL) && (cmd->resp_type & MMC_RSP_BUSY))
		mask |= MVMAP2315_MMC_PRNSTS_CMD_INHIBIT_DAT;

	while (readl(&host->reg->SD_PRESENT_STATE_00) & mask) {
		if (timeout == 0) {
			mmc_error("%s: timeout error\n", __func__);
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	return 0;
}

static int mvmap2315_mmc_send_cmd_bounced(MmcCtrlr *ctrlr, MmcCommand *cmd,
					  MmcData *data,
					  struct bounce_buffer *bbstate)
{
	Mvmap2315MmcHost *host = container_of(ctrlr, Mvmap2315MmcHost, mmc);
	int flags, i;
	int result;
	unsigned int mask = 0;
	const int retry = 10;

	mmc_debug("%s called\n", __func__);

	result = mvmap2315_mmc_wait_inhibit(host, cmd, data, 10 /* ms */);

	if (result < 0)
		return result;

	if (data)
		mvmap2315_mmc_prepare_data(host, data, bbstate);

	mmc_debug("cmd->arg: %08x\n", cmd->cmdarg);
	writel(cmd->cmdarg, &host->reg->SD_ARG_LOW0);

	if (data)
		mvmap2315_mmc_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY))
		return -1;

	if (!(cmd->resp_type & MMC_RSP_PRESENT))
		flags = MVMAP2315_MMC_CMDREG_RESP_TYPE_SELECT_NO_RESPONSE;

	else if (cmd->resp_type & MMC_RSP_136)
		flags = MVMAP2315_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_136;

	else if (cmd->resp_type & MMC_RSP_BUSY)
		flags = MVMAP2315_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_48_BUSY;

	else
		flags = MVMAP2315_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_48;

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= MVMAP2315_MMC_TRNMOD_CMD_CRC_CHECK;

	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= MVMAP2315_MMC_TRNMOD_CMD_INDEX_CHECK;

	if (data)
		flags |= MVMAP2315_MMC_TRNMOD_DATA_PRESENT_SELECT_DATA_XFER;

	mmc_debug("cmd: %d\n", cmd->cmdidx);

	writew((cmd->cmdidx << 8) | flags, &host->reg->SD_CMD0);

	for (i = 0; i < retry; i++) {
		mask = readl(&host->reg->SD_NORMAL_INT_STATUS0);
		/* Command Complete */
		if (mask & MVMAP2315_MMC_NORINTSTS_CMD_COMPLETE) {
			if (!data)
				writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);
			break;
		}
		udelay(1000);
	}

	if (i == retry) {
		mmc_error("%s: waiting for status update\n", __func__);
		writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);
		return MMC_TIMEOUT;
	}

	if (mask & MVMAP2315_MMC_NORINTSTS_CMD_TIMEOUT) {
		/* Timeout Error */
		mmc_debug("timeout: %08x cmd %d\n", mask, cmd->cmdidx);
		writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);

		return MMC_TIMEOUT;
	} else if (mask & MVMAP2315_MMC_NORINTSTS_ERR_INTERRUPT) {
		/* Error Interrupt */
		mmc_error("%08x cmd %d\n", mask, cmd->cmdidx);
		writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);

		return -1;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			for (i = 0; i < 4; i++) {
				u32 *offset = (u32 *)(&host->reg->SD_RESP_60 -
					      (2 * i));

				mmc_debug("offset=%lx\n", (uintptr_t)offset);
				cmd->response[i] = readl(offset);

				mmc_debug("cmd->resp[%d]: %08x\n",
					  i, cmd->response[i]);
			}
		} else if (cmd->resp_type & MMC_RSP_BUSY) {
			cmd->response[0] = readl(&host->reg->SD_RESP_00);
			mmc_debug("cmd->resp[0]: %08x\n", cmd->response[0]);

		} else {
			cmd->response[0] = readl(&host->reg->SD_RESP_00);
			mmc_debug("cmd->resp[0]: %08x\n", cmd->response[0]);
		}
	}

	if (data) {
		u64 start = timer_us(0);
		u64 timeout_ms = 60000;

		while (1) {
			mask = readl(&host->reg->SD_NORMAL_INT_STATUS0);

			if (mask & MVMAP2315_MMC_NORINTSTS_ERR_INTERRUPT) {
				/* Error Interrupt */
				writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);
				mmc_error("%s: error during transfer: 0x%08x\n",
					  __func__, mask);

				return -1;
			} else if (mask &
				MVMAP2315_MMC_NORINTSTS_DMA_INTERRUPT) {
				/*
				 * DMA Interrupt, restart the transfer where
				 * it was interrupted.
				 */
				unsigned int address = readl(
					&host->reg->SD_SYS_ADDR_LOW0);

				mmc_debug("DMA end\n");

				writel(MVMAP2315_MMC_NORINTSTS_DMA_INTERRUPT,
				       &host->reg->SD_NORMAL_INT_STATUS0);
				writel(address, &host->reg->SD_SYS_ADDR_LOW0);

			} else if (mask &
				MVMAP2315_MMC_NORINTSTS_XFER_COMPLETE) {
				/* Transfer Complete */
				mmc_debug("r/w is done\n");

				break;
			} else if (timer_us(start) / 1000 > timeout_ms) {
				writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);
				mmc_error("%s: MMC Timeout\n"
					"    Interrupt status	0x%08x\n"
					"    Interrupt status enable 0x%08x\n"
					"    Interrupt signal enable 0x%08x\n"
					"    Present status	  0x%08x\n",
					__func__, mask, readl(
					&host->reg->SD_NORMAL_INT_STATUS_EN0),
					readl(
					&host->reg->
		SD_NORMAL_INT_STATUS_SIG_EN0),
				       readl(&host->reg->SD_PRESENT_STATE_00));

				return -1;
			}
		}
		writel(mask, &host->reg->SD_NORMAL_INT_STATUS0);
	}

	return 0;
}

static int mvmap2315_mmc_send_cmd(MmcCtrlr *ctrlr, MmcCommand *cmd,
				  MmcData *data)
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

	ret = mvmap2315_mmc_send_cmd_bounced(ctrlr, cmd, data, &bbstate);

	if (data)
		bounce_buffer_stop(&bbstate);

	return ret;
}

static int mvmap2315_mmc_change_clock(Mvmap2315MmcHost *host, u32 clock)
{
	int div;
	u16 clk;
	unsigned long timeout;
	u32 mask;

	mmc_debug("%s called, frequency is %d.\n", __func__, clock);

	/* Change clock only if necessary. */
	if (clock == 0 || clock == host->clock) {
		host->clock = clock;
		return 0;
	}

	/* Clear clock settings and stop SD clock. */
	writew(0, &host->reg->SD_CLOCK_CTRL0);

	assert(host->src_hz >= clock);
	div = host->src_hz / clock;

	clk = ((div << MVMAP2315_MMC_CLKCON_SDCLK_FREQ_SEL_SHIFT) |
		MVMAP2315_MMC_CLKCON_INTERNAL_CLOCK_ENABLE);
	writew(clk, &host->reg->SD_CLOCK_CTRL0);

	/* Wait max 10 ms */
	timeout = 10;
	while (!(readw(&host->reg->SD_CLOCK_CTRL0) &
		 MVMAP2315_MMC_CLKCON_INTERNAL_CLOCK_STABLE)) {
		if (timeout == 0) {
			mmc_error("%s: timeout error\n", __func__);
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	mask = MVMAP2315_RESET_SETTING;
	writel(mask, &host->reg->EMMC_PHY_FUNC_CONTROL_00);

	mask = MVMAP2315_RESET_SETTINGS;
	writel(mask, &host->reg->EMMC_PHY_TIMING_ADJUST_00);

	mask = 0;
	writel(mask, &host->reg->EMMC_PHY_PAD_CONTROL_00);

	mask = 0;
	writel(mask, &host->reg->EMMC_PHY_PAD_CONTROL2_00);

	mask = readl(&host->reg->EMMC_PHY_PAD_CONTROL_00);
	mask |= MVMAP2315_AUTO_REC_EN_BIT | MVMAP2315_OUTJPUT_QSN_EN_BIT;
	writel(mask, &host->reg->EMMC_PHY_PAD_CONTROL_00);

	mask = readl(&host->reg->EMMC_PHY_PAD_CONTROL_00);
	mask |= MVMAP2315_PAD_CTRL_0;
	writel(mask, &host->reg->EMMC_PHY_PAD_CONTROL_00);

	mask = MVMAP2315_PAD_CTRL_1;
	writel(mask, &host->reg->EMMC_PHY_PAD_CONTROL1_00);

	mask = readl(&host->reg->EMMC_PHY_PAD_CONTROL2_00);
	mask |= MVMAP2315_PAD_CTRL_2;
	writel(mask, &host->reg->EMMC_PHY_PAD_CONTROL2_00);

	mask = readl(&host->reg->EMMC_PHY_DLL_CONTROL_00);
	mask |= MVMAP2315_PHY_EN;
	writel(mask, &host->reg->EMMC_PHY_DLL_CONTROL_00);

	while (readl(&host->reg->EMMC_PHY_TIMING_ADJUST_00) & MVMAP2315_PHY_EN)
		udelay(1000);

	clk |= MVMAP2315_MMC_CLKCON_SD_CLOCK_ENABLE;
	writew(clk, &host->reg->SD_CLOCK_CTRL0);

	mmc_debug("%s: clkcon = %08X\n", __func__, clk);

	host->clock = clock;

	return 0;
}

static void mvmap2315_mmc_set_ios(MmcCtrlr *ctrlr)
{
	Mvmap2315MmcHost *host = container_of(ctrlr, Mvmap2315MmcHost, mmc);
	u8 ctrl;
	u32 result;

	mmc_debug("%s: called, bus_width: %x, clock: %d -> %d\n", __func__,
		  ctrlr->bus_width, host->clock, ctrlr->bus_hz);

	/* Change clock first */
	result = mvmap2315_mmc_change_clock(host, ctrlr->bus_hz);

	/* Timeout */
	if (result < 0)
		return;

	ctrl = readb(&host->reg->SD_HOST_CTRL0);

	if (ctrlr->bus_width == 8)
		ctrl |= (1 << 5);
	else if (ctrlr->bus_width == 4)
		ctrl |= (1 << 1);
	else
		ctrl &= ~(1 << 1);

	writeb(ctrl, &host->reg->SD_HOST_CTRL0);
	mmc_debug("%s: hostctl = %08X\n", __func__, ctrl);
}

static int mvmap2315_mmc_reset(Mvmap2315MmcHost *host)
{
	unsigned int timeout;

	mmc_debug("%s called\n", __func__);
	u16 timer_reset;
	u32 mask;

	timer_reset = readw(&host->reg->SD_TIMEOUT_CTRL_SW_RESET0);
	timer_reset |= MVMAP2315_MMC_SWRST_SW_RESET_FOR_ALL;
	writew(timer_reset, &host->reg->SD_TIMEOUT_CTRL_SW_RESET0);

	host->clock = 0;

	/* Wait max 100 ms */
	timeout = 100;

	/* HW clears the bit when it's done */
	while (readw(&host->reg->SD_TIMEOUT_CTRL_SW_RESET0) &
		MVMAP2315_MMC_SWRST_SW_RESET_FOR_ALL) {
		if (timeout == 0) {
			mmc_error("%s: timeout error\n", __func__);
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	/* Set SD bus voltage & enable bus power */
	mvmap2315_mmc_set_power(host, MVMAP2315_MMC_VOLTAGES_MSB - 1);
	mmc_debug("%s: host control = %0X,\n", __func__,
		  readw(&host->reg->SD_HOST_CTRL0));

	/*
	*  This is a workaround for a bug, data line hangs
	*  unless an cmd/data reset is issued. Investigation
	*  of root cause is underway
	*/
	mask = readl((u32 *)0xE013882C);
	mask |= 0x06000000;
	writel(mask, (u32 *)0xE013882C);

	return 0;
}

static int mvmap2315_mmc_init(Mvmap2315MmcHost *host)
{
	u32 result;

	mmc_debug("%s called\n", __func__);

	result = mvmap2315_mmc_reset(host);

	if (result < 0)
		return -1;

	mmc_debug("host version = %x\n", readw(&host->reg->SD_HOST_CTRL_VER0));

	/* will use polling, so mask all interrupts */
	writel(0xFFFFFFFF, &host->reg->SD_NORMAL_INT_STATUS_EN0);
	writel(0x0, &host->reg->SD_NORMAL_INT_STATUS_SIG_EN0);

	writeb(0xe, &host->reg->SD_TIMEOUT_CTRL_SW_RESET0);

	return 0;
}

static int mvmap2315_mmc_update(BlockDevCtrlrOps *me)
{
	Mvmap2315MmcHost *host = container_of(me, Mvmap2315MmcHost,
		mmc.ctrlr.ops);
	if (!host->initialized && mvmap2315_mmc_init(host))
		return -1;
	host->initialized = 1;

	if (mmc_setup_media(&host->mmc))
		return -1;
	host->mmc.media->dev.name = "SD card";
	host->mmc.media->dev.removable = 0;
	host->mmc.media->dev.ops.read = &block_mmc_read;
	host->mmc.media->dev.ops.write = &block_mmc_write;
	host->mmc.media->dev.ops.new_stream = &new_simple_stream;
	list_insert_after(&host->mmc.media->dev.list_node,
			  &fixed_block_devices);
	host->mmc.ctrlr.need_update = 0;
	mmc_error("made it to the end of mvmap2315_mmc_update\n");
	return 0;
}

Mvmap2315MmcHost *new_mvmap2315_mmc_host(uintptr_t ioaddr,
					 int bus_width,
					 u32 min_freq_hz,
					 u32 max_freq_hz,
					 u32 src_freq_hz)
{
	Mvmap2315MmcHost *ctrlr = xzalloc(sizeof(*ctrlr));

	ctrlr->mmc.ctrlr.ops.is_bdev_owned = block_mmc_is_bdev_owned;
	ctrlr->mmc.ctrlr.ops.update = &mvmap2315_mmc_update;
	ctrlr->mmc.ctrlr.need_update = 1;

	ctrlr->mmc.voltages = MVMAP2315_MMC_VOLTAGES;
	ctrlr->mmc.f_min = min_freq_hz;
	ctrlr->mmc.f_max = max_freq_hz;

	ctrlr->mmc.bus_width = bus_width;
	ctrlr->mmc.bus_hz = ctrlr->mmc.f_min;
	ctrlr->mmc.b_max = 65535;
	if (bus_width == 8)
		ctrlr->mmc.caps |= MMC_MODE_8BIT;
	else if (bus_width == 4)
		ctrlr->mmc.caps |= MMC_MODE_4BIT;

	ctrlr->mmc.caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_HC;
	ctrlr->mmc.send_cmd = &mvmap2315_mmc_send_cmd;
	ctrlr->mmc.set_ios = &mvmap2315_mmc_set_ios;

	ctrlr->src_hz = src_freq_hz;

	ctrlr->reg = (Mvmap2315MmcReg *)ioaddr;

	return ctrlr;
}
