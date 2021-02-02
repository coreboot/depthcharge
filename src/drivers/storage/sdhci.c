/*
 * Copyright 2011, Marvell Semiconductor Inc.
 * Lei Wen <leiwen@marvell.com>
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
 * Back ported to the 8xx platform (from the 8260 platform) by
 * Murray.Jensen@cmst.csiro.au, 27-Jan-01.
 */

#include <assert.h>
#include <libpayload.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/mmc.h"
#include "drivers/storage/sdhci.h"

static void sdhci_reset(SdhciHost *host, u8 mask)
{
	unsigned long timeout;

	/* Wait max 100 ms */
	timeout = 100;
	sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);
	while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			printf("Reset 0x%x never completed.\n", (int)mask);
			return;
		}
		timeout--;
		udelay(1000);
	}
}

static void sdhci_cmd_done(SdhciHost *host, MmcCommand *cmd)
{
	int i;
	if (cmd->resp_type & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0; i < 4; i++) {
			cmd->response[i] = sdhci_readl(host,
					SDHCI_RESPONSE + (3-i)*4) << 8;
			if (i != 3)
				cmd->response[i] |= sdhci_readb(host,
						SDHCI_RESPONSE + (3-i)*4-1);
		}
	} else {
		cmd->response[0] = sdhci_readl(host, SDHCI_RESPONSE);
	}
}

static void sdhci_transfer_pio(SdhciHost *host, MmcData *data)
{
	int i;
	char *offs;
	for (i = 0; i < data->blocksize; i += 4) {
		offs = data->dest + i;
		if (data->flags == MMC_DATA_READ)
			*(u32 *)offs = sdhci_readl(host, SDHCI_BUFFER);
		else
			sdhci_writel(host, *(u32 *)offs, SDHCI_BUFFER);
	}
}

static int sdhci_transfer_data(SdhciHost *host, MmcData *data,
			       unsigned int start_addr)
{
	unsigned int stat, rdy, mask, timeout, block = 0;

	timeout = 1000000;
	rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
	mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) {
			printf("Error detected in status(0x%X)!\n", stat);
			return -1;
		}
		if (stat & rdy) {
			if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & mask))
				continue;
			sdhci_writel(host, rdy, SDHCI_INT_STATUS);
			sdhci_transfer_pio(host, data);
			data->dest += data->blocksize;
			if (++block >= data->blocks)
				break;
		}
		if (timeout-- > 0)
			udelay(10);
		else {
			printf("Transfer data timeout\n");
			return -1;
		}
	} while (!(stat & SDHCI_INT_DATA_END));
	return 0;
}

static void sdhci_alloc_adma_descs(SdhciHost *host, u32 need_descriptors)
{
	if (host->adma_descs) {
		if (host->adma_desc_count < need_descriptors) {
			/* Previously allocated array is too small */
			free(host->adma_descs);
			host->adma_desc_count = 0;
			host->adma_descs = NULL;
		}
	}

	/* use dma_malloc() to make sure we get the coherent/uncached memory */
	if (!host->adma_descs) {
		host->adma_descs = dma_malloc(need_descriptors *
					      sizeof(*host->adma_descs));
		if (host->adma_descs == NULL)
			die("fail to malloc adma_descs\n");
		host->adma_desc_count = need_descriptors;
	}

	memset(host->adma_descs, 0, sizeof(*host->adma_descs) *
	       need_descriptors);
}

static void sdhci_alloc_adma64_descs(SdhciHost *host, u32 need_descriptors)
{
	if (host->adma64_descs) {
		if (host->adma_desc_count < need_descriptors) {
			/* Previously allocated array is too small */
			free(host->adma64_descs);
			host->adma_desc_count = 0;
			host->adma64_descs = NULL;
		}
	}

	/* use dma_malloc() to make sure we get the coherent/uncached memory */
	if (!host->adma64_descs) {
		host->adma64_descs = dma_malloc(need_descriptors *
					   sizeof(*host->adma64_descs));
		if (host->adma64_descs == NULL)
			die("fail to malloc adma64_descs\n");

		host->adma_desc_count = need_descriptors;
	}

	memset(host->adma64_descs, 0, sizeof(*host->adma64_descs) *
	       need_descriptors);
}

static int sdhci_setup_adma(SdhciHost *host, MmcData *data,
			    struct bounce_buffer *bbstate)
{
	int i, togo, need_descriptors;
	char *buffer_data;
	u16 attributes;

	togo = data->blocks * data->blocksize;
	if (!togo) {
		printf("%s: MmcData corrupted: %d blocks of %d bytes\n",
		       __func__, data->blocks, data->blocksize);
		return -1;
	}

	need_descriptors = 1 +  togo / SDHCI_MAX_PER_DESCRIPTOR;

	if (host->dma64)
		sdhci_alloc_adma64_descs(host, need_descriptors);
	else
		sdhci_alloc_adma_descs(host, need_descriptors);

	if (bbstate)
		buffer_data = (char *)bbstate->bounce_buffer;
	else
		buffer_data = data->dest;

	/* Now set up the descriptor chain. */
	for (i = 0; togo; i++) {
		unsigned desc_length;

		if (togo < SDHCI_MAX_PER_DESCRIPTOR)
			desc_length = togo;
		else
			desc_length = SDHCI_MAX_PER_DESCRIPTOR;
		togo -= desc_length;

		attributes = SDHCI_ADMA_VALID | SDHCI_ACT_TRAN;
		if (togo == 0)
			attributes |= SDHCI_ADMA_END;

		if (host->dma64) {
			uintptr_t lo = (uintptr_t) buffer_data & 0xFFFFFFFF;
			uintptr_t hi = 0;

			host->adma64_descs[i].addr = lo;
			host->adma64_descs[i].addr_hi = hi;
			host->adma64_descs[i].length = desc_length;
			host->adma64_descs[i].attributes = attributes;

		} else {
			host->adma_descs[i].addr = (uintptr_t) buffer_data;
			host->adma_descs[i].length = desc_length;
			host->adma_descs[i].attributes = attributes;
		}

		buffer_data += desc_length;
	}

	if (host->dma64) {
		uintptr_t lo = (uintptr_t) host->adma64_descs & 0xFFFFFFFF;
		uintptr_t hi = 0;

		sdhci_writel(host, lo, SDHCI_ADMA_ADDRESS);
		sdhci_writel(host, hi, SDHCI_ADMA_ADDRESS_HI);
	} else {
		sdhci_writel(host, (uintptr_t) host->adma_descs,
			     SDHCI_ADMA_ADDRESS);
	}

	return 0;
}

static int sdhci_complete_adma(SdhciHost *host, MmcCommand *cmd)
{
	int retry;
	u32 stat = 0, mask;

	mask = SDHCI_INT_RESPONSE | SDHCI_INT_ERROR;

	retry = 10000; /* Command should be done in way less than 10 ms. */
	while (--retry) {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & mask)
			break;
		udelay(1);
	}

	sdhci_writel(host, SDHCI_INT_RESPONSE, SDHCI_INT_STATUS);

	if (retry && !(stat & SDHCI_INT_ERROR)) {
		/* Command OK, let's wait for data transfer completion. */
		mask = SDHCI_INT_DATA_END |
			SDHCI_INT_ERROR | SDHCI_INT_ADMA_ERROR;

		/* Transfer should take 10 seconds tops. */
		retry = 10 * 1000 * 1000;
		while (--retry) {
			stat = sdhci_readl(host, SDHCI_INT_STATUS);
			if (stat & mask)
				break;
			udelay(1);
		}

		sdhci_writel(host, stat, SDHCI_INT_STATUS);
		if (retry && !(stat & SDHCI_INT_ERROR)) {
			sdhci_cmd_done(host, cmd);
			return 0;
		}
	}

	printf("%s: transfer error, stat %#x, adma error %#x, retry %d\n",
	       __func__, stat, sdhci_readl(host, SDHCI_ADMA_ERROR), retry);

	sdhci_reset(host, SDHCI_RESET_CMD);
	sdhci_reset(host, SDHCI_RESET_DATA);

	if (stat & SDHCI_INT_TIMEOUT)
		return MMC_TIMEOUT;
	else
		return MMC_COMM_ERR;
}

/* Send eMMC HS200 tuning command */
int sdhci_send_hs200_tuning_cmd(MmcCtrlr *mmc_ctrlr)
{
	SdhciHost *sdhc = container_of(mmc_ctrlr, SdhciHost, mmc_ctrlr);
	int block_size;
	u32 stat;
	uint64_t start;
	int ret = 0;

	switch (mmc_ctrlr->bus_width) {
	case 8:
		block_size = 128;
		break;
	case 4:
		block_size = 64;
		break;
	default:
		printf("Unsupported bus width: %d\n", mmc_ctrlr->bus_width);
		return MMC_SUPPORT_ERR;
	}

	sdhci_writel(sdhc, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);

	sdhci_writew(sdhc,
		     SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG, block_size),
		     SDHCI_BLOCK_SIZE);

	sdhci_writew(sdhc, 1, SDHCI_BLOCK_COUNT);

	sdhci_writew(sdhc, SDHCI_TRNS_READ, SDHCI_TRANSFER_MODE);

	sdhci_writel(sdhc, 0, SDHCI_ARGUMENT);

	sdhci_writew(sdhc,
		     SDHCI_MAKE_CMD(MMC_SEND_TUNING_BLOCK_HS200,
				    (SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC |
				     SDHCI_CMD_INDEX | SDHCI_CMD_DATA)),
		     SDHCI_COMMAND);

	start = timer_us(0);
	do {
		stat = sdhci_readl(sdhc, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) {
			printf("SDHCI controller error!\n");
			ret = MMC_COMM_ERR;
			break;
		}

		if (timer_us(start) > SDHCI_TUNING_MAX_US) {
			printf("Command timeout!\n");
			ret = MMC_TIMEOUT;
			break;
		}
	} while (!(stat & SDHCI_INT_DATA_AVAIL));

	sdhci_writel(sdhc, stat, SDHCI_INT_STATUS);

	return ret;
}

int sdhci_execute_tuning(struct MmcMedia *media)
{
	int ret;
	u16 reg;
	u32 ctrl;
	uint64_t start;
	unsigned int blocks = 0;

	SdhciHost *host = container_of(media->ctrlr, SdhciHost, mmc_ctrlr);

	host->tuned_clock = 0;

	if (media->ctrlr->timing != MMC_TIMING_MMC_HS200) {
		printf("%s: Tuning only supports HS200\n", host->name);
		return MMC_SUPPORT_ERR;
	}

	/* Start tuning */
	reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	reg |= SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);

	/* Wait for tuning to finish */
	start = timer_us(0);
	while (1) {
		blocks++;

		ret = sdhci_send_hs200_tuning_cmd(media->ctrlr);
		if (ret) {
			printf("%s: Failed to send tuning command\n",
			       host->name);
			goto tuning_failed;
		}

		ctrl = sdhci_readl(host, SDHCI_HOST_CONTROL2);
		if (!(ctrl & SDHCI_CTRL_EXEC_TUNING))
			break;

		if (timer_us(start) > SDHCI_TUNING_MAX_US) {
			printf("%s: Tuning timed out\n", host->name);
			ret = MMC_TIMEOUT;
			goto tuning_failed;
		}
	}

	if (!(ctrl & SDHCI_CTRL_TUNED_CLK)) {
		printf("%s: HW tuning failed\n", host->name);
		ret = MMC_UNUSABLE_ERR;
		goto tuning_failed;
	}

	printf("%s: Tuning complete after %llu us, %d blocks\n", host->name,
	       timer_us(start), blocks);

	host->tuned_clock = host->clock;
	return 0;

 tuning_failed:
	printf("%s: Tuning failed after %llu us, %d blocks\n", host->name,
	       timer_us(start), blocks);

	/* Tuning has timed out or failed. */
	reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	reg &= ~SDHCI_CTRL_TUNED_CLK;
	reg &= ~SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);

	return ret;
}

static int sdhci_send_command_bounced(MmcCtrlr *mmc_ctrl, MmcCommand *cmd,
				      MmcData *data,
				      struct bounce_buffer *bbstate)
{
	unsigned int stat = 0;
	int ret = 0;
	u32 mask, flags;
	unsigned int timeout, start_addr = 0;
	uint64_t start;
	SdhciHost *host = container_of(mmc_ctrl, SdhciHost, mmc_ctrlr);

	/* Wait max 1 s */
	timeout = 1000;

	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	   though they might use busy signaling */
	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		mask &= ~SDHCI_DATA_INHIBIT;

	while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
		if (timeout == 0) {
			printf("Controller never released inhibit bit(s), "
			       "present state %#8.8x.\n",
			       sdhci_readl(host, SDHCI_PRESENT_STATE));
			return MMC_COMM_ERR;
		}
		timeout--;
		udelay(1000);
	}

	mask = SDHCI_INT_RESPONSE;
	if (!(cmd->resp_type & MMC_RSP_PRESENT))
		flags = SDHCI_CMD_RESP_NONE;
	else if (cmd->resp_type & MMC_RSP_136)
		flags = SDHCI_CMD_RESP_LONG;
	else if (cmd->resp_type & MMC_RSP_BUSY) {
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SDHCI_INT_DATA_END;
	} else
		flags = SDHCI_CMD_RESP_SHORT;

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= SDHCI_CMD_CRC;
	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= SDHCI_CMD_INDEX;
	if (data)
		flags |= SDHCI_CMD_DATA;

	/* Set Transfer mode regarding to data flag */
	if (data) {
		u16 mode = 0;

		sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
						    data->blocksize),
			     SDHCI_BLOCK_SIZE);

		if (data->flags == MMC_DATA_READ)
			mode |= SDHCI_TRNS_READ;

		if (data->blocks > 1)
			mode |= SDHCI_TRNS_BLK_CNT_EN |
				SDHCI_TRNS_MULTI | SDHCI_TRNS_ACMD12;

		sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);

		if (host->host_caps & MMC_CAPS_AUTO_CMD12) {
			if (sdhci_setup_adma(host, data, bbstate))
				return -1;

			mode |= SDHCI_TRNS_DMA;
		}
		sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
	} else {
		/* Quirk: Some AMD chipsets require the cleraring the
		 * transfer mode 0 before sending a command without data.
		 * Commands with data always set the transfer mode */
		if (host->quirks & SDHCI_QUIRK_CLEAR_TRANSFER_BEFORE_CMD)
			sdhci_writew(host, 0, SDHCI_TRANSFER_MODE);
	}

	sdhci_writel(host, cmd->cmdarg, SDHCI_ARGUMENT);
	sdhci_writew(host, SDHCI_MAKE_CMD(cmd->cmdidx, flags), SDHCI_COMMAND);

	if (data && (host->host_caps & MMC_CAPS_AUTO_CMD12))
		return sdhci_complete_adma(host, cmd);

	start = timer_us(0);
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR)
			break;

		/* Apply max timeout for R1b-type CMD defined in eMMC ext_csd
		   except for erase ones */
		if (timer_us(start) > 2550000) {
			if (host->quirks & SDHCI_QUIRK_BROKEN_R1B)
				return 0;
			else {
				printf("Timeout for status update!\n");
				return MMC_TIMEOUT;
			}
		}
	} while ((stat & mask) != mask);

	if ((stat & (SDHCI_INT_ERROR | mask)) == mask) {
		sdhci_cmd_done(host, cmd);
		sdhci_writel(host, mask, SDHCI_INT_STATUS);
	} else
		ret = -1;

	if (!ret && data)
		ret = sdhci_transfer_data(host, data, start_addr);

	if (host->quirks & SDHCI_QUIRK_WAIT_SEND_CMD)
		udelay(1000);

	stat = sdhci_readl(host, SDHCI_INT_STATUS);
	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);

	if (!ret)
		return 0;

	sdhci_reset(host, SDHCI_RESET_CMD);
	sdhci_reset(host, SDHCI_RESET_DATA);
	if (stat & SDHCI_INT_TIMEOUT)
		return MMC_TIMEOUT;
	else
		return MMC_COMM_ERR;
}

static int sdhci_send_command(MmcCtrlr *mmc_ctrl, MmcCommand *cmd,
			      MmcData *data)
{
	void *buf;
	unsigned int bbflags;
	size_t len;
	struct bounce_buffer *bbstate = NULL;
	struct bounce_buffer bbstate_val;
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

		/*
		 * on some platform(like rk3399 etc) need to worry about
		 * cache coherency, so check the buffer, if not dma
		 * coherent, use bounce_buffer to do DMA management.
		 */
		if (!dma_coherent(buf)) {
			bbstate = &bbstate_val;
			if (bounce_buffer_start(bbstate, buf, len, bbflags)) {
				printf("ERROR: Failed to get bounce buffer.\n");
				return -1;
			}
		}
	}

	ret = sdhci_send_command_bounced(mmc_ctrl, cmd, data, bbstate);

	if (bbstate)
		bounce_buffer_stop(bbstate);

	return ret;
}

static int sdhci_set_clock(MmcCtrlr *mmc_ctrlr, unsigned int clock)
{
	unsigned int div, clk, timeout;
	enum mmc_timing timing = mmc_ctrlr->timing;
	SdhciHost *host = container_of(mmc_ctrlr,
				       SdhciHost, mmc_ctrlr);

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return 0;

	/* Quirk: Some controller (Eg. qcom SDHC) needs double the input clock
	 * for DDR modes. So to get the right divider, double the clock during
	 *  divider calculation */
	if ((host->quirks & SDHCI_QUIRK_NEED_2X_CLK_FOR_DDR_MODE) &&
	    ((timing == MMC_TIMING_UHS_DDR50) ||
	     (timing == MMC_TIMING_MMC_DDR52) ||
	     (timing == MMC_TIMING_MMC_HS400) ||
	     (timing == MMC_TIMING_MMC_HS400ES)))
		clock *= 2;

	if ((host->version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300) {
		/* Version 3.00 divisors must be a multiple of 2. */
		if (host->clock_base <= clock)
			div = 1;
		else {
			for (div = 2; div < SDHCI_MAX_DIV_SPEC_300; div += 2) {
				if ((host->clock_base / div) <= clock)
					break;
			}
		}
	} else {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
			if ((host->clock_base / div) <= clock)
				break;
		}
	}
	div >>= 1;

	clk = (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			printf("Internal clock never stabilised.\n");
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	host->clock = host->mmc_ctrlr.bus_hz;

	return 0;
}

/* Find leftmost set bit in a 32 bit integer */
static int fls(u32 x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static void sdhci_set_power(SdhciHost *host, unsigned short power)
{
	u8 pwr = 0;

	if (power != (unsigned short)-1) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = SDHCI_POWER_180;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = SDHCI_POWER_300;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = SDHCI_POWER_330;
			break;
		}
	}

	if (pwr == 0) {
		sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
		return;
	}

	if (host->quirks & SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER)
		sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);

	pwr |= SDHCI_POWER_ON;

	sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
}

static u16 sdhci_preset_value(SdhciHost *host, enum mmc_timing timing)
{
	u16 preset;

	/*
	 * This table should match the UHS modes in sdhci_set_uhs_signaling.
	 * SD timings are not listed since we currently only use presets for
	 * eMMC.
	 */
	switch (timing) {
	/*
	 * There is no official HS400 preset register in the SDHCI spec. We
	 * use the HS200 presets since HS400 requires tuning at HS200, so
	 * we should use the same drive strength.
	 */
	case MMC_TIMING_MMC_HS400ES:
	case MMC_TIMING_MMC_HS400:
	case MMC_TIMING_MMC_HS200:
		preset = sdhci_readw(host, SDHCI_PRESET_VALUE_SDR104);
		break;
	case MMC_TIMING_MMC_DDR52:
		preset = sdhci_readw(host, SDHCI_PRESET_VALUE_DDR50);
		break;
	case MMC_TIMING_MMC_HS:
		/*
		 * UHS-I presets can only be used with 1.8V VCCQ. Otherwise
		 * we need to use the 3.3V presets.
		 */
		if (host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ)
			preset = sdhci_readw(host,
					     SDHCI_PRESET_VALUE_HIGH_SPEED);
		else
			preset = sdhci_readw(host, SDHCI_PRESET_VALUE_SDR25);
		break;
	case MMC_TIMING_MMC_LEGACY:
		/*
		 * UHS-I presets can only be used with 1.8V VCCQ. Otherwise
		 * we need to use the 3.3V presets.
		 */
		if (host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ)
			preset = sdhci_readw(host,
					     SDHCI_PRESET_VALUE_DEFAULT_SPEED);
		else
			preset = sdhci_readw(host, SDHCI_PRESET_VALUE_SDR12);
		break;
	case MMC_TIMING_INITIALIZATION:
		preset = sdhci_readw(host, SDHCI_PRESET_VALUE_INIT);
		break;
	default:
		printf("Error: Unknown preset for timing %u\n", timing);
		/*
		 * Fall back to the initialization clock and driver strength
		 * since we don't know what mode we are in.
		 */
		preset = sdhci_readw(host, SDHCI_PRESET_VALUE_INIT);
	}

	return preset;
}

static enum mmc_driver_strength sdhci_preset_driver_strength(
	SdhciHost *host, enum mmc_timing timing) {
	u16 preset, driver_strength;

	preset = sdhci_preset_value(host, timing);

	driver_strength = preset & SDHCI_PRESET_VALUE_DRIVE_STRENGTH_MASK;
	driver_strength >>= SDHCI_PRESET_VALUE_DRIVE_STRENGTH_SHIFT;

	return (enum mmc_driver_strength)driver_strength;
}

static enum mmc_driver_strength sdhci_card_driver_strength(
	MmcMedia *media, enum mmc_timing timing)
{
	SdhciHost *host = container_of(media->ctrlr, SdhciHost, mmc_ctrlr);

	if (!(host->platform_info & SDHCI_PLATFORM_VALID_PRESETS))
		return MMC_DRIVER_STRENGTH_B;

	return sdhci_preset_driver_strength(host, timing);
}

static u16 sdhci_ctrlr_driver_strength(SdhciHost *host, enum mmc_timing timing)
{
	enum mmc_driver_strength driver_strength;

	if (host->platform_info & SDHCI_PLATFORM_VALID_PRESETS) {
		driver_strength = sdhci_preset_driver_strength(host, timing);
	} else {
		switch (timing) {
		case MMC_TIMING_MMC_HS200:
		case MMC_TIMING_UHS_SDR104:
		case MMC_TIMING_MMC_HS400:
		case MMC_TIMING_MMC_HS400ES:
			driver_strength = MMC_DRIVER_STRENGTH_A;
			break;
		default:
			driver_strength = MMC_DRIVER_STRENGTH_B;
		}
	}

	return (u16)driver_strength << SDHCI_CTRL_DRV_TYPE_SHIFT;
}

static unsigned int sdhci_preset_frequency(SdhciHost *host,
					   enum mmc_timing timing)
{
	u16 preset, divisor;
	unsigned int frequency;

	preset = sdhci_preset_value(host, timing);

	divisor = preset & SDHCI_PRESET_VALUE_DIVISOR_MASK;
	divisor >>= SDHCI_PRESET_VALUE_DIVISOR_SHIFT;

	if (divisor)
		frequency = host->clock_base / (divisor << 1);
	else
		frequency = host->clock_base;

	return frequency;
}

static void sdhci_set_uhs_signaling(SdhciHost *host, enum mmc_timing timing)
{
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	ctrl_2 &= ~SDHCI_CTRL_DRV_TYPE_MASK;

	switch (timing) {
	case MMC_TIMING_MMC_HS200:
	case MMC_TIMING_UHS_SDR104:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_TIMING_UHS_SDR12:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_UHS_SDR25:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_UHS_SDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
		break;
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
		break;
	case MMC_TIMING_MMC_HS400:
	case MMC_TIMING_MMC_HS400ES:
		ctrl_2 |= SDHCI_CTRL_HS400;
		break;
	case MMC_TIMING_MMC_HS:
		/*
		 * UHS-I timings will only be used with 1.8V VCCQ. Otherwise the
		 * High Speed Enabled bit determines the timing.
		 */
		if (!(host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ))
			ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_MMC_LEGACY:
		/*
		 * UHS-I timings will only be used with 1.8V VCCQ. Otherwise the
		 * High Speed Enabled bit determines the timing.
		 */
		if (!(host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ))
			ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_SD_DS:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_INITIALIZATION:
		break;
	default:
		mmc_error("%s: Unknown timing %u\n", __func__, timing);
	}

	ctrl_2 |= sdhci_ctrlr_driver_strength(host, timing);

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
}

void sdhci_set_ios(MmcCtrlr *mmc_ctrlr)
{
	u32 ctrl;
	unsigned int clock_or_timing_changed;
	SdhciHost *host = container_of(mmc_ctrlr,
				       SdhciHost, mmc_ctrlr);

	/*
	 * The SDHCI spec says the clock needs to be disabled when modifying
	 * the HISPD bit, changing the clock frequency, or changing timings
	 * when presets are enabled.
	 *
	 * Note: The HISPD bit is dependent on the timing.
	 */
	clock_or_timing_changed = mmc_ctrlr->bus_hz != host->clock ||
				  mmc_ctrlr->timing != host->timing;

	if (clock_or_timing_changed)
		sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	/* Switch to 1.8 volt for HS200 */
	if (mmc_ctrlr->caps & MMC_CAPS_1V8_VDD)
		if (mmc_ctrlr->bus_hz == MMC_CLOCK_200MHZ)
			sdhci_set_power(host, MMC_VDD_165_195_SHIFT);

	/* Set bus width */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	if (mmc_ctrlr->bus_width == 8) {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		if ((host->version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300)
			ctrl |= SDHCI_CTRL_8BITBUS;
	} else {
		if ((host->version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300)
			ctrl &= ~SDHCI_CTRL_8BITBUS;
		if (mmc_ctrlr->bus_width == 4)
			ctrl |= SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SDHCI_CTRL_4BITBUS;
	}

	/* Set the High Speed Enable bit */
	switch (mmc_ctrlr->timing) {
	case MMC_TIMING_INITIALIZATION:
	case MMC_TIMING_SD_DS:
		ctrl &= ~SDHCI_CTRL_HISPD;
		break;
	case MMC_TIMING_SD_HS:
		ctrl |= SDHCI_CTRL_HISPD;
		break;
	/*
	 * The high speed enable flag only has an effect when UHS-I
	 * (1.8V signaling) is disabled.
	 */
	case MMC_TIMING_MMC_LEGACY:
		if (host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ)
			ctrl &= ~SDHCI_CTRL_HISPD;
		break;
	case MMC_TIMING_MMC_HS:
		if (host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ)
			ctrl |= SDHCI_CTRL_HISPD;
		break;
	default:
		break;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	if (mmc_ctrlr->timing != host->timing)
		sdhci_set_uhs_signaling(host, mmc_ctrlr->timing);

	/*
	 * Now that the timing and the high speed bit have been set, we can
	 * enable the clock.
	 */
	if (clock_or_timing_changed) {
		/*
		 * If presets are enabled, the MMC layer will leave the
		 * responsibility of selecting the correct clock on a timing
		 * change to the SDHCI layer.
		 */
		if (mmc_ctrlr->bus_hz == host->clock &&
		    host->platform_info & SDHCI_PLATFORM_VALID_PRESETS)
			mmc_ctrlr->bus_hz =
				sdhci_preset_frequency(host, mmc_ctrlr->timing);
		sdhci_set_clock(mmc_ctrlr, mmc_ctrlr->bus_hz);
	}

	host->timing = mmc_ctrlr->timing;
}

/* Prepare SDHCI controller to be initialized */
static int sdhci_pre_init(SdhciHost *host)
{
	unsigned int caps, caps_1;

	if (host->attach) {
		int rv = host->attach(host);
		if (rv)
			return rv;
	}

	host->version = sdhci_readw(host, SDHCI_HOST_VERSION) & 0xff;

	caps = sdhci_readl(host, SDHCI_CAPABILITIES);
	caps_1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);

	if (host->platform_info & SDHCI_PLATFORM_SUPPORTS_HS400)
		host->host_caps |= MMC_CAPS_HS400;

	if (host->platform_info & SDHCI_PLATFORM_SUPPORTS_HS400ES)
		host->host_caps |= MMC_CAPS_HS400ES;

	if (caps_1 & SDHCI_SUPPORT_DDR50)
		host->host_caps |= MMC_CAPS_DDR52;

	if (caps & SDHCI_CAN_DO_ADMA2)
		host->host_caps |= MMC_CAPS_AUTO_CMD12;

	/* get base clock frequency from CAP register */
	if (!(host->quirks & SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN)) {
		if ((host->version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300)
			host->clock_base = (caps & SDHCI_CLOCK_V3_BASE_MASK)
				>> SDHCI_CLOCK_BASE_SHIFT;
		else
			host->clock_base = (caps & SDHCI_CLOCK_BASE_MASK)
				>> SDHCI_CLOCK_BASE_SHIFT;

		host->clock_base *= 1000000;
	}

	if (host->clock_base == 0) {
		printf("Hardware doesn't specify base clock frequency\n");
		return -1;
	}

	if (host->clock_f_max)
		host->mmc_ctrlr.f_max = host->clock_f_max;
	else
		host->mmc_ctrlr.f_max = host->clock_base;

	if (host->clock_f_min) {
		host->mmc_ctrlr.f_min = host->clock_f_min;
	} else {
		if ((host->version & SDHCI_SPEC_VER_MASK) >= SDHCI_SPEC_300)
			host->mmc_ctrlr.f_min =
				host->clock_base / SDHCI_MAX_DIV_SPEC_300;
		else
			host->mmc_ctrlr.f_min =
				host->clock_base / SDHCI_MAX_DIV_SPEC_200;
	}

	if (caps & SDHCI_CAN_VDD_330)
		host->mmc_ctrlr.voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
	if (caps & SDHCI_CAN_VDD_300)
		host->mmc_ctrlr.voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
	if (caps & SDHCI_CAN_VDD_180)
		host->mmc_ctrlr.voltages |= MMC_VDD_165_195;

	if (host->quirks & SDHCI_QUIRK_BROKEN_VOLTAGE)
		host->mmc_ctrlr.voltages |= host->voltages;

	if (host->platform_info & SDHCI_PLATFORM_NO_EMMC_HS200)
		host->mmc_ctrlr.caps = MMC_CAPS_HS | MMC_CAPS_HS_52MHz |
			MMC_CAPS_4BIT | MMC_CAPS_HC;
	else
		host->mmc_ctrlr.caps = MMC_CAPS_HS | MMC_CAPS_HS_52MHz |
			MMC_CAPS_4BIT | MMC_CAPS_HC | MMC_CAPS_HS200;

	if (host->platform_info & SDHCI_PLATFORM_EMMC_1V8_POWER)
		host->mmc_ctrlr.caps |= MMC_CAPS_1V8_VDD;

	if (caps & SDHCI_CAN_DO_8BIT)
		host->mmc_ctrlr.caps |= MMC_CAPS_8BIT;
	if (host->host_caps)
		host->mmc_ctrlr.caps |= host->host_caps;
	if (caps & SDHCI_CAN_64BIT)
		host->dma64 = 1;

	host->timing = MMC_TIMING_UNINITIALIZED;

	sdhci_reset(host, SDHCI_RESET_ALL);

	return 0;
}

static int sdhci_init(SdhciHost *host)
{
	u16 reg;
	u32 ctrl;
	int rv;

	rv = sdhci_pre_init(host);
	if (rv)
		return rv; /* The error has been already reported */

	sdhci_set_power(host, fls(host->mmc_ctrlr.voltages) - 1);

	if (host->quirks & SDHCI_QUIRK_NO_CD) {
		unsigned int status;

		sdhci_writel(host, SDHCI_CTRL_CD_TEST_INS | SDHCI_CTRL_CD_TEST,
			SDHCI_HOST_CONTROL);

		status = sdhci_readl(host, SDHCI_PRESENT_STATE);
		while ((!(status & SDHCI_CARD_PRESENT)) ||
		    (!(status & SDHCI_CARD_STATE_STABLE)) ||
		    (!(status & SDHCI_CARD_DETECT_PIN_LEVEL)))
			status = sdhci_readl(host, SDHCI_PRESENT_STATE);
	}

	/*
	 * eMMC VCCQ (I/O Signaling) is typically hard wired. Unlike SD cards,
	 * There is no protocol to negotiate signal voltage switching,
	 * We need to configure the controllers signaling level before
	 * performing any transactions to avoid a voltage mismatch.
	 */
	if (host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_EMBEDDED &&
	    !(host->platform_info & SDHCI_PLATFORM_EMMC_33V_VCCQ)) {
		reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		reg |= SDHCI_CTRL_180V_SIGNALING_ENABLE;
		sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
	}

	/* Configure ADMA mode */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	if (host->host_caps & MMC_CAPS_AUTO_CMD12) {
		ctrl &= ~SDHCI_CTRL_DMA_MASK;
		if (host->dma64)
			ctrl |= SDHCI_CTRL_ADMA64;
		else
			ctrl |= SDHCI_CTRL_ADMA32;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	/* Enable only interrupts served by the SD controller */
	sdhci_writel(host, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK,
		     SDHCI_INT_ENABLE);
	/* Mask all sdhci interrupt sources */
	sdhci_writel(host, 0x0, SDHCI_SIGNAL_ENABLE);

	/* Set timeout to maximum, shouldn't happen if everything's right. */
	sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);

	if (!(host->platform_info & SDHCI_PLATFORM_EMMC_HARDWIRED_VCC))
		udelay(10000);

	return 0;
}

static int sdhci_update(BlockDevCtrlrOps *me)
{
	SdhciHost *host = container_of
		(me, SdhciHost, mmc_ctrlr.ctrlr.ops);

	if (host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_REMOVABLE) {
		int present;

		if (host->cd_gpio)
			present = gpio_get(host->cd_gpio);
		else
			present = (sdhci_readl(host, SDHCI_PRESENT_STATE) &
				   SDHCI_CARD_PRESENT) != 0;

		if (!present) {
			if (host->mmc_ctrlr.media) {
				/*
				 * A card was present but isn't any more. Get
				 * rid of it.
				 */
				list_remove
					(&host->mmc_ctrlr.media->dev.list_node);
				free(host->mmc_ctrlr.media);
				host->mmc_ctrlr.media = NULL;
			}
			host->detection_tries = 0;
			return 0;
		}

		if (host->detection_tries > 3)
			return 0;

		if (!host->mmc_ctrlr.media) {
			/*
			 * A card is present and not set up yet. Get it ready.
			 */
			if (sdhci_init(host) ||
			    mmc_setup_media(&host->mmc_ctrlr)) {
				host->detection_tries++;
				return -1;
			}
			host->mmc_ctrlr.media->dev.name = "SDHCI card";
			list_insert_after
				(&host-> mmc_ctrlr.media->dev.list_node,
				 &removable_block_devices);
		}
	} else {
		if (!host->initialized && sdhci_init(host))
			return -1;

		host->initialized = 1;

		if (mmc_setup_media(&host->mmc_ctrlr))
			return -1;
		host->mmc_ctrlr.media->dev.name = "SDHCI fixed";
		list_insert_after(&host->mmc_ctrlr.media->dev.list_node,
				  &fixed_block_devices);
		host->mmc_ctrlr.ctrlr.need_update = 0;
	}

	host->mmc_ctrlr.media->dev.removable =
		host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_REMOVABLE;
	host->mmc_ctrlr.media->dev.ops.read = block_mmc_read;
	host->mmc_ctrlr.media->dev.ops.write = block_mmc_write;
	host->mmc_ctrlr.media->dev.ops.fill_write = block_mmc_fill_write;
	host->mmc_ctrlr.media->dev.ops.new_stream = new_simple_stream;
	host->mmc_ctrlr.media->dev.ops.get_health_info =
		block_mmc_get_health_info;

	return 0;
}

void add_sdhci(SdhciHost *host)
{
	host->mmc_ctrlr.send_cmd = &sdhci_send_command;
	host->mmc_ctrlr.set_ios = &sdhci_set_ios;
	host->mmc_ctrlr.card_driver_strength = &sdhci_card_driver_strength;
	host->mmc_ctrlr.presets_enabled =
		!!(host->platform_info & SDHCI_PLATFORM_VALID_PRESETS);

	host->mmc_ctrlr.ctrlr.ops.is_bdev_owned = block_mmc_is_bdev_owned;
	host->mmc_ctrlr.ctrlr.ops.update = &sdhci_update;
	host->mmc_ctrlr.ctrlr.need_update = 1;

	/* TODO(vbendeb): check if SDHCI spec allows to retrieve this value. */
	host->mmc_ctrlr.b_max = 65535;

	assert(host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_EMBEDDED ||
	       !(host->platform_info & SDHCI_PLATFORM_EMMC_HARDWIRED_VCC));
}
