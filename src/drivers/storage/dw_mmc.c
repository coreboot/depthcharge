/*
 * (C) Copyright 2012 SAMSUNG Electronics
 * Jaehoon Chung <jh80.chung@samsung.com>
 * Rajeshawari Shinde <rajeshwari.s@samsung.com>
 * Copyright 2013 Google Inc.  All rights reserved.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,  MA 02111-1307 USA
 *
 */

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <stdint.h>

#include "base/time.h"
#include "drivers/storage/dw_mmc.h"

#define PAGE_SIZE 4096

static int dwmci_wait_reset(DwmciHost *host, uint32_t value)
{
	unsigned long timeout_ms = 10;

	dwmci_writel(host, DWMCI_CTRL, value);
	return !mmc_busy_wait_io(dwmci_get_ioaddr(host, DWMCI_CTRL), NULL,
				 DWMCI_RESET_ALL, timeout_ms);
}

static void dwmci_set_idma_desc(DwmciIdmac *idmac, uint32_t desc0,
				uint32_t desc1, uint32_t desc2)
{
	DwmciIdmac *desc = idmac;

	desc->flags = desc0;
	desc->cnt = desc1;
	desc->addr = desc2;
	desc->next_addr = (unsigned int)desc + sizeof(DwmciIdmac);
}

static void dwmci_prepare_data(DwmciHost *host, MmcData *data)
{
	unsigned long ctrl;
	unsigned int i = 0, flags, cnt, blk_cnt;
	void *data_start;
	void const *start_addr;
	size_t data_len;
	ALLOC_CACHE_ALIGN_BUFFER(DwmciIdmac, cur_idmac, data->blocks);


	blk_cnt = data->blocks;

	dwmci_wait_reset(host, DWMCI_CTRL_FIFO_RESET);

	data_start = cur_idmac;
	dwmci_writel(host, DWMCI_DBADDR, (uintptr_t)cur_idmac);

	if (data->flags == MMC_DATA_READ)
		start_addr = data->dest;
	else
		start_addr = data->src;

	do {
		flags = DWMCI_IDMAC_OWN | DWMCI_IDMAC_CH ;
		flags |= (i == 0) ? DWMCI_IDMAC_FS : 0;
		if (blk_cnt <= 8) {
			flags |= DWMCI_IDMAC_LD;
			cnt = data->blocksize * blk_cnt;
		} else
			cnt = data->blocksize * 8;

		dwmci_set_idma_desc(cur_idmac, flags, cnt,
				(uintptr_t)start_addr + (i * PAGE_SIZE));

		if(blk_cnt < 8)
			break;
		blk_cnt -= 8;
		cur_idmac++;
		i++;
	} while(1);

	data_len = (void *)cur_idmac - data_start;
	dcache_clean_invalidate_by_mva(data_start, data_len + DMA_MINALIGN);

	dcache_clean_invalidate_by_mva(start_addr,
				       (data->blocks * data->blocksize));

	ctrl = dwmci_readl(host, DWMCI_CTRL);
	ctrl |= DWMCI_IDMAC_EN | DWMCI_DMA_EN;
	dwmci_writel(host, DWMCI_CTRL, ctrl);

	ctrl = dwmci_readl(host, DWMCI_BMOD);
	ctrl |= DWMCI_BMOD_IDMAC_FB | DWMCI_BMOD_IDMAC_EN;
	dwmci_writel(host, DWMCI_BMOD, ctrl);

	dwmci_writel(host, DWMCI_BLKSIZ, data->blocksize);
	dwmci_writel(host, DWMCI_BYTCNT, data->blocksize * data->blocks);
}

static int dwmci_set_transfer_mode(DwmciHost *host, MmcData *data)
{
	unsigned long mode;

	mode = DWMCI_CMD_DATA_EXP;
	if (data->flags & MMC_DATA_WRITE)
		mode |= DWMCI_CMD_RW;

	return mode;
}

static int dwmci_send_cmd(MmcDevice *mmc, MmcCommand *cmd, MmcData *data)
{
	DwmciHost *host = (DwmciHost *)mmc->host;
	int flags = 0;
	unsigned int busy_timeout_ms = 100, send_timeout_ms = 10;
	uint32_t mask, ctrl;

	if (mmc_busy_wait_io(dwmci_get_ioaddr(host, DWMCI_STATUS), NULL,
			     DWMCI_BUSY, busy_timeout_ms) != 0) {
		printf("Timeout on data busy\n");
		return MMC_TIMEOUT;
	}

	dwmci_writel(host, DWMCI_RINTSTS, DWMCI_INTMSK_ALL);

	if (data)
		dwmci_prepare_data(host, data);

	dwmci_writel(host, DWMCI_CMDARG, cmd->cmdarg);

	if (data)
		flags = dwmci_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY))
		return -1;

	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		flags |= DWMCI_CMD_ABORT_STOP;
	else
		flags |= DWMCI_CMD_PRV_DAT_WAIT;

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		flags |= DWMCI_CMD_RESP_EXP;
		if (cmd->resp_type & MMC_RSP_136)
			flags |= DWMCI_CMD_RESP_LENGTH;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= DWMCI_CMD_CHECK_CRC;

	flags |= (cmd->cmdidx | DWMCI_CMD_START | DWMCI_CMD_USE_HOLD_REG);

	mmc_debug("Sending CMD%d\n",cmd->cmdidx);

	dwmci_writel(host, DWMCI_CMD, flags);

	mask = 0;
	if (mmc_busy_wait_io_until(
			dwmci_get_ioaddr(host, DWMCI_RINTSTS), &mask,
			DWMCI_INTMSK_CDONE, send_timeout_ms) != 0) {
		mmc_error("CMD%d timeout.\n", cmd->cmdidx);
		return MMC_TIMEOUT;
	}
	if (!data)
		dwmci_writel(host, DWMCI_RINTSTS, mask);

	if (mask & DWMCI_INTMSK_RTO) {
		mmc_debug("Response Timeout..\n");
		return MMC_TIMEOUT;
	} else if (mask & DWMCI_INTMSK_RE) {
		mmc_debug("Response Error..\n");
		return -1;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[0] = dwmci_readl(host, DWMCI_RESP3);
			cmd->response[1] = dwmci_readl(host, DWMCI_RESP2);
			cmd->response[2] = dwmci_readl(host, DWMCI_RESP1);
			cmd->response[3] = dwmci_readl(host, DWMCI_RESP0);
		} else {
			cmd->response[0] = dwmci_readl(host, DWMCI_RESP0);
		}
	}

	if (data) {
		uint32_t error_mask = (DWMCI_DATA_ERR | DWMCI_DATA_TOUT),
			 timeout_ms = 1000;
		mask = 0;
		if (mmc_busy_wait_io_until(
				dwmci_get_ioaddr(host, DWMCI_RINTSTS),
				&mask, error_mask | DWMCI_INTMSK_DTO,
				timeout_ms) != 0) {
			mmc_error("DATA timeout!\n");
			return MMC_TIMEOUT;
		}
		if (mask & error_mask) {
			mmc_error("DATA ERROR!\n");
			return -1;
		}

		dwmci_writel(host, DWMCI_RINTSTS, mask);

		ctrl = dwmci_readl(host, DWMCI_CTRL);
		ctrl &= ~(DWMCI_DMA_EN);
		dwmci_writel(host, DWMCI_CTRL, ctrl);
		if (data->flags & MMC_DATA_READ) {
			unsigned long data_len = data->blocks * data->blocksize;
			/*
			 * TODO(hungte) The buffer is supposed to have padding
			 * to the closest cache line boundary, ex:
			 * data_len = ALIGN(data_end, dcache_get_line_size());
			 */
			dcache_invalidate_by_mva(data->dest, data_len);
		}
	}

	return 0;
}

static int dwmci_setup_bus(DwmciHost *host, uint32_t freq)
{
	uint32_t div;
	int timeout_ms = 10;
	unsigned long sclk;

	if ((freq == host->clock) || (freq == 0))
		return 0;
	/*
	 * If host->mmc_clk didn't define,
	 * then assume that host->bus_hz is source clock value.
	 * host->bus_hz should be set from user.
	 */
	if (host->mmc_clk)
		sclk = host->mmc_clk(host->dev_index);
	else if (host->bus_hz)
		sclk = host->bus_hz;
	else {
		printf("Didn't get source clock value..\n");
		return -1;
	}

	// Round up division.
	div = (sclk + (2 * freq) - 1) / (2 * freq);

	dwmci_writel(host, DWMCI_CLKENA, 0);
	dwmci_writel(host, DWMCI_CLKSRC, 0);

	dwmci_writel(host, DWMCI_CLKDIV, div);
	dwmci_writel(host, DWMCI_CMD, DWMCI_CMD_PRV_DAT_WAIT |
			DWMCI_CMD_UPD_CLK | DWMCI_CMD_START);

	if (mmc_busy_wait_io(dwmci_get_ioaddr(host, DWMCI_CMD),
			     NULL, DWMCI_CMD_START, timeout_ms) != 0) {
		mmc_error("%s: TIMEOUT error!!\n", __func__);
		return -1;
	}

	dwmci_writel(host, DWMCI_CLKENA, DWMCI_CLKEN_ENABLE |
			DWMCI_CLKEN_LOW_PWR);

	dwmci_writel(host, DWMCI_CMD, DWMCI_CMD_PRV_DAT_WAIT |
			DWMCI_CMD_UPD_CLK | DWMCI_CMD_START);

	if (mmc_busy_wait_io(dwmci_get_ioaddr(host, DWMCI_CMD),
			     NULL, DWMCI_CMD_START, timeout_ms) != 0) {
		mmc_error("%s: TIMEOUT error!!\n", __func__);
		return -1;
	}

	host->clock = freq;
	return 0;
}

static void dwmci_set_ios(MmcDevice *mmc)
{
	DwmciHost *host = (DwmciHost *)mmc->host;
	uint32_t ctype;

	mmc_debug("Buswidth = %d, clock: %d\n",mmc->bus_width, mmc->clock);

	dwmci_setup_bus(host, mmc->clock);
	switch (mmc->bus_width) {
	case 8:
		ctype = DWMCI_CTYPE_8BIT;
		break;
	case 4:
		ctype = DWMCI_CTYPE_4BIT;
		break;
	default:
		ctype = DWMCI_CTYPE_1BIT;
		break;
	}

	dwmci_writel(host, DWMCI_CTYPE, ctype);

	if (host->clksel)
		host->clksel(host);
}

static int dwmci_init(MmcDevice *mmc)
{
	DwmciHost *host = (DwmciHost *)mmc->host;
	uint32_t fifo_size, fifoth_val;

	dwmci_writel(host, EMMCP_MPSBEGIN0, 0);
	dwmci_writel(host, EMMCP_SEND0, 0);
	dwmci_writel(host, EMMCP_CTRL0,
		MPSCTRL_SECURE_READ_BIT | MPSCTRL_SECURE_WRITE_BIT |
		MPSCTRL_NON_SECURE_READ_BIT | MPSCTRL_NON_SECURE_WRITE_BIT |
		MPSCTRL_VALID);

	dwmci_writel(host, DWMCI_PWREN, 1);

	if (!dwmci_wait_reset(host, DWMCI_RESET_ALL)) {
		mmc_debug("%s[%d] Fail-reset!!\n",__func__,__LINE__);
		return -1;
	}

	/* Enumerate at 400KHz */
	dwmci_setup_bus(host, mmc->f_min);

	dwmci_writel(host, DWMCI_RINTSTS, 0xFFFFFFFF);
	dwmci_writel(host, DWMCI_INTMASK, 0);

	dwmci_writel(host, DWMCI_TMOUT, 0xFFFFFFFF);

	dwmci_writel(host, DWMCI_IDINTEN, 0);
	dwmci_writel(host, DWMCI_BMOD, 1);

	fifo_size = dwmci_readl(host, DWMCI_FIFOTH);
	fifo_size = ((fifo_size & RX_WMARK_MASK) >> RX_WMARK_SHIFT) + 1;
	if (host->fifoth_val) {
		fifoth_val = host->fifoth_val;
	} else {
		fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_size / 2 - 1) |
			TX_WMARK(fifo_size / 2);
		host->fifoth_val = fifoth_val;
	}
	dwmci_writel(host, DWMCI_FIFOTH, fifoth_val);

	dwmci_writel(host, DWMCI_CLKENA, 0);
	dwmci_writel(host, DWMCI_CLKSRC, 0);

	return 0;
}

int dw_mmc_register(DwmciHost *host, uint32_t max_clk, uint32_t min_clk,
		    int removable, MmcDevice **refresh_list,
		    int (*is_card_present)(MmcDevice *mmc))
{
	MmcDevice *mmc = (MmcDevice *)malloc(sizeof(*mmc));
	BlockDev *block_dev = (BlockDev *)malloc(sizeof(*block_dev));

	assert(mmc && block_dev && host);

	memset(mmc, 0, sizeof(*mmc));
	memset(block_dev, 0, sizeof(*block_dev));

	host->mmc = mmc;

	mmc->host = host;
	mmc->send_cmd = dwmci_send_cmd;
	mmc->set_ios = dwmci_set_ios;
	mmc->init = dwmci_init;
	mmc->is_card_present = is_card_present;

	mmc->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	mmc->f_min = min_clk;
	mmc->f_max = max_clk;
	mmc->host_caps = host->caps;

	if (host->buswidth == 8) {
		mmc->host_caps |= MMC_MODE_8BIT;
		mmc->host_caps &= ~MMC_MODE_4BIT;
	} else {
		mmc->host_caps |= MMC_MODE_4BIT;
		mmc->host_caps &= ~MMC_MODE_8BIT;
	}
	mmc->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_HC;

	const int name_size = 16;
	char *name = (char *)malloc(name_size);
	snprintf(name, name_size, "dwmmc%d", host->dev_index);

	block_dev->name = name;
	block_dev->block_size = 512; // TODO(hungte) Probe correct value.
	block_dev->removable = removable;

	if (removable) {
		block_mmc_register(block_dev, mmc, refresh_list);
		mmc_debug("%s: removable registered (init on refresh)\n", name);
	} else {
		block_mmc_register(block_dev, mmc, NULL);
		if (mmc_init(mmc)) {
			mmc_error("%s: failed to init %s.\n", __func__, name);
			free(name);
			free(mmc);
			free(block_dev);
			return -1;
		}
		list_insert_after(&block_dev->list_node, &fixed_block_devices);
		mmc_debug("%s:  init success (reset OK): %s\n", __func__, name);
	}

	return 0;
}
