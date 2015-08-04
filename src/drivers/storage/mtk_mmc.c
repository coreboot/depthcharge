/*
 * Copyright 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/storage/mtk_mmc_private.h"
#include "drivers/storage/mtk_mmc.h"

enum {
	/* For card identification, and also the highest low-speed SDOI card */
	/* frequency (actually 400Khz). */
	MtkMmcMinFreq = 375000,

	/* Highest HS eMMC clock as per the SD/MMC spec (actually 52MHz). */
	MtkMmcMaxFreq = 50000000,

	MtkMmcVoltages = (MMC_VDD_32_33 | MMC_VDD_33_34),
};

static void mtk_mmc_set_buswidth(MtkMmcHost *host, u32 width)
{
	MtkMmcReg *reg = host->reg;
	u32 val;

	switch (width) {
	case 1:
		val = (MSDC_BUS_1BITS << 16);
		break;
	case 4:
		val = (MSDC_BUS_4BITS << 16);
		break;
	case 8:
		val = (MSDC_BUS_8BITS << 16);
		break;
	default:
		die("Impossible bus width!");
	}

	clrsetbits_le32(&reg->sdc_cfg, SDC_CFG_BUSWIDTH, val);
}

static void mtk_mmc_change_clock(MtkMmcHost *host, uint32_t clock)
{
	u32 mode;
	u32 div;
	u32 sclk;
	u32 hclk = host->src_hz;
	MtkMmcReg *reg = host->reg;

	/* Only support SDR mode */
	if (clock >= hclk) {
		mode = 0x1;	/* no divisor */
		div = 0;
		sclk = hclk;
	} else {
		mode = 0x0;	/* use divisor */
		if (clock >= (hclk / 2)) {
			div = 0;	/* mean div = 1/2 */
			sclk = hclk / 2;	/* sclk = clk / 2 */
		} else {
			div = div_round_up(hclk, clock * 4);
			sclk = (hclk >> 2) / div;
		}
	}

	clrsetbits_le32(&reg->msdc_cfg, MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(mode << 16) | (((div + 1) % 0xff) << 8));
	if (mmc_busy_wait_io_until(&reg->msdc_cfg, NULL, MSDC_CFG_CKSTB,
				   MTK_MMC_TIMEOUT_MS))
		mmc_error("Failed while wait clock stable!\n");
	mmc_debug("sclk: %d\n", sclk);
}

static inline u32 mtk_mmc_cmd_find_resp(MmcCommand *cmd)
{
	u32 resp;

	switch (cmd->resp_type) {
	/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = RESP_R1;
		break;
	case MMC_RSP_R1b:
		resp = RESP_R1B;
		break;
	case MMC_RSP_R2:
		resp = RESP_R2;
		break;
	case MMC_RSP_R3:
		resp = RESP_R3;
		break;
	case MMC_RSP_NONE:
	default:
		resp = RESP_NONE;
		break;
	}

	return resp;
}

static void mtk_mmc_dma_on(MtkMmcHost *host)
{
	MtkMmcReg *reg = host->reg;
	clrbits_le32(&reg->msdc_cfg, MSDC_CFG_PIO);
}

static void mtk_mmc_dma_start(MtkMmcHost *host)
{
	MtkMmcReg *reg = host->reg;
	setbits_le32(&reg->dma_ctrl, MSDC_DMA_CTRL_START);
}

static void mtk_mmc_dma_stop(MtkMmcHost *host)
{
	MtkMmcReg *reg = host->reg;
	setbits_le32(&reg->dma_ctrl, MSDC_DMA_CTRL_STOP);
	if (mmc_busy_wait_io(&reg->dma_cfg, NULL, MSDC_DMA_CFG_STS,
			     MTK_MMC_TIMEOUT_MS))
		mmc_error("Failed while wait DMA to inactive!\n");
}

static inline u32 mtk_mmc_prepare_raw_cmd(MtkMmcHost *host,
        MmcCommand *cmd, MmcData *data)
{
	MtkMmcReg *reg = host->reg;
	union {
		u32 full;
		struct {
			u32 cmd: 6;
			u32 brk: 1;
			u32 rsptype: 3;
			u32 resv: 1;
			u32 dtype: 2;
			u32 rw: 1;
			u32 stop: 1;
			u32 go_irq: 1;
			u32 len: 12;
			u32 auto_cmd: 2;
			u32 vol_switch: 1;
		};
	} rawcmd = {.full = 0};
	u32 opcode = cmd->cmdidx;
	rawcmd.cmd = cmd->cmdidx;
	rawcmd.rsptype = mtk_mmc_cmd_find_resp(cmd);

	if (opcode == MMC_CMD_STOP_TRANSMISSION)
		rawcmd.stop = 1;

	if ((opcode == SD_CMD_APP_SEND_SCR) ||
	    (opcode == SD_CMD_SWITCH_FUNC &&
	     cmd->resp_type == MMC_RSP_R1 /* for Sd card */) ||
	    (opcode == MMC_CMD_SEND_EXT_CSD && data != NULL))
		rawcmd.dtype = 1;

	if (data) {
		rawcmd.len = data->blocksize & 0xFFF;
		if (data->flags & MMC_DATA_WRITE)
			rawcmd.rw = 1;
		if (data->blocks > 1)
			rawcmd.dtype = 2;
		else
			rawcmd.dtype = 1;

		mtk_mmc_dma_on(host);	/* always use basic DMA */

		writel(data->blocks, &reg->sdc_blk_num);
	}
	return rawcmd.full;
}

static inline u32 mtk_mmc_get_irq_status(MtkMmcHost *host, u32 mask)
{
	u32 status = readl(&host->reg->msdc_int);
	if (status)
		writel(status, &host->reg->msdc_int);
	return status & mask;
}

/* returns true if command is fully handled; returns false otherwise */
static int mtk_mmc_cmd_done(MtkMmcHost *host, int events, MmcCommand *cmd)
{
	MtkMmcReg *reg = host->reg;

	if (events & MSDC_INT_CMDTMO) {
		mmc_debug("cmd: %d timeout!\n", cmd->cmdidx);
		return MMC_TIMEOUT;
	} else if (events & MSDC_INT_CMDRDY) {
		switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			break;
		case MMC_RSP_R2:
			cmd->response[0] = readl(&reg->sdc_resp3);
			cmd->response[1] = readl(&reg->sdc_resp2);
			cmd->response[2] = readl(&reg->sdc_resp1);
			cmd->response[3] = readl(&reg->sdc_resp0);
			break;
		default:	/* Response types 1, 3, 4, 5, 6, 7(1b) */
			cmd->response[0] = readl(&reg->sdc_resp0);
			break;

		}
		return 0;

	} else {
		mmc_error("cmd: %d crc error!\n", cmd->cmdidx);
		return MMC_COMM_ERR;
	}
}

static inline int mtk_mmc_cmd_do_poll(MtkMmcHost *host, MmcCommand *cmd)
{
	int intsts;
	u32 done = 0;

	for (; !done;) {
		intsts = mtk_mmc_get_irq_status(host, MSDC_INT_CMDRDY |
				MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO);
		done = intsts;
	}

	return mtk_mmc_cmd_done(host, intsts, cmd);
}

static void mtk_mmc_prepare_data(MtkMmcHost *host, MmcData *data,
                                 struct bounce_buffer *bbstate)
{
	MtkMmcReg *reg = host->reg;

	writel((uintptr_t) bbstate->bounce_buffer, &reg->dma_sa);
	writel(data->blocks * data->blocksize, &reg->dma_length);
	clrsetbits_le32(&reg->dma_ctrl, MSDC_DMA_CTRL_MODE,
			MSDC_DMA_CTRL_LASTBUF);
}

static int sdc_wait_ready(MtkMmcHost *host, MmcCommand *cmd, MmcData *data)
{
	MtkMmcReg *reg = host->reg;
	if (mmc_busy_wait_io(&reg->sdc_sts, NULL, SDC_STS_CMDBUSY,
			     MTK_MMC_TIMEOUT_MS)) {
			mmc_error("Failed while wait sdc_cmdbusy ready!\n");
			return -1;
	}

	if (cmd->resp_type == MMC_RSP_R1b || data) {
		/* should wait SDCBUSY goto high
		 * the core layer should already ensure card is in
		 * ready for data state by cmd13.
		 */
		if (mmc_busy_wait_io(&reg->sdc_sts, NULL, SDC_STS_SDCBUSY,
				     MTK_MMC_TIMEOUT_MS)) {
			mmc_error("Failed while wait sdc_busy ready!\n");
			return -1;
		}
	}

	return 0;
}

static int mtk_mmc_send_cmd_bounced(MmcCtrlr *ctrlr, MmcCommand *cmd,
                                    MmcData *data,
                                    struct bounce_buffer *bbstate)
{
	MtkMmcHost *host = container_of(ctrlr, MtkMmcHost, mmc);
	MtkMmcReg *reg = host->reg;
	u32 done = 0;
	int ret = 0;
	u32 rawcmd;
	u32 ints;
	mmc_debug("%s called\n", __func__);

	ret = sdc_wait_ready(host, cmd, data);

	if (ret < 0)
		return ret;

	rawcmd = mtk_mmc_prepare_raw_cmd(host, cmd, data);

	writel(cmd->cmdarg, &reg->sdc_arg);
	writel(rawcmd, &reg->sdc_cmd);

	ret = mtk_mmc_cmd_do_poll(host, cmd);
	if (ret < 0) {
		return ret;
	}

	if (data) {
		mtk_mmc_prepare_data(host, data, bbstate);
		mtk_mmc_dma_start(host);
		for (; !done;) {
			ints = mtk_mmc_get_irq_status(host,  MSDC_INT_XFER_COMPL |
					MSDC_INT_DATTMO | MSDC_INT_DATCRCERR |
					MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR |
					MSDC_INT_DMA_PROTECT);
			done = ints;
		}
		mtk_mmc_dma_stop(host);

		if (ints & MSDC_INT_XFER_COMPL) {
			return 0;
		} else {
			if (ints & MSDC_INT_DATTMO)
				mmc_error("Data timeout, ints: %x\n", ints);
			else if (ints & MSDC_INT_DATCRCERR)
				mmc_error("Data crc error, ints: %x\n", ints);
			return -1;
		}
	}

	mmc_debug("cmd->arg: %08x\n", cmd->cmdarg);

	return 0;
}

static int mtk_mmc_send_cmd(MmcCtrlr *ctrlr, MmcCommand *cmd, MmcData *data)
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

	ret = mtk_mmc_send_cmd_bounced(ctrlr, cmd, data, &bbstate);

	if (data)
		bounce_buffer_stop(&bbstate);

	return ret;
}

static void mtk_mmc_set_ios(MmcCtrlr *ctrlr)
{
	MtkMmcHost *host = container_of(ctrlr, MtkMmcHost, mmc);

	mmc_debug("%s: called, bus_width: %x, clock: %d -> %d\n", __func__,
	          ctrlr->bus_width, host->clock, ctrlr->bus_hz);

	/* Change clock first */
	mtk_mmc_change_clock(host, ctrlr->bus_hz);
	mtk_mmc_set_buswidth(host, ctrlr->bus_width);
}

static void mtk_mmc_reset_hw(MtkMmcHost *host)
{
	MtkMmcReg *reg = host->reg;

	setbits_le32(&(reg)->msdc_cfg, MSDC_CFG_RST);
	if (mmc_busy_wait_io(&reg->msdc_cfg, NULL, MSDC_CFG_RST,
			     MTK_MMC_TIMEOUT_MS))
		mmc_error("Failed while wait msdc reset!\n");
	setbits_le32(&(reg)->msdc_fifocs, MSDC_FIFOCS_CLR);
	if (mmc_busy_wait_io(&reg->msdc_fifocs, NULL, MSDC_FIFOCS_CLR,
			     MTK_MMC_TIMEOUT_MS))
		mmc_error("Failed while wait fifo clear!\n");
}

static int mtk_mmc_init(BlockDevCtrlrOps *me)
{
	MtkMmcHost *host = container_of(me, MtkMmcHost, mmc.ctrlr.ops);
	MtkMmcReg *reg = host->reg;
	mmc_debug("%s called\n", __func__);

	/* Configure to MMC/SD mode */
	setbits_le32(&reg->msdc_cfg, MSDC_CFG_MODE);
	/* Reset */
	mtk_mmc_reset_hw(host);
	/* Disable card detection */
	clrbits_le32(&reg->msdc_ps, MSDC_PS_CDEN);
	/* Disable and clear all interrupts */
	writel(0, &reg->msdc_inten);
	/* All the bits in msdc_int are WIC (write 1 to clear),
	 * so writing the value we just read should clear all bits. */
	writel(readl(&reg->msdc_int), &reg->msdc_int);
	/* Configure to default data timeout */
	clrsetbits_le32(&reg->sdc_cfg, SDC_CFG_DTOC, DEFAULT_DTOC << 24);
	mtk_mmc_set_buswidth(host, 1);

	return 0;
}

static int mtk_mmc_update(BlockDevCtrlrOps *me)
{
	MtkMmcHost *host = container_of(me, MtkMmcHost, mmc.ctrlr.ops);
	if (!host->initialized && mtk_mmc_init(me))
		return -1;
	host->initialized = 1;

	if (host->removable) {
		int present = host->cd_gpio->get(host->cd_gpio);
		mmc_debug("SD card present: %d\n", present);
		if (present && !host->mmc.media) {
			/* A card is present and not set up yet. Get it ready. */
			if (mmc_setup_media(&host->mmc))
				return -1;
			host->mmc.media->dev.name = "removable mtk_mmc";
			host->mmc.media->dev.removable = 1;
			host->mmc.media->dev.ops.read = &block_mmc_read;
			host->mmc.media->dev.ops.write = &block_mmc_write;
			host->mmc.media->dev.ops.fill_write =
				&block_mmc_fill_write;
			host->mmc.media->dev.ops.new_stream =
						&new_simple_stream;
			list_insert_after(&host->mmc.media->dev.list_node,
			                  &removable_block_devices);
		} else if (!present && host->mmc.media) {
			/* A card was present but isn't any more. Get rid of it. */
			list_remove(&host->mmc.media->dev.list_node);
			free(host->mmc.media);
			host->mmc.media = NULL;
		}
	} else {
		if (mmc_setup_media(&host->mmc))
			return -1;
		host->mmc.media->dev.name = "mtk_mmc";
		host->mmc.media->dev.removable = 0;
		host->mmc.media->dev.ops.read = &block_mmc_read;
		host->mmc.media->dev.ops.write = &block_mmc_write;
		host->mmc.media->dev.ops.fill_write = &block_mmc_fill_write;
		host->mmc.media->dev.ops.new_stream = &new_simple_stream;
		list_insert_after(&host->mmc.media->dev.list_node,
		                  &fixed_block_devices);
		host->mmc.ctrlr.need_update = 0;
	}
	return 0;
}

MtkMmcHost *new_mtk_mmc_host(uintptr_t ioaddr, uint32_t src_hz, int bus_width,
			     int removable, GpioOps *card_detect)
{
	MtkMmcHost *ctrlr = xzalloc(sizeof(*ctrlr));

	ctrlr->mmc.ctrlr.ops.update = &mtk_mmc_update;
	ctrlr->mmc.ctrlr.need_update = 1;

	ctrlr->mmc.voltages = MtkMmcVoltages;
	ctrlr->mmc.f_min = MtkMmcMinFreq;
	ctrlr->mmc.f_max = MtkMmcMaxFreq;

	ctrlr->mmc.bus_width = bus_width;
	ctrlr->mmc.bus_hz = ctrlr->mmc.f_min;
	ctrlr->mmc.b_max = 65535;	/* Some controllers use 16-bit regs. */

	if (bus_width == 8) {
		ctrlr->mmc.caps |= MMC_MODE_8BIT;
		ctrlr->mmc.caps &= ~MMC_MODE_4BIT;
	} else {
		ctrlr->mmc.caps |= MMC_MODE_4BIT;
		ctrlr->mmc.caps &= ~MMC_MODE_8BIT;
	}
	ctrlr->mmc.caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_HC;
	ctrlr->mmc.send_cmd = &mtk_mmc_send_cmd;
	ctrlr->mmc.set_ios = &mtk_mmc_set_ios;

	ctrlr->src_hz = src_hz;
	ctrlr->reg = (MtkMmcReg *)ioaddr;
	ctrlr->removable = removable;
	ctrlr->cd_gpio = card_detect;

	return ctrlr;
}
