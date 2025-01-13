/*
 * (C) Copyright 2012 Samsung Electronics Co. Ltd
 * Copyright 2013 Google LLC
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
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/init_funcs.h"
#include "arch/cache.h"
#include "drivers/storage/exynos_mshc.h"
#include "drivers/storage/mmc.h"

static void setbits32(uint32_t *data, uint32_t bits)
{
	write32(data, read32(data) | bits);
}

static void clrbits32(uint32_t *data, uint32_t bits)
{
	write32(data, read32(data) & ~bits);
}

static int mshci_setbits(MshciHost *host, unsigned int bits)
{
	setbits32(&host->regs->ctrl, bits);
	if (mmc_busy_wait_io(&host->regs->ctrl, NULL, bits,
			     S5P_MSHC_TIMEOUT_MS)) {
		mmc_error("Set bits failed\n");
		return -1;
	}

	return 0;
}

static int mshci_reset_all(MshciHost *host)
{
	/*
	* Before we reset ciu check the DATA0 line.  If it is low and
	* we resets the ciu then we might see some errors.
	*/
	if (mmc_busy_wait_io(&host->regs->status, NULL, DATA_BUSY,
			     S5P_MSHC_TIMEOUT_MS)) {
		mmc_error("Controller did not release data0 "
			  "before ciu reset\n");
		return -1;
	}

	if (mshci_setbits(host, CTRL_RESET)) {
		mmc_error("Fail to reset card.\n");
		return -1;
	}
	if (mshci_setbits(host, FIFO_RESET)) {
		mmc_error("Fail to reset fifo.\n");
		return -1;
	}
	if (mshci_setbits(host, DMA_RESET)) {
		mmc_error("Fail to reset dma.\n");
		return -1;
	}

	return 0;
}

static void mshci_set_mdma_desc(void *desc_vir, void *desc_phy,
				uint32_t des0, uint32_t des1, uint32_t des2)
{
	MshciIdmac *desc = (MshciIdmac *)desc_vir;

	desc->des0 = des0;
	desc->des1 = des1;
	desc->des2 = des2;
	desc->des3 = (uintptr_t)desc_phy + sizeof(MshciIdmac);
}

static int mshci_prepare_data(MshciHost *host, MmcData *data)
{
	unsigned int i, data_cnt, blksz;
	static MshciIdmac idmac_desc[0x10000];
	/* TODO(alim.akhtar@samsung.com): do we really need this big array? */

	if (mshci_setbits(host, FIFO_RESET)) {
		mmc_error("Fail to reset FIFO\n");
		return -1;
	}

	MshciIdmac *pdesc_dmac = idmac_desc;
	blksz = data->blocksize;
	data_cnt = data->blocks;

	for  (i = 0;; i++) {
		uint32_t des_flag = (MSHCI_IDMAC_OWN | MSHCI_IDMAC_CH);
		des_flag |= (i == 0) ? MSHCI_IDMAC_FS : 0;
		if (data_cnt <= 8) {
			des_flag |= MSHCI_IDMAC_LD;
			mshci_set_mdma_desc(pdesc_dmac, pdesc_dmac,
				des_flag, blksz * data_cnt,
				(uintptr_t)data->dest + i * 0x1000);
			break;
		}
		/* max transfer size is 4KB per descriptor */
		mshci_set_mdma_desc(pdesc_dmac, pdesc_dmac, des_flag,
			blksz * 8, (uintptr_t)data->dest + i * 0x1000);

		data_cnt -= 8;
		pdesc_dmac++;
	}

	void *data_start = idmac_desc;
	size_t data_len = (uint8_t *)pdesc_dmac - (uint8_t *)idmac_desc +
			  DMA_MINALIGN;
	dcache_clean_invalidate_by_mva(data_start, data_len);

	data_start = data->dest;
	data_len = data->blocks * data->blocksize;
	dcache_clean_invalidate_by_mva(data_start, data_len);

	write32(&host->regs->dbaddr, (uintptr_t)virt_to_phys(idmac_desc));

	// Enable the Internal DMA Controller.
	setbits32(&host->regs->ctrl, ENABLE_IDMAC | DMA_ENABLE);
	setbits32(&host->regs->bmod, BMOD_IDMAC_ENABLE | BMOD_IDMAC_FB);

	write32(&host->regs->blksiz, data->blocksize);
	write32(&host->regs->bytcnt, data->blocksize * data->blocks);
	return 0;
}

static int mshci_set_transfer_mode(MshciHost *host, MmcData *data)
{
	int mode = CMD_DATA_EXP_BIT;

	if (data->blocks > 1)
		mode |= CMD_SENT_AUTO_STOP_BIT;
	if (data->flags & MMC_DATA_WRITE)
		mode |= CMD_RW_BIT;

	return mode;
}

static int s5p_mshci_send_command(MmcCtrlr *ctrlr, MmcCommand *cmd,
				  MmcData *data)
{
	MshciHost *host = container_of(ctrlr, MshciHost, mmc);

	int flags = 0, i;
	unsigned int mask;

	/*
	 * If auto stop is enabled in the control register, ignore STOP
	 * command, as controller will internally send a STOP command after
	 * every block read/write
	 */
	if ((read32(&host->regs->ctrl) & SEND_AS_CCSD) &&
			(cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION))
		return 0;

	/*
	* We shouldn't wait for data inihibit for stop commands, even
	* though they might use busy signaling
	*/
	if (mmc_busy_wait_io(&host->regs->status, NULL, DATA_BUSY,
			     S5P_MSHC_COMMAND_TIMEOUT)) {
		mmc_error("timeout on data busy\n");
		return -1;
	}

	if ((read32(&host->regs->rintsts) & (INTMSK_CDONE | INTMSK_ACD)) == 0) {
		uint32_t rintsts = read32(&host->regs->rintsts);
		if (rintsts) {
			mmc_debug("there're pending interrupts %#x\n", rintsts);
		}
	}

	// Clear all pending interrupts before sending a command.
	write32(&host->regs->rintsts, INTMSK_ALL);

	if (data) {
		if (mshci_prepare_data(host, data)) {
			mmc_error("fail to prepare data\n");
			return -1;
		}
	}

	write32(&host->regs->cmdarg, cmd->cmdarg);

	if (data)
		flags = mshci_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY)) {
		// Out of SD spec.
		mmc_error("wrong response type or response busy for cmd %d\n",
			  cmd->cmdidx);
		return -1;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		flags |= CMD_RESP_EXP_BIT;
		if (cmd->resp_type & MMC_RSP_136)
			flags |= CMD_RESP_LENGTH_BIT;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= CMD_CHECK_CRC_BIT;
	flags |= (cmd->cmdidx | CMD_STRT_BIT | CMD_USE_HOLD_REG |
		  CMD_WAIT_PRV_DAT_BIT);

	mask = read32(&host->regs->cmd);
	if (mask & CMD_STRT_BIT) {
		mmc_error("cmd busy, current cmd: %d", cmd->cmdidx);
		return -1;
	}

	write32(&host->regs->cmd, flags);
	/* wait for command complete by busy waiting. */
	for (i = 0; i < S5P_MSHC_COMMAND_TIMEOUT; i++) {
		mask = read32(&host->regs->rintsts);
		if (mask & INTMSK_CDONE) {
			if (!data)
				write32(&host->regs->rintsts, mask);
			break;
		}
	}

	/* timeout for command complete. */
	if (S5P_MSHC_COMMAND_TIMEOUT == i) {
		mmc_error("timeout waiting for status update\n");
		return MMC_TIMEOUT;
	}

	if (mask & INTMSK_RTO)
		return MMC_TIMEOUT;
	else if (mask & INTMSK_RE) {
		mmc_error("response mmc_error: %#x cmd: %d\n",
			  mask, cmd->cmdidx);
		return -1;
	}
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			// CRC is stripped so we need to do some shifting.
			cmd->response[0] = read32(&host->regs->resp3);
			cmd->response[1] = read32(&host->regs->resp2);
			cmd->response[2] = read32(&host->regs->resp1);
			cmd->response[3] = read32(&host->regs->resp0);
		} else {
			cmd->response[0] = read32(&host->regs->resp0);
			mmc_debug("\tcmd->response[0]: %#08x\n",
				  cmd->response[0]);
		}
	}

	if (data) {
		if (mmc_busy_wait_io_until(&host->regs->rintsts,
					   &mask,
					   DATA_ERR | DATA_TOUT | INTMSK_DTO,
					   S5P_MSHC_COMMAND_TIMEOUT)) {
			mmc_debug("timeout on data error\n");
			return -1;
		}
		mmc_debug("%s: write32(rintsts, mask)\n", __func__);
		write32(&host->regs->rintsts, mask);

		if (data->flags & MMC_DATA_READ) {
			size_t data_len = data->blocks * data->blocksize;
			dcache_clean_invalidate_by_mva(data->dest, data_len);
		}
		mmc_debug("%s: clrbits32(ctrl, +DMA, +IDMAC)\n", __func__);
		// Disable IDMAC and IDMAC_Interrupts.
		clrbits32(&host->regs->ctrl, DMA_ENABLE | ENABLE_IDMAC);
		if (mask & (DATA_ERR | DATA_TOUT)) {
			mmc_error("error during transfer: %#x\n", mask);
			// mask all interrupt source of IDMAC.
			write32(&host->regs->idinten, 0);
			return -1;
		} else if (mask & INTMSK_DTO) {
			mmc_debug("mshci dma interrupt end\n");
		} else {
			mmc_error("unexpected condition %#x\n", mask);
			return -1;
		}
		clrbits32(&host->regs->ctrl, DMA_ENABLE | ENABLE_IDMAC);
		/* mask all interrupt source of IDMAC */
		write32(&host->regs->idinten, 0);
	}

	/* TODO(alim.akhtar@samsung.com): check why we need this delay */
	udelay(100);
	return 0;
}

static int mshci_clock_onoff(MshciHost *host, int val)
{
	if (val)
		write32(&host->regs->clkena, CLK_ENABLE);
	else
		write32(&host->regs->clkena, CLK_DISABLE);

	write32(&host->regs->cmd, 0);
	write32(&host->regs->cmd, CMD_ONLY_CLK);

	/* wait till command is taken by CIU, when this bit is set
	 * host should not attempted to write to any command registers.
	 */
	if (mmc_busy_wait_io(&host->regs->cmd, NULL, CMD_STRT_BIT,
			     S5P_MSHC_COMMAND_TIMEOUT)) {
		mmc_error("Clock %s has failed.\n ", val ? "ON" : "OFF");
		return -1;
	}

	return 0;
}

static int mshci_change_clock(MshciHost *host, uint32_t clock)
{
	if (clock == host->clock)
		return 0;

	/* If Input clock is higher than maximum mshc clock */
	if (clock > MAX_MSHCI_CLOCK) {
		mmc_debug("Input clock is too high\n");
		clock = MAX_MSHCI_CLOCK;
	}

	/* disable the clock before changing it */
	if (mshci_clock_onoff(host, CLK_DISABLE)) {
		mmc_error("failed to DISABLE clock\n");
		return -1;
	}

	/* Calculate clock division */
	uint32_t sclk_mshc = host->src_hz /
		(((host->clksel_val >> 24) & 0x7) + 1);
	int div;
	for (div = 1 ; div <= 0xff; div++) {
		if (((sclk_mshc / 4) / (2 * div)) <= clock) {
			write32(&host->regs->clkdiv, div);
			break;
		}
	}

	write32(&host->regs->clkdiv, div);

	write32(&host->regs->cmd, 0);
	write32(&host->regs->cmd, CMD_ONLY_CLK);

	/*
	 * wait till command is taken by CIU, when this bit is set
	 * host should not attempted to write to any command registers.
	 */
	if (mmc_busy_wait_io(&host->regs->cmd, NULL, CMD_STRT_BIT,
			     S5P_MSHC_COMMAND_TIMEOUT)) {
		mmc_error("Changing clock has timed out.\n");
		return -1;
	}

	clrbits32(&host->regs->cmd, CMD_SEND_CLK_ONLY);
	if (mshci_clock_onoff(host, CLK_ENABLE)) {
		mmc_error("failed to ENABLE clock\n");
		return -1;
	}

	host->clock = clock;
	return 0;
}

static void s5p_mshci_set_ios(MmcCtrlr *ctrlr)
{
	MshciHost *host = container_of(ctrlr, MshciHost, mmc);

	mmc_debug("bus_width: %x, clock: %d\n",
		  ctrlr->bus_width, ctrlr->bus_hz);
	if (ctrlr->bus_hz > 0 && mshci_change_clock(host, ctrlr->bus_hz))
		mmc_debug("mshci_change_clock failed\n");

	if (ctrlr->bus_width == 8)
		write32(&host->regs->ctype, PORT0_CARD_WIDTH8);
	else if (ctrlr->bus_width == 4)
		write32(&host->regs->ctype, PORT0_CARD_WIDTH4);
	else
		write32(&host->regs->ctype, PORT0_CARD_WIDTH1);

	write32(&host->regs->clksel, host->clksel_val);
}

static void mshci_fifo_init(MshciHost *host)
{
	int fifo_val, fifo_depth, fifo_threshold;

	fifo_val = read32(&host->regs->fifoth);

	fifo_depth = 0x80;
	fifo_threshold = fifo_depth / 2;

	fifo_val &= ~(RX_WMARK | TX_WMARK | MSIZE_MASK);
	fifo_val |= (fifo_threshold | (fifo_threshold << 16) | MSIZE_8);
	write32(&host->regs->fifoth, fifo_val);
}

static int mshci_init(MshciHost *host)
{
	write32(&host->regs->pwren, POWER_ENABLE);

	if (mshci_reset_all(host)) {
		mmc_error("mshci_reset_all() failed\n");
		return -1;
	}

	mshci_fifo_init(host);

	/* clear all pending interrupts */
	write32(&host->regs->rintsts, INTMSK_ALL);

	/* interrupts are not used, disable all */
	write32(&host->regs->intmask, 0);
	return 0;
}

static int s5p_mshci_init(BlockDevCtrlrOps *me)
{
	MshciHost *host = container_of(me, MshciHost, mmc.ctrlr.ops);
	unsigned int ier;

	if (mshci_init(host)) {
		mmc_error("mshci_init() failed\n");
		return -1;
	}

	/* enumerate at 400KHz */
	if (mshci_change_clock(host, 400000)) {
		mmc_error("mshci_change_clock failed\n");
		return -1;
	}

	/* set auto stop command */
	ier = read32(&host->regs->ctrl);
	ier |= SEND_AS_CCSD;
	write32(&host->regs->ctrl, ier);

	/* set 1bit card mode */
	write32(&host->regs->ctype, PORT0_CARD_WIDTH1);

	write32(&host->regs->debnce, 0xfffff);

	/* set bus mode register for IDMAC */
	write32(&host->regs->bmod, BMOD_IDMAC_RESET);

	write32(&host->regs->idinten, 0x0);

	/* set the max timeout for data and response */
	write32(&host->regs->tmout, TMOUT_MAX);

	return 0;
}

static int s5p_mshc_update(BlockDevCtrlrOps *me)
{
	MshciHost *host = container_of(me, MshciHost, mmc.ctrlr.ops);

	if (!host->initialized && s5p_mshci_init(me))
		return -1;

	host->initialized = 1;

	if (host->mmc.slot_type == MMC_SLOT_TYPE_REMOVABLE) {
		// cdetect is active low
		int present = !read32(&host->regs->cdetect);

		if (present && !host->mmc.media) {
			// A card is present and not set up yet. Get it ready.
			if (mmc_setup_media(&host->mmc))
				return -1;
			host->mmc.media->dev.name = "removable s5o mmc";
			host->mmc.media->dev.removable = 1;
			host->mmc.media->dev.ops.read = &block_mmc_read;
			host->mmc.media->dev.ops.write = &block_mmc_write;
			host->mmc.media->dev.ops.new_stream =
						&new_simple_stream;
			host->mmc.media->dev.ops.get_health_info =
				block_mmc_get_health_info;
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
		host->mmc.media->dev.name = "s5p mmc";
		host->mmc.media->dev.removable = 0;
		host->mmc.media->dev.ops.read = &block_mmc_read;
		host->mmc.media->dev.ops.write = &block_mmc_write;
		host->mmc.media->dev.ops.new_stream = &new_simple_stream;
		host->mmc.media->dev.ops.get_health_info =
			block_mmc_get_health_info;
		list_insert_after(&host->mmc.media->dev.list_node,
				  &fixed_block_devices);
		host->mmc.ctrlr.need_update = 0;
	}

	return 0;
}

MshciHost *new_mshci_host(uintptr_t ioaddr, uint32_t src_hz, int bus_width,
			  int removable, uint32_t clksel_val)
{
	MshciHost *ctrlr = xzalloc(sizeof(*ctrlr));

	ctrlr->mmc.ctrlr.ops.is_bdev_owned = block_mmc_is_bdev_owned;
	ctrlr->mmc.ctrlr.ops.update = &s5p_mshc_update;
	ctrlr->mmc.ctrlr.need_update = 1;

	ctrlr->mmc.voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	ctrlr->mmc.f_min = MIN_MSHCI_CLOCK;
	ctrlr->mmc.f_max = MAX_MSHCI_CLOCK;
	ctrlr->mmc.bus_width = bus_width;
	ctrlr->mmc.bus_hz = ctrlr->mmc.f_min;
	ctrlr->mmc.b_max = 65535; // Some controllers use 16-bit regs.
	if (bus_width == 8) {
		ctrlr->mmc.caps |= MMC_CAPS_8BIT;
		ctrlr->mmc.caps &= ~MMC_CAPS_4BIT;
	} else {
		ctrlr->mmc.caps |= MMC_CAPS_4BIT;
		ctrlr->mmc.caps &= ~MMC_CAPS_8BIT;
	}
	ctrlr->mmc.caps |= MMC_CAPS_HS_52MHz | MMC_CAPS_HS | MMC_CAPS_HC;
	ctrlr->mmc.send_cmd = &s5p_mshci_send_command;
	ctrlr->mmc.set_ios = &s5p_mshci_set_ios;
	ctrlr->mmc.slot_type =
		removable ? MMC_SLOT_TYPE_REMOVABLE : MMC_SLOT_TYPE_EMBEDDED;

	ctrlr->regs = (void *)ioaddr;
	ctrlr->src_hz = src_hz;
	ctrlr->clksel_val = clksel_val;

	return ctrlr;
}
