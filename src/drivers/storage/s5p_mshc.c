/*
 * (C) Copyright 2012 Samsung Electronics Co. Ltd
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

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/init_funcs.h"
#include "config.h"
#include "arch/cache.h"
#include "drivers/storage/exynos_mshc.h"
#include "drivers/storage/mmc.h"

static void setbits32(uint32_t *data, uint32_t bits)
{
	writel(readl(data) | bits, data);
}

static void clrbits32(uint32_t *data, uint32_t bits)
{
	writel(readl(data) & ~bits, data);
}

static inline S5pMshci *s5p_get_base_mshci(int dev_index)
{
	switch(dev_index) {
		case 0:
			return ((S5pMshci *)
				CONFIG_DRIVER_STORAGE_MSHC_S5P_MMC0_ADDRESS);
		case 1:
			return ((S5pMshci *)
				CONFIG_DRIVER_STORAGE_MSHC_S5P_MMC1_ADDRESS);
	}
	mmc_error("Unknown device index (%d): Check your Kconfig settings for "
	      "DRIVER_MSHC_S5P_DEVICES.\n", dev_index);
	assert(dev_index < CONFIG_DRIVER_STORAGE_MSHC_S5P_DEVICES);
	return NULL;
}

static int s5p_mshci_is_card_present(S5pMshci *reg)
{
	return !readl(&reg->cdetect);
}

static int mshci_setbits(MshciHost *host, unsigned int bits)
{
	setbits32(&host->reg->ctrl, bits);
	if (mmc_busy_wait_io(&host->reg->ctrl, NULL, bits,
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
	if (mmc_busy_wait_io(&host->reg->status, NULL, DATA_BUSY,
			     S5P_MSHC_TIMEOUT_MS)) {
		mmc_error("Controller did not release data0 before ciu reset\n");
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

static void mshci_set_mdma_desc(uint8_t *desc_vir, uint8_t *desc_phy,
				unsigned int des0, unsigned int des1,
				unsigned int des2)
{
	MshciIdmac *desc = (MshciIdmac *)desc_vir;

	desc->des0 = des0;
	desc->des1 = des1;
	desc->des2 = des2;
	desc->des3 = (unsigned int)desc_phy + sizeof(MshciIdmac);
}

static int mshci_prepare_data(MshciHost *host, MmcData *data)
{
	unsigned int i;
	unsigned int data_cnt;
	unsigned int des_flag;
	unsigned int blksz;
	static MshciIdmac idmac_desc[0x10000];
	/* TODO(alim.akhtar@samsung.com): do we really need this big array? */

	MshciIdmac *pdesc_dmac;

	if (mshci_setbits(host, FIFO_RESET)) {
		mmc_error("Fail to reset FIFO\n");
		return -1;
	}

	pdesc_dmac = idmac_desc;
	blksz = data->blocksize;
	data_cnt = data->blocks;

	for  (i = 0;; i++) {
		des_flag = (MSHCI_IDMAC_OWN | MSHCI_IDMAC_CH);
		des_flag |= (i == 0) ? MSHCI_IDMAC_FS : 0;
		if (data_cnt <= 8) {
			des_flag |= MSHCI_IDMAC_LD;
			mshci_set_mdma_desc((uint8_t *)pdesc_dmac,
			(uint8_t *)virt_to_phys(pdesc_dmac),
			des_flag, blksz * data_cnt,
			(unsigned int)(virt_to_phys(data->dest)) +
			(unsigned int)(i * 0x1000));
			break;
		}
		/* max transfer size is 4KB per descriptor */
		mshci_set_mdma_desc((uint8_t *)pdesc_dmac,
			(uint8_t *)virt_to_phys(pdesc_dmac),
			des_flag, blksz * 8,
			virt_to_phys(data->dest) +
			(unsigned int)(i * 0x1000));

		data_cnt -= 8;
		pdesc_dmac++;
	}

	uint32_t data_start, data_len;
	data_start = (uintptr_t)idmac_desc;
	data_len = (uintptr_t)pdesc_dmac - (uintptr_t)idmac_desc + DMA_MINALIGN;
	dcache_clean_invalidate_by_mva(data_start, data_len);

	data_start = (uint32_t)data->dest;
	data_len  = (uint32_t)(data->blocks * data->blocksize);
	dcache_clean_invalidate_by_mva(data_start, data_len);

	writel((unsigned int)virt_to_phys(idmac_desc), &host->reg->dbaddr);

	/* enable the Internal DMA Controller */
	setbits32(&host->reg->ctrl, ENABLE_IDMAC | DMA_ENABLE);
	setbits32(&host->reg->bmod, BMOD_IDMAC_ENABLE | BMOD_IDMAC_FB);

	writel(data->blocksize, &host->reg->blksiz);
	writel(data->blocksize * data->blocks, &host->reg->bytcnt);
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

static int s5p_mshci_send_command(MmcDevice *mmc, MmcCommand *cmd,
				  MmcData *data)
{
	MshciHost *host = (MshciHost *)mmc->host;

	int flags = 0, i;
	unsigned int mask;

	/*
	 * If auto stop is enabled in the control register, ignore STOP
	 * command, as controller will internally send a STOP command after
	 * every block read/write
	 */
	if ((readl(&host->reg->ctrl) & SEND_AS_CCSD) &&
			(cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION))
		return 0;

	/*
	* We shouldn't wait for data inihibit for stop commands, even
	* though they might use busy signaling
	*/
	if (mmc_busy_wait_io(&host->reg->status, NULL, DATA_BUSY,
			     S5P_MSHC_COMMAND_TIMEOUT)) {
		mmc_error("timeout on data busy\n");
		return -1;
	}

	if ((readl(&host->reg->rintsts) & (INTMSK_CDONE | INTMSK_ACD)) == 0) {
		uint32_t rintsts = readl(&host->reg->rintsts);
		if (rintsts) {
			mmc_debug("there're pending interrupts %#x\n", rintsts);
		}
	}

	/* It clears all pending interrupts before sending a command*/
	writel(INTMSK_ALL, &host->reg->rintsts);

	if (data) {
		if (mshci_prepare_data(host, data)) {
			mmc_error("fail to prepare data\n");
			return -1;
		}
	}

	writel(cmd->cmdarg, &host->reg->cmdarg);

	if (data)
		flags = mshci_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY)) {
		/* this is out of SD spec */
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

	mask = readl(&host->reg->cmd);
	if (mask & CMD_STRT_BIT) {
		mmc_error("cmd busy, current cmd: %d", cmd->cmdidx);
		return -1;
	}

	writel(flags, &host->reg->cmd);
	/* wait for command complete by busy waiting. */
	for (i = 0; i < S5P_MSHC_COMMAND_TIMEOUT; i++) {
		mask = readl(&host->reg->rintsts);
		if (mask & INTMSK_CDONE) {
			if (!data)
				writel(mask, &host->reg->rintsts);
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
		mmc_error("response mmc_error: %#x cmd: %d\n", mask, cmd->cmdidx);
		return -1;
	}
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			/* CRC is stripped so we need to do some shifting. */
			cmd->response[0] = readl(&host->reg->resp3);
			cmd->response[1] = readl(&host->reg->resp2);
			cmd->response[2] = readl(&host->reg->resp1);
			cmd->response[3] = readl(&host->reg->resp0);
		} else {
			cmd->response[0] = readl(&host->reg->resp0);
			mmc_debug("\tcmd->response[0]: %#08x\n",
				  cmd->response[0]);
		}
	}

	if (data) {
		if (mmc_busy_wait_io_until(&host->reg->rintsts,
					   &mask,
					   DATA_ERR | DATA_TOUT | INTMSK_DTO,
					   S5P_MSHC_COMMAND_TIMEOUT)) {
			mmc_debug("timeout on data error\n");
			return -1;
		}
		mmc_debug("%s: writel(mask, rintsts)\n", __func__);
		writel(mask, &host->reg->rintsts);

		if (data->flags & MMC_DATA_READ) {
			uint32_t data_base, data_len;
			data_base = (uint32_t)data->dest;
			data_len = data->blocks * data->blocksize;
			dcache_clean_invalidate_by_mva(data_base, data_len);
		}
		mmc_debug("%s: clrbits32(ctrl, +DMA, +IDMAC)\n", __func__);
		/* make sure disable IDMAC and IDMAC_Interrupts */
		clrbits32(&host->reg->ctrl, DMA_ENABLE | ENABLE_IDMAC);
		if (mask & (DATA_ERR | DATA_TOUT)) {
			mmc_error("error during transfer: %#x\n", mask);
			/* mask all interrupt source of IDMAC */
			writel(0, &host->reg->idinten);
			return -1;
		} else if (mask & INTMSK_DTO) {
			mmc_debug("mshci dma interrupt end\n");
		} else {
			mmc_error("unexpected condition %#x\n", mask);
			return -1;
		}
		clrbits32(&host->reg->ctrl, DMA_ENABLE | ENABLE_IDMAC);
		/* mask all interrupt source of IDMAC */
		writel(0, &host->reg->idinten);
	}

	/* TODO(alim.akhtar@samsung.com): check why we need this delay */
	udelay(100);
	return 0;
}

static int mshci_clock_onoff(MshciHost *host, int val)
{
	if (val)
		writel(CLK_ENABLE, &host->reg->clkena);
	else
		writel(CLK_DISABLE, &host->reg->clkena);

	writel(0, &host->reg->cmd);
	writel(CMD_ONLY_CLK, &host->reg->cmd);

	/* wait till command is taken by CIU, when this bit is set
	 * host should not attempted to write to any command registers.
	 */
	if (mmc_busy_wait_io(&host->reg->cmd, NULL, CMD_STRT_BIT,
			     S5P_MSHC_COMMAND_TIMEOUT)) {
		mmc_error("Clock %s has failed.\n ", val ? "ON" : "OFF");
		return -1;
	}

	return 0;
}

static int mshci_change_clock(MshciHost *host, uint32_t clock)
{
	int div;
	uint32_t sclk_mshc;

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
	sclk_mshc = CONFIG_DRIVER_STORAGE_MSHC_S5P_CLOCK_RATE;
	for (div = 1 ; div <= 0xff; div++) {
		if (((sclk_mshc / 4) / (2 * div)) <= clock) {
			writel(div, &host->reg->clkdiv);
			break;
		}
	}

	writel(div, &host->reg->clkdiv);

	writel(0, &host->reg->cmd);
	writel(CMD_ONLY_CLK, &host->reg->cmd);

	/*
	 * wait till command is taken by CIU, when this bit is set
	 * host should not attempted to write to any command registers.
	 */
	if (mmc_busy_wait_io(&host->reg->cmd, NULL, CMD_STRT_BIT,
			     S5P_MSHC_COMMAND_TIMEOUT)) {
		mmc_error("Changing clock has timed out.\n");
		return -1;
	}

	clrbits32(&host->reg->cmd, CMD_SEND_CLK_ONLY);
	if (mshci_clock_onoff(host, CLK_ENABLE)) {
		mmc_error("failed to ENABLE clock\n");
		return -1;
	}

	host->clock = clock;
	return 0;
}

static void s5p_mshci_set_ios(MmcDevice *mmc)
{
	MshciHost *host = (MshciHost *)mmc->host;

	mmc_debug("bus_width: %x, clock: %d\n", mmc->bus_width, mmc->clock);
	if (mmc->clock > 0 && mshci_change_clock(host, mmc->clock))
		mmc_debug("mshci_change_clock failed\n");

	if (mmc->bus_width == 8)
		writel(PORT0_CARD_WIDTH8, &host->reg->ctype);
	else if (mmc->bus_width == 4)
		writel(PORT0_CARD_WIDTH4, &host->reg->ctype);
	else
		writel(PORT0_CARD_WIDTH1, &host->reg->ctype);

	/* TODO(hungte) Put these into macro or config variables. */
	if (host->dev_index == 0)
		writel(0x03030001, &host->reg->clksel);
	else if (host->dev_index == 1)
		writel(0x03020001, &host->reg->clksel);
}

static void mshci_fifo_init(MshciHost *host)
{
	int fifo_val, fifo_depth, fifo_threshold;

	fifo_val = readl(&host->reg->fifoth);

	fifo_depth = 0x80;
	fifo_threshold = fifo_depth / 2;

	fifo_val &= ~(RX_WMARK | TX_WMARK | MSIZE_MASK);
	fifo_val |= (fifo_threshold | (fifo_threshold << 16) | MSIZE_8);
	writel(fifo_val, &host->reg->fifoth);
}

static int mshci_is_card_present(MmcDevice *mmc)
{
	BlockDev *block_dev = (BlockDev *)mmc->block_dev;
	MshciHost *host = (MshciHost *)mmc->host;
	assert(block_dev);

	mmc_debug("%s\n", __func__);
	if (!block_dev->removable)
		return 1;
	return s5p_mshci_is_card_present(host->reg);
}

static int mshci_init(MshciHost *host)
{
	writel(POWER_ENABLE, &host->reg->pwren);

	if (mshci_reset_all(host)) {
		mmc_error("mshci_reset_all() failed\n");
		return -1;
	}

	mshci_fifo_init(host);

	/* clear all pending interrupts */
	writel(INTMSK_ALL, &host->reg->rintsts);

	/* interrupts are not used, disable all */
	writel(0, &host->reg->intmask);
	return 0;
}

static int s5p_mshci_init(MmcDevice *mmc)
{
	MshciHost *host = (MshciHost *)mmc->host;
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
	ier = readl(&host->reg->ctrl);
	ier |= SEND_AS_CCSD;
	writel(ier, &host->reg->ctrl);

	/* set 1bit card mode */
	writel(PORT0_CARD_WIDTH1, &host->reg->ctype);

	writel(0xfffff, &host->reg->debnce);

	/* set bus mode register for IDMAC */
	writel(BMOD_IDMAC_RESET, &host->reg->bmod);

	writel(0x0, &host->reg->idinten);

	/* set the max timeout for data and response */
	writel(TMOUT_MAX, &host->reg->tmout);

	return 0;
}

static int s5p_mshc_initialize(int dev_index, S5pMshci *reg,
			       MmcDevice **refresh_list)
{
	int bus_width = 1;
	const int name_size = 16;
	char *name;

	MshciHost *host = (MshciHost *)malloc(sizeof(MshciHost));
	MmcDevice *mmc_dev = (MmcDevice *)malloc(sizeof(MmcDevice));
	BlockDev *mshc_dev = (BlockDev *)malloc(sizeof(BlockDev));

	mmc_debug("%s: init device #%d using reg %p\n", __func__, dev_index, reg);

	memset(host, 0, sizeof(*host));
	memset(mmc_dev, 0, sizeof(*mmc_dev));
	memset(mshc_dev, 0, sizeof(*mshc_dev));

	host->clock = 0;
	host->reg = reg;
	host->dev_index = dev_index;

	mmc_dev->host = host;
	mmc_dev->send_cmd = s5p_mshci_send_command;
	mmc_dev->set_ios = s5p_mshci_set_ios;
	mmc_dev->init = s5p_mshci_init;
	mmc_dev->is_card_present = mshci_is_card_present;

	mmc_dev->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc_dev->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC;
	mmc_dev->f_min = MIN_MSHCI_CLOCK;
	mmc_dev->f_max = MAX_MSHCI_CLOCK;

	name = malloc(name_size);
	snprintf(name, name_size, "s5p_mshc%d", dev_index);
	mshc_dev->name = name;
	mshc_dev->block_size = reg->blksiz;

	/* TODO(hungte) Get configuration data instead of hard-coded. */
	if (dev_index == 0) {
		bus_width = 8;
		mshc_dev->removable = 0;
		mmc_dev->host_caps |= MMC_MODE_8BIT;
	} else {
		bus_width = 4;
		mshc_dev->removable = 1;
		mmc_dev->host_caps |= MMC_MODE_4BIT;
	}
	mmc_dev->bus_width = bus_width;

	if (mshc_dev->removable) {
		block_mmc_register(mshc_dev, mmc_dev, refresh_list);
		mmc_debug("%s: removable registered (init on refresh).\n", name);
	} else {
		block_mmc_register(mshc_dev, mmc_dev, NULL);
		if (mmc_init(mmc_dev)) {
			mmc_error("%s: init failed: %s.\n", __func__, name);
			free(name);
			free(mmc_dev);
			free(mshc_dev);
			free(host);
			return -1;
		}
		list_insert_after(&mshc_dev->list_node, &fixed_block_devices);
		mmc_debug("%s:  init success (reset OK): %s\n", __func__, name);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Depthcharge block device I/O

static void s5p_mshc_ctrlr_init(BlockDevCtrlr *ctrlr)
{
	int i;
	MmcDevice **refresh_list = (MmcDevice **)(&ctrlr->ctrlr_data);

	mmc_debug("%s started.\n", __func__);
	for (i = 0; i < CONFIG_DRIVER_STORAGE_MSHC_S5P_DEVICES; i++) {
		s5p_mshc_initialize(i, s5p_get_base_mshci(i), refresh_list);
	}
}

static void s5p_mshc_ctrlr_refresh(BlockDevCtrlr *ctrlr)
{
	MmcDevice *mmc = (MmcDevice *)ctrlr->ctrlr_data;
	mmc_debug("%s: enter (root=%p).\n", __func__, mmc);
	for (; mmc; mmc = mmc->next) {
		block_mmc_refresh(&removable_block_devices, mmc);
	}
	mmc_debug("%s: leave.\n", __func__);
}

int s5p_mshc_ctrlr_register(void)
{
	static BlockDevCtrlr s5p_mshc =
	{
		&s5p_mshc_ctrlr_init,
		&s5p_mshc_ctrlr_refresh,
		NULL,
	};
	list_insert_after(&s5p_mshc.list_node, &block_dev_controllers);
	mmc_debug("%s: done.\n", __func__);
	return 0;
}

INIT_FUNC(s5p_mshc_ctrlr_register);
