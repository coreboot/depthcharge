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
 */

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/storage/mtk_mmc_private.h"
#include "drivers/storage/mtk_mmc.h"

#define PAD_DELAY_MAX 32

struct msdc_delay_phase {
	u8 maxlen;
	u8 start;
	u8 final_phase;
};

static const u8 tuning_blk_pattern_4bit[] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const u8 tuning_blk_pattern_8bit[] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

enum {
	/* For card identification, and also the highest low-speed SDOI card */
	/* frequency (actually 400Khz). */
	MtkMmcMinFreq = 375 * KHz,

	/* Highest HS eMMC clock as per the SD/MMC spec. */
	MtkMmcMaxFreq = 200 * MHz,

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
		die("Impossible bus width!\n");
	}

	clrsetbits_le32(&reg->sdc_cfg, SDC_CFG_BUSWIDTH, val);
}

static void mtk_mmc_change_clock(MtkMmcHost *host, uint32_t clock)
{
	u32 mode, mode_shift;
	u32 div, div_mask, div_width = 0;
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

	switch (host->version) {
		case MTK_MMC_V1:
			div_width = 8;
			break;
		case MTK_MMC_V2:
			div_width = 12;
			break;
	}
	assert(0 < div_width);

	div_mask = (1 << div_width) - 1;
	mode_shift = 8 + div_width;
	assert(div <= div_mask);

	clrsetbits_le32(&reg->msdc_cfg, (0x3 << mode_shift) | (div_mask << 8),
			(mode << mode_shift) | (div << 8));
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

		write32(&reg->sdc_blk_num, data->blocks);
	}
	return rawcmd.full;
}

static inline u32 mtk_mmc_get_irq_status(MtkMmcHost *host, u32 mask)
{
	u32 status = read32(&host->reg->msdc_int);
	if (status)
		write32(&host->reg->msdc_int, status);
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
			cmd->response[0] = read32(&reg->sdc_resp3);
			cmd->response[1] = read32(&reg->sdc_resp2);
			cmd->response[2] = read32(&reg->sdc_resp1);
			cmd->response[3] = read32(&reg->sdc_resp0);
			break;
		default:	/* Response types 1, 3, 4, 5, 6, 7(1b) */
			cmd->response[0] = read32(&reg->sdc_resp0);
			break;

		}
		return 0;

	} else {
		if (cmd->cmdidx == MMC_CMD_SEND_STATUS)
			return 0;
		else {
			mmc_error("cmd: %d crc error!\n", cmd->cmdidx);
			return MMC_COMM_ERR;
		}
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

	write32(&reg->dma_sa, (uintptr_t)bbstate->bounce_buffer);
	write32(&reg->dma_length, data->blocks * data->blocksize);
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
	int cmd_err = 0;

	mmc_debug("%s called\n", __func__);

	ret = sdc_wait_ready(host, cmd, data);

	if (ret < 0)
		return ret;

	rawcmd = mtk_mmc_prepare_raw_cmd(host, cmd, data);

	write32(&reg->sdc_arg, cmd->cmdarg);
	write32(&reg->sdc_cmd, rawcmd);

	ret = mtk_mmc_cmd_do_poll(host, cmd);
	if (ret < 0) {
		if (cmd->cmdidx == MMC_SEND_TUNING_BLOCK_HS200 &&
		    ret == MMC_COMM_ERR)
			cmd_err = MMC_COMM_ERR;
		else
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
			return cmd_err;
		} else {
			if (ints & MSDC_INT_DATTMO)
				mmc_error("Data timeout, ints: %x\n", ints);
			else if (ints & MSDC_INT_DATCRCERR)
				mmc_error("Data crc error, ints: %x\n", ints);
			return -1;
		}
	}

	mmc_debug("cmd->arg: %08x\n", cmd->cmdarg);

	return cmd_err;
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

static int get_delay_len(u32 delay, u32 start_bit)
{
	int i;

	for (i = 0; i < PAD_DELAY_MAX - start_bit; i++) {
		if (!(delay & BIT(start_bit + i)))
			break;
	}
	return i;
}

static struct msdc_delay_phase get_best_delay(MtkMmcHost *host, u32 delay)
{
	int start = 0, len = 0;
	int start_final = 0, len_final = 0;
	u8 final_phase;
	struct msdc_delay_phase delay_phase = { 0 };

	if (delay == 0) {
		mmc_error("phase error: [map:%x]\n", delay);
		delay_phase.final_phase = 0xff;
		return delay_phase;
	}

	while (start < PAD_DELAY_MAX) {
		len = get_delay_len(delay, start);
		if (len_final < len) {
			start_final = start;
			len_final = len;
		}
		start += len ? len : 1;
		if (len_final >= 12 && start_final < 4)
			break;
	}

	/* The goal is to find the smallest delay cell */
	if (start_final == 0)
		final_phase = (start_final + len_final / 3) % PAD_DELAY_MAX;
	else
		final_phase = (start_final + len_final / 2) % PAD_DELAY_MAX;
	printf("phase: [map:%x] [maxlen:%d] [final:%d]\n",
	       delay, len_final, final_phase);

	delay_phase.maxlen = len_final;
	delay_phase.start = start_final;
	delay_phase.final_phase = final_phase;
	return delay_phase;
}

static inline void msdc_set_cmd_delay(MtkMmcHost *host, u32 val)
{
	MtkMmcTopReg *top_reg = host->top_reg;
	MtkMmcReg *reg = host->reg;

	if (top_reg)
		clrsetbits_le32(&top_reg->emmc_top_cmd, PAD_CMD_RXDLY,
				val << 5);
	else
		clrsetbits_le32(&reg->pad_tune, MSDC_PAD_TUNE_CMDRDLY,
				val << 16);
}

static inline void msdc_set_data_delay(MtkMmcHost *host, u32 val)
{
	MtkMmcTopReg *top_reg = host->top_reg;
	MtkMmcReg *reg = host->reg;

	if (top_reg)
		clrsetbits_le32(&top_reg->emmc_top_control, PAD_DAT_RD_RXDLY,
				val << 7);
	else
		clrsetbits_le32(&reg->pad_tune, MSDC_PAD_TUNE_DATRRDLY,
				val << 8);
}

static int mtk_mmc_send_tuning(MmcCtrlr *ctrlr, u32 opcode)
{
	int ret;
	char *buf;
	const u8 *tuning_block_pattern;
	MmcCommand cmd = {
		.cmdidx = opcode,
		.resp_type = MMC_RSP_R1,
		.cmdarg = 0,
		.flags = 0,
	};

	MmcData data;
	data.blocks = 1;
	switch (ctrlr->bus_width) {
	case 4:
		data.blocksize = sizeof(tuning_blk_pattern_4bit);
		tuning_block_pattern = tuning_blk_pattern_4bit;
		break;
	case 8:
		data.blocksize = sizeof(tuning_blk_pattern_8bit);
		tuning_block_pattern = tuning_blk_pattern_8bit;
		break;
	default:
		mmc_error("Unsupported bus width: %d\n", ctrlr->bus_width);
		return MMC_SUPPORT_ERR;
	}
	buf = xmalloc(data.blocksize);
	data.dest = buf;
	data.flags = MMC_DATA_READ;

	ret = mtk_mmc_send_cmd(ctrlr, &cmd, &data);
	if (ret)
		goto out;

	if (memcmp(buf, tuning_block_pattern, data.blocksize))
		ret = -1;

out:
	free(buf);
	return ret;
}

static u32 mtk_mmc_tuning_together(MmcCtrlr *ctrlr)
{
	MtkMmcHost *host = container_of(ctrlr, MtkMmcHost, mmc);
	u32 delay = 0;
	int i, ret;

	for (i = 0; i < PAD_DELAY_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		msdc_set_data_delay(host, i);
		ret = mtk_mmc_send_tuning(ctrlr, MMC_SEND_TUNING_BLOCK_HS200);
		if (!ret)
			delay |= BIT(i);
	}

	return delay;
}

static int mtk_mmc_execute_tuning(MmcMedia *media)
{
	MmcCtrlr *ctrlr = media->ctrlr;
	MtkMmcHost *host = container_of(ctrlr, MtkMmcHost, mmc);
	MtkMmcReg *reg = host->reg;
	u32 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0 };
	u8 final_delay;

	clrbits_le32(&reg->msdc_iocon, MSDC_IOCON_RSPL);
	clrbits_le32(&reg->msdc_iocon, MSDC_IOCON_DSPL);
	clrbits_le32(&reg->msdc_iocon, MSDC_IOCON_W_DSPL);

	rise_delay = mtk_mmc_tuning_together(ctrlr);
	final_rise_delay = get_best_delay(host, rise_delay);
	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	setbits_le32(&reg->msdc_iocon, MSDC_IOCON_RSPL);
	setbits_le32(&reg->msdc_iocon, MSDC_IOCON_DSPL);
	setbits_le32(&reg->msdc_iocon, MSDC_IOCON_W_DSPL);
	fall_delay = mtk_mmc_tuning_together(ctrlr);
	final_fall_delay = get_best_delay(host, fall_delay);

skip_fall:
	if (final_rise_delay.maxlen >= final_fall_delay.maxlen) {
		clrbits_le32(&reg->msdc_iocon, MSDC_IOCON_RSPL);
		clrbits_le32(&reg->msdc_iocon, MSDC_IOCON_DSPL);
		clrbits_le32(&reg->msdc_iocon, MSDC_IOCON_W_DSPL);
		final_delay = final_rise_delay.final_phase;
	} else {
		setbits_le32(&reg->msdc_iocon, MSDC_IOCON_RSPL);
		setbits_le32(&reg->msdc_iocon, MSDC_IOCON_DSPL);
		setbits_le32(&reg->msdc_iocon, MSDC_IOCON_W_DSPL);
		final_delay = final_fall_delay.final_phase;
	}

	msdc_set_cmd_delay(host, final_delay);
	msdc_set_data_delay(host, final_delay);

	printf("Final pad delay: %x\n", final_delay);
	return final_delay == 0xff ? MMC_COMM_ERR : 0;
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
	MtkMmcTopReg *top_reg = host->top_reg;
	MtkMmcTuneReg tune_reg = host->tune_reg;
	mmc_debug("%s called\n", __func__);

	/* Configure to MMC/SD mode */
	setbits_le32(&reg->msdc_cfg, MSDC_CFG_MODE);
	/* Reset */
	mtk_mmc_reset_hw(host);
	/* Disable card detection */
	clrbits_le32(&reg->msdc_ps, MSDC_PS_CDEN);
	/* Disable and clear all interrupts */
	write32(&reg->msdc_inten, 0);
	/* All the bits in msdc_int are WIC (write 1 to clear),
	 * so writing the value we just read should clear all bits. */
	write32(&reg->msdc_int, read32(&reg->msdc_int));
	/* Configure to default data timeout */
	clrsetbits_le32(&reg->sdc_cfg, SDC_CFG_DTOC, DEFAULT_DTOC << 24);
	write32(&reg->msdc_iocon, tune_reg.msdc_iocon);
	write32(&reg->pad_tune, tune_reg.pad_tune);

	if (host->version == MTK_MMC_V2) {
		write32(&reg->msdc_iocon, 0);
		write32(&reg->patch_bit0, 0x403c0446);
		write32(&reg->patch_bit1, 0xffff4089);
		setbits_le32(&reg->emmc50_cfg0, EMMC50_CFG_CFCSTS_SEL);

		if (top_reg) {
			write32(&top_reg->emmc_top_control, 0);
			write32(&top_reg->emmc_top_cmd, 0);
		} else {
			write32(&reg->pad_tune0, 0);
		}

		/* Stop clk fix */
		clrsetbits_le32(&reg->patch_bit1, MSDC_PATCH_BIT1_STOP_DLY,
				3 << 8);
		clrbits_le32(&reg->sdc_fifo_cfg, SDC_FIFO_CFG_WRVALIDSEL);
		clrbits_le32(&reg->sdc_fifo_cfg, SDC_FIFO_CFG_RDVALIDSEL);

		/* Async fifo */
		clrsetbits_le32(&reg->patch_bit2, MSDC_PB2_RESPWAIT, 3 << 2);
		clrbits_le32(&reg->patch_bit2, MSDC_PATCH_BIT2_CFGRESP);
		setbits_le32(&reg->patch_bit2, MSDC_PATCH_BIT2_CFGCRCSTS);

		/* Enhance rx */
		if (top_reg)
			setbits_le32(&top_reg->emmc_top_control, SDC_RX_ENH_EN);
		else
			setbits_le32(&reg->sdc_adv_cfg0, SDC_RX_ENHANCE_EN);

		/* Tune data */
		if (top_reg) {
			setbits_le32(&top_reg->emmc_top_control,
				     PAD_DAT_RD_RXDLY_SEL);
			clrbits_le32(&top_reg->emmc_top_control,
				     DATA_K_VALUE_SEL);
			setbits_le32(&top_reg->emmc_top_cmd,
				     PAD_CMD_RD_RXDLY_SEL);
		} else {
			setbits_le32(&reg->pad_tune0, MSDC_PAD_TUNE_RD_SEL);
			setbits_le32(&reg->pad_tune0, MSDC_PAD_TUNE_CMD_SEL);
		}
	}

	mtk_mmc_set_buswidth(host, 1);

	return 0;
}

static int mtk_mmc_update(BlockDevCtrlrOps *me)
{
	MtkMmcHost *host = container_of(me, MtkMmcHost, mmc.ctrlr.ops);
	if (!host->initialized && mtk_mmc_init(me))
		return -1;
	host->initialized = 1;

	if (host->mmc.slot_type == MMC_SLOT_TYPE_REMOVABLE) {
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
			host->mmc.media->dev.ops.get_health_info =
				block_mmc_get_health_info;
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
		host->mmc.media->dev.ops.get_health_info =
			block_mmc_get_health_info;
		list_insert_after(&host->mmc.media->dev.list_node,
		                  &fixed_block_devices);
		host->mmc.ctrlr.need_update = 0;
	}
	return 0;
}

MtkMmcHost *new_mtk_mmc_host(uintptr_t ioaddr, uintptr_t top_ioaddr,
			     uint32_t src_hz, uint32_t max_freq,
			     MtkMmcTuneReg tune_reg, int bus_width,
			     int removable, GpioOps *card_detect,
			     MtkMmcIpVersion version)
{
	MtkMmcHost *ctrlr = xzalloc(sizeof(*ctrlr));

	assert((max_freq <= MtkMmcMaxFreq) && (max_freq >= MtkMmcMinFreq));

	ctrlr->mmc.ctrlr.ops.is_bdev_owned = block_mmc_is_bdev_owned;
	ctrlr->mmc.ctrlr.ops.update = &mtk_mmc_update;
	ctrlr->mmc.ctrlr.need_update = 1;
	ctrlr->mmc.hardcoded_voltage = 0x40ff8080;

	ctrlr->mmc.voltages = MtkMmcVoltages;
	ctrlr->mmc.f_min = MtkMmcMinFreq;
	ctrlr->mmc.f_max = max_freq;
	ctrlr->tune_reg = tune_reg;

	ctrlr->mmc.bus_width = bus_width;
	ctrlr->mmc.bus_hz = ctrlr->mmc.f_min;
	ctrlr->mmc.b_max = 65535;	/* Some controllers use 16-bit regs. */

	if (bus_width == 8) {
		ctrlr->mmc.caps |= MMC_CAPS_8BIT;
		ctrlr->mmc.caps &= ~MMC_CAPS_4BIT;
	} else {
		ctrlr->mmc.caps |= MMC_CAPS_4BIT;
		ctrlr->mmc.caps &= ~MMC_CAPS_8BIT;
	}
	ctrlr->mmc.caps |= MMC_CAPS_HS | MMC_CAPS_HS_52MHz | MMC_CAPS_HC;
	if (max_freq > 100 * MHz && !removable)
		ctrlr->mmc.caps |= MMC_CAPS_HS200;
	ctrlr->mmc.send_cmd = &mtk_mmc_send_cmd;
	ctrlr->mmc.set_ios = &mtk_mmc_set_ios;
	ctrlr->mmc.execute_tuning = &mtk_mmc_execute_tuning;
	ctrlr->mmc.slot_type =
		removable ? MMC_SLOT_TYPE_REMOVABLE : MMC_SLOT_TYPE_EMBEDDED;

	ctrlr->src_hz = src_hz;
	ctrlr->reg = (MtkMmcReg *)ioaddr;
	ctrlr->top_reg = (MtkMmcTopReg *)top_ioaddr;
	ctrlr->cd_gpio = card_detect;
	ctrlr->version = version;

	return ctrlr;
}
