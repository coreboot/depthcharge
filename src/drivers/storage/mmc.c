/*
 * Copyright 2008, Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * Based vaguely on the Linux code
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

#include "config.h"
#include "drivers/storage/mmc.h"

/* Set block count limit because of 16 bit register limit on some hardware*/
#ifndef CONFIG_SYS_MMC_MAX_BLK_COUNT
#define CONFIG_SYS_MMC_MAX_BLK_COUNT 65535
#endif

/* Set to 1 to turn on debug messages. */
int __mmc_debug = 0;
int __mmc_trace = 0;

int mmc_busy_wait_io(volatile uint32_t *address, uint32_t *output,
		     uint32_t io_mask, uint32_t timeout_ms)
{
	uint32_t value = (uint32_t)-1;
	uint64_t start = timer_us(0);

	if (!output)
		output = &value;
	for (; *output & io_mask; *output = readl(address)) {
		if (timer_us(start) > timeout_ms * 1000)
			return -1;
	}
	return 0;
}

int mmc_busy_wait_io_until(volatile uint32_t *address, uint32_t *output,
			   uint32_t io_mask, uint32_t timeout_ms)
{
	uint32_t value = 0;
	uint64_t start = timer_us(0);

	if (!output)
		output = &value;
	for (; !(*output & io_mask); *output = readl(address)) {
		if (timer_us(start) > timeout_ms * 1000)
			return -1;
	}
	return 0;
}

static uint64_t extract_uint32_bits(const uint32_t *array, int start, int count)
{
	int i;
	uint64_t value = 0;

	for (i = 0; i < count; i++, start++) {
		value <<= 1;
		value |= (array[start / 32] >> (31 - (start % 32))) & 0x1;
	}
	return value;
}

static int mmc_send_cmd(MmcCtrlr *ctrlr, MmcCommand *cmd, MmcData *data)
{
	int ret = -1, retries = 2;

	mmc_trace("CMD_SEND:%d %p\n", cmd->cmdidx, ctrlr);
	mmc_trace("\tARG\t\t\t %#8.8x\n", cmd->cmdarg);
	mmc_trace("\tFLAG\t\t\t %d\n", cmd->flags);
	if (data) {
		mmc_trace("\t%s %d block(s) of %d bytes (%p)\n",
			  data->flags == MMC_DATA_READ ? "READ" : "WRITE",
			  data->blocks,
			  data->blocksize,
			  data->dest);
	}

	while (retries--) {
		ret = ctrlr->send_cmd(ctrlr, cmd, data);

		switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			mmc_trace("\tMMC_RSP_NONE\n");
			break;

		case MMC_RSP_R1:
			mmc_trace("\tMMC_RSP_R1,5,6,7 \t %#8.8x\n",
				  cmd->response[0]);
			break;

		case MMC_RSP_R1b:
			mmc_trace("\tMMC_RSP_R1b\t\t %#8.8x\n",
				  cmd->response[0]);
			break;

		case MMC_RSP_R2:
			mmc_trace("\tMMC_RSP_R2\t\t %#8.8x\n",
				  cmd->response[0]);
			mmc_trace("\t          \t\t %#8.8x\n",
				  cmd->response[1]);
			mmc_trace("\t          \t\t %#8.8x\n",
				  cmd->response[2]);
			mmc_trace("\t          \t\t %#8.8x\n",
				  cmd->response[3]);
			break;

		case MMC_RSP_R3:
			mmc_trace("\tMMC_RSP_R3,4\t\t %#8.8x\n",
				  cmd->response[0]);
			break;

		default:
			mmc_trace("\tERROR MMC rsp not supported\n");
			break;
		}
		mmc_trace("\trv:\t\t\t %d\n", ret);

		/* Retry failed data commands, bail out otherwise.  */
		if (!data || !ret)
			break;
	}
	return ret;
}

static int mmc_send_status(MmcMedia *media, int tries)
{
	MmcCommand cmd;
	cmd.cmdidx = MMC_CMD_SEND_STATUS;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = media->rca << 16;
	cmd.flags = 0;

	while (tries--) {
		int err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
		if (err)
			return err;
		else if (cmd.response[0] & MMC_STATUS_RDY_FOR_DATA)
			break;
		else if (cmd.response[0] & MMC_STATUS_MASK) {
			mmc_error("Status Error: %#8.8x\n", cmd.response[0]);
			return MMC_COMM_ERR;
		}

		udelay(100);
	}

	mmc_trace("CURR STATE:%d\n",
		  (cmd.response[0] & MMC_STATUS_CURR_STATE) >> 9);

	if (tries < 0) {
		mmc_error("Timeout waiting card ready\n");
		return MMC_TIMEOUT;
	}
	return 0;
}

static int mmc_set_blocklen(MmcCtrlr *ctrlr, int len)
{
	MmcCommand cmd;
	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;
	cmd.flags = 0;

	return mmc_send_cmd(ctrlr, &cmd, NULL);
}

static uint32_t mmc_write(MmcMedia *media, uint32_t start, lba_t block_count,
			  const void *src)
{
	MmcCommand cmd;
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	if (block_count > 1)
		cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;

	if (media->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * media->write_bl_len;

	MmcData data;
	data.src = src;
	data.blocks = block_count;
	data.blocksize = media->write_bl_len;
	data.flags = MMC_DATA_WRITE;

	if (mmc_send_cmd(media->ctrlr, &cmd, &data)) {
		mmc_error("mmc write failed\n");
		return 0;
	}

	/* SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	if ((block_count > 1) && !(media->ctrlr->caps & MMC_AUTO_CMD12)) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		if (mmc_send_cmd(media->ctrlr, &cmd, NULL)) {
			mmc_error("mmc fail to send stop cmd\n");
			return 0;
		}

		/* Waiting for the ready status */
		mmc_send_status(media, MMC_IO_RETRIES);
	}

	return block_count;
}

static int mmc_read(MmcMedia *media, void *dest, uint32_t start,
		    lba_t block_count)
{

	MmcCommand cmd;
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	if (block_count > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	if (media->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * media->read_bl_len;

	MmcData data;
	data.dest = dest;
	data.blocks = block_count;
	data.blocksize = media->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (mmc_send_cmd(media->ctrlr, &cmd, &data))
		return 0;

	if ((block_count > 1) && !(media->ctrlr->caps & MMC_AUTO_CMD12)) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		if (mmc_send_cmd(media->ctrlr, &cmd, NULL)) {
			mmc_error("mmc fail to send stop cmd\n");
			return 0;
		}

		/* Waiting for the ready status */
		mmc_send_status(media, MMC_IO_RETRIES);
	}

	return block_count;
}

static int mmc_go_idle(MmcMedia *media)
{
	// Some cards can't accept idle commands without delay.
	if (media->dev.removable)
		mdelay(1);

	MmcCommand cmd;
	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.flags = 0;

	int err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
	if (err)
		return err;

	// Some cards need more than half second to respond to next command (ex,
	// SEND_OP_COND).
	if (media->dev.removable)
		mdelay(2);

	return 0;
}

static int sd_send_op_cond(MmcMedia *media)
{
	int err;
	MmcCommand cmd;

	int tries = MMC_IO_RETRIES;
	while (tries--) {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
		if (err)
			return err;

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;

		/*
		 * Most cards do not answer if some reserved bits
		 * in the ocr are set. However, Some controller
		 * can set bit 7 (reserved for low voltages), but
		 * how to manage low voltages SD card is not yet
		 * specified.
		 */
		cmd.cmdarg = (media->ctrlr->voltages & 0xff8000);

		if (media->version == SD_VERSION_2)
			cmd.cmdarg |= OCR_HCS;

		err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
		if (err)
			return err;

		// OCR_BUSY means "initialization complete".
		if (cmd.response[0] & OCR_BUSY)
			break;

		udelay(100);
	}
	if (tries < 0)
		return MMC_UNUSABLE_ERR;

	if (media->version != SD_VERSION_2)
		media->version = SD_VERSION_1_0;

	media->ocr = cmd.response[0];
	media->high_capacity = ((media->ocr & OCR_HCS) == OCR_HCS);
	media->rca = 0;
	return 0;
}

/* We pass in the cmd since otherwise the init seems to fail */
static int mmc_send_op_cond_iter(MmcMedia *media, MmcCommand *cmd, int use_arg)
{
	cmd->cmdidx = MMC_CMD_SEND_OP_COND;
	cmd->resp_type = MMC_RSP_R3;

	if (use_arg) {
		uint32_t mask = media->op_cond_response &
			(OCR_VOLTAGE_MASK | OCR_ACCESS_MODE);
		cmd->cmdarg = media->ctrlr->voltages & mask;

		if (media->ctrlr->caps & MMC_MODE_HC)
			cmd->cmdarg |= OCR_HCS;
	}
	cmd->flags = 0;
	int err = mmc_send_cmd(media->ctrlr, cmd, NULL);
	if (err)
		return err;

	media->op_cond_response = cmd->response[0];
	return 0;
}

static int mmc_send_op_cond(MmcMedia *media)
{
	MmcCommand cmd;
	int max_iters;

	/* Some cards seem to need this */
	mmc_go_idle(media);

	/* Devices with hardcoded voltage do not need second iteration. */
	cmd.cmdarg = media->ctrlr->hardcoded_voltage;
	max_iters = cmd.cmdarg ? 1 : 2;

	/* Ask the card for its capabilities unless required to be hardcoded. */
	for (int i = 0; i < max_iters; i++) {
		int err = mmc_send_op_cond_iter(media, &cmd, i != 0);
		if (err)
			return err;

		// OCR_BUSY is active low, this bit set means
		// "initialization complete".
		if (media->op_cond_response & OCR_BUSY)
			return 0;
	}
	return MMC_IN_PROGRESS;
}

static int mmc_complete_op_cond(MmcMedia *media)
{
	MmcCommand cmd;

	int timeout = MMC_INIT_TIMEOUT_US;
	uint64_t start;

	start = timer_us(0);
	while (1) {
		// CMD1 queries whether initialization is done.
		int err = mmc_send_op_cond_iter(media, &cmd, 1);
		if (err)
			return err;

		// OCR_BUSY means "initialization complete".
		if (media->op_cond_response & OCR_BUSY)
			break;

		// Check if init timeout has expired.
		if (timer_us(start) > timeout)
			return MMC_UNUSABLE_ERR;

		udelay(100);
	}

	media->version = MMC_VERSION_UNKNOWN;
	media->ocr = cmd.response[0];

	media->high_capacity = ((media->ocr & OCR_HCS) == OCR_HCS);
	media->rca = 0;
	return 0;
}

static int mmc_send_ext_csd(MmcCtrlr *ctrlr, unsigned char *ext_csd)
{
	int rv;
	/* Get the Card Status Register */
	MmcCommand cmd;
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	MmcData data;
	data.dest = (char *)ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	rv = mmc_send_cmd(ctrlr, &cmd, &data);

	if (!rv && __mmc_trace) {
		int i, size;

		size = data.blocks * data.blocksize;
		mmc_trace("\t%p ext_csd:", ctrlr);
		for (i = 0; i < size; i++) {
			if (!(i % 32))
			    printf("\n");
			printf(" %2.2x", ext_csd[i]);
		}
		printf("\n");
	}
	return rv;
}

static int mmc_switch(MmcMedia *media, uint8_t set, uint8_t index,
		      uint8_t value)
{
	MmcCommand cmd;
	cmd.cmdidx = MMC_CMD_SWITCH;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = ((MMC_SWITCH_MODE_WRITE_BYTE << 24) |
			   (index << 16) | (value << 8));
	cmd.flags = 0;

	int ret = mmc_send_cmd(media->ctrlr, &cmd, NULL);

	/* Waiting for the ready status */
	mmc_send_status(media, MMC_IO_RETRIES);
	return ret;

}

static void mmc_set_bus_width(MmcCtrlr *ctrlr, uint32_t width)
{
	ctrlr->bus_width = width;
	ctrlr->set_ios(ctrlr);
}

static int mmc_change_freq(MmcMedia *media)
{
	char cardtype;
	int err;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, ext_csd, 512);

	media->caps = 0;

	/* Only version 4 supports high-speed */
	if (media->version < MMC_VERSION_4)
		return 0;

	err = mmc_send_ext_csd(media->ctrlr, ext_csd);
	if (err)
		return err;

	if (media->ctrlr->caps & MMC_MODE_HS_200MHz)
		cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0x1f;
	else
		cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;

	if (cardtype & MMC_HS_200MHZ) {
		/* Switch to 8-bit since HS200 only support 8-bit bus width */
		err = mmc_switch(media, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_8);
		if (err)
			return err;

		/* Switch to HS200 */
		err = mmc_switch(media, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_HS_TIMING, 0x2);
		if (err)
			return err;

		/* Adjust Host Bus Wisth to 8-bit */
		mmc_set_bus_width(media->ctrlr, 8);
		media->caps |= EXT_CSD_BUS_WIDTH_8;
	} else {
		err = mmc_switch(media, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_HS_TIMING, 1);
	}

	if (err)
		return err;

	/* Now check to see that it worked */
	err = mmc_send_ext_csd(media->ctrlr, ext_csd);
	if (err)
		return err;

	/* No high-speed support */
	if (!ext_csd[EXT_CSD_HS_TIMING])
		return 0;

	/* High Speed is set, there are types: HS200, 52MHz, 26MHz */
	if (cardtype & MMC_HS_200MHZ)
		media->caps |= (MMC_MODE_HS_200MHz
			| MMC_MODE_HS_52MHz | MMC_MODE_HS);
	else if (cardtype & MMC_HS_52MHZ)
		media->caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;
	else
		media->caps |= MMC_MODE_HS;
	return 0;
}

static int sd_switch(MmcCtrlr *ctrlr, int mode, int group, uint8_t value,
		     uint8_t *resp)
{
	/* Switch the frequency */
	MmcCommand cmd;
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | (0xffffff & ~(0xf << (group * 4))) |
		     (value << (group * 4));
	cmd.flags = 0;

	MmcData data;
	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(ctrlr, &cmd, &data);
}

static int sd_change_freq(MmcMedia *media)
{
	int err, timeout;
	MmcCommand cmd;
	MmcData data;
	ALLOC_CACHE_ALIGN_BUFFER(uint32_t, scr, 2);
	ALLOC_CACHE_ALIGN_BUFFER(uint32_t, switch_status, 16);

	media->caps = 0;

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = media->rca << 16;
	cmd.flags = 0;

	err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
	if (err)
		return err;

	mmc_debug("%s: before SD_CMD_APP_SEND_SCR\n", __func__);
	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	timeout = 3;
	while (timeout--) {
		data.dest = (char *)scr;
		data.blocksize = 8;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		err = mmc_send_cmd(media->ctrlr, &cmd, &data);
		if (!err)
			break;
	}
	if (err) {
		mmc_error("%s: return err (%d).\n", __func__, err);
		return err;
	}
	mmc_debug("%s: end SD_CMD_APP_SEND_SCR\n", __func__);

	media->scr[0] = betohl(scr[0]);
	media->scr[1] = betohl(scr[1]);

	switch ((media->scr[0] >> 24) & 0xf) {
		case 0:
			media->version = SD_VERSION_1_0;
			break;
		case 1:
			media->version = SD_VERSION_1_10;
			break;
		case 2:
			media->version = SD_VERSION_2;
			break;
		default:
			media->version = SD_VERSION_1_0;
			break;
	}

	if (media->scr[0] & SD_DATA_4BIT)
		media->caps |= MMC_MODE_4BIT;

	/* Version 1.0 doesn't support switching */
	if (media->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = sd_switch(media->ctrlr, SD_SWITCH_CHECK, 0, 1,
				(uint8_t *)switch_status);
		if (err)
			return err;

		/* The high-speed function is busy.  Try again */
		if (!(ntohl(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	/* If high-speed isn't supported, we return */
	if (!(ntohl(switch_status[3]) & SD_HIGHSPEED_SUPPORTED))
		return 0;

	/*
	 * If the host doesn't support SD_HIGHSPEED, do not switch card to
	 * HIGHSPEED mode even if the card support SD_HIGHSPPED.
	 * This can avoid furthur problem when the card runs in different
	 * mode between the host.
	 */
	if (!((media->ctrlr->caps & MMC_MODE_HS_52MHz) &&
		(media->ctrlr->caps & MMC_MODE_HS)))
		return 0;

	err = sd_switch(media->ctrlr, SD_SWITCH_SWITCH, 0, 1,
			(uint8_t *)switch_status);
	if (err)
		return err;

	if ((ntohl(switch_status[4]) & 0x0f000000) == 0x01000000)
		media->caps |= MMC_MODE_HS;
	return 0;
}

static void mmc_set_clock(MmcCtrlr *ctrlr, uint32_t clock)
{
	clock = MIN(clock, ctrlr->f_max);
	clock = MAX(clock, ctrlr->f_min);

	ctrlr->bus_hz = clock;
	ctrlr->set_ios(ctrlr);
}

static uint32_t mmc_calculate_transfer_speed(uint32_t csd0)
{
	uint32_t mult, freq;

	/* frequency bases, divided by 10 to be nice to platforms without
	 * floating point */
	static const int fbase[] = {
		10000,
		100000,
		1000000,
		10000000,
	};
	/* Multiplier values for TRAN_SPEED. Multiplied by 10 to be nice
	 * to platforms without floating point. */
	static const int multipliers[] = {
		0,  // reserved
		10,
		12,
		13,
		15,
		20,
		25,
		30,
		35,
		40,
		45,
		50,
		55,
		60,
		70,
		80,
	};

	/* divide frequency by 10, since the mults are 10x bigger */
	freq = fbase[csd0 & 0x7];
	mult = multipliers[(csd0 >> 3) & 0xf];
	return freq * mult;
}

static int mmc_startup(MmcMedia *media)
{
	int err, width;
	uint64_t cmult, csize, capacity;
	uint32_t clock = MMC_CLOCK_DEFAULT_MHZ;

	MmcCommand cmd;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, ext_csd, EXT_CSD_SIZE);
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, test_csd, EXT_CSD_SIZE);

	/* Put the Card in Identify Mode */
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;
	err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
	if (err)
		return err;
	memcpy(media->cid, cmd.response, sizeof(media->cid));

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */
	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = media->rca << 16;
	cmd.resp_type = MMC_RSP_R6;
	cmd.flags = 0;
	err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
	if (err)
		return err;
	if (IS_SD(media))
		media->rca = (cmd.response[0] >> 16) & 0xffff;

	/* Get the Card-Specific Data */
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = media->rca << 16;
	cmd.flags = 0;
	err = mmc_send_cmd(media->ctrlr, &cmd, NULL);

	/* Waiting for the ready status */
	mmc_send_status(media, MMC_IO_RETRIES);
	if (err)
		return err;

	memcpy(media->csd, cmd.response, sizeof(media->csd));
	if (media->version == MMC_VERSION_UNKNOWN) {
		int version = extract_uint32_bits(media->csd, 2, 4);
		switch (version) {
			case 0:
				media->version = MMC_VERSION_1_2;
				break;
			case 1:
				media->version = MMC_VERSION_1_4;
				break;
			case 2:
				media->version = MMC_VERSION_2_2;
				break;
			case 3:
				media->version = MMC_VERSION_3;
				break;
			case 4:
				media->version = MMC_VERSION_4;
				break;
			default:
				media->version = MMC_VERSION_1_2;
				break;
		}
	}

	media->tran_speed = mmc_calculate_transfer_speed(media->csd[0]);
	media->read_bl_len = 1 << extract_uint32_bits(media->csd, 44, 4);

	if (IS_SD(media))
		media->write_bl_len = media->read_bl_len;
	else
		media->write_bl_len = 1 << ((media->csd[3] >> 22) & 0xf);

	if (media->high_capacity) {
		cmult = 8;
		csize = extract_uint32_bits(media->csd, 58, 22);

	} else {
		csize = extract_uint32_bits(media->csd, 54, 12);
		cmult = extract_uint32_bits(media->csd, 78, 3);
	}

	media->capacity = (csize + 1) << (cmult + 2);
	media->capacity *= media->read_bl_len;

	if (media->read_bl_len > 512)
		media->read_bl_len = 512;

	if (media->write_bl_len > 512)
		media->write_bl_len = 512;

	mmc_debug("mmc media info: version=%#x, tran_speed=%d\n",
	      media->version, (int)media->tran_speed);

	/* Select the card, and put it into Transfer Mode */
	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = media->rca << 16;
	cmd.flags = 0;
	err = mmc_send_cmd(media->ctrlr, &cmd, NULL);

	if (err)
		return err;

	if (!IS_SD(media) && (media->version >= MMC_VERSION_4)) {
		/* check  ext_csd version and capacity */
		err = mmc_send_ext_csd(media->ctrlr, ext_csd);
		if (!err & (ext_csd[EXT_CSD_REV] >= 2)) {
			/* According to the JEDEC Standard, the value of
			 * ext_csd's capacity is valid if the value is more
			 * than 2GB */
			// TODO(hungte) Replace by letohl().
			capacity = (ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
				    ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
				    ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
				    ext_csd[EXT_CSD_SEC_CNT + 3] << 24);
			capacity *= 512;

			if ((capacity >> 20) > 2 * 1024)
				media->capacity = capacity;
		}
	}

	if (IS_SD(media))
		err = sd_change_freq(media);
	else
		err = mmc_change_freq(media);
	if (err)
		return err;

	/* Restrict card's capabilities by what the host can do */
	media->caps &= media->ctrlr->caps;

	if (IS_SD(media)) {
		if (media->caps & MMC_MODE_4BIT) {
			cmd.cmdidx = MMC_CMD_APP_CMD;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = media->rca << 16;
			cmd.flags = 0;

			err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
			if (err)
				return err;

			cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = 2;
			cmd.flags = 0;
			err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
			if (err)
				return err;

			mmc_set_bus_width(media->ctrlr, 4);
		}

		if (media->caps & MMC_MODE_HS)
			clock = MMC_CLOCK_50MHZ;
		else
			clock = MMC_CLOCK_25MHZ;
	} else {
		for (width = EXT_CSD_BUS_WIDTH_8; width >= 0; width--) {
			/* If HS200 is switched, Bus Width has been 8-bit */
			if (media->caps & MMC_MODE_HS_200MHz)
				break;

			/* Set the card to use 4 bit*/
			err = mmc_switch(media, EXT_CSD_CMD_SET_NORMAL,
					 EXT_CSD_BUS_WIDTH, width);
			if (err)
				continue;

			if (!width) {
				mmc_set_bus_width(media->ctrlr, 1);
				break;
			} else
				mmc_set_bus_width(media->ctrlr, 4 * width);

			err = mmc_send_ext_csd(media->ctrlr, test_csd);
			if (!err &&
			    (ext_csd[EXT_CSD_PARTITIONING_SUPPORT] ==
			    test_csd[EXT_CSD_PARTITIONING_SUPPORT]) &&
			    (ext_csd[EXT_CSD_ERASE_GROUP_DEF] ==
			    test_csd[EXT_CSD_ERASE_GROUP_DEF]) &&
			    (ext_csd[EXT_CSD_REV] ==
			    test_csd[EXT_CSD_REV]) &&
			    (ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] ==
			    test_csd[EXT_CSD_HC_ERASE_GRP_SIZE]) &&
			    memcmp(&ext_csd[EXT_CSD_SEC_CNT],
				   &test_csd[EXT_CSD_SEC_CNT], 4) == 0) {
				media->caps |= width;
				break;
			}
		}

		if (media->caps & MMC_MODE_HS) {
			if (media->caps & MMC_MODE_HS_200MHz)
				clock = MMC_CLOCK_200MHZ;
			else if (media->caps & MMC_MODE_HS_52MHz)
				clock = MMC_CLOCK_52MHZ;
			else
				clock = MMC_CLOCK_26MHZ;
		}
	}
	mmc_set_clock(media->ctrlr, clock);
	media->dev.block_count = media->capacity / media->read_bl_len;
	media->dev.block_size = media->read_bl_len;

	printf("Man %06x Snr %u ",
	       media->cid[0] >> 24,
	       (((media->cid[2] & 0xffff) << 16) |
		((media->cid[3] >> 16) & 0xffff)));
	printf("Product %c%c%c%c", media->cid[0] & 0xff,
	       (media->cid[1] >> 24), (media->cid[1] >> 16) & 0xff,
	       (media->cid[1] >> 8) & 0xff);
	if (!IS_SD(media)) /* eMMC product string is longer */
		printf("%c%c", media->cid[1] & 0xff,
		       (media->cid[2] >> 24) & 0xff);
	printf(" Revision %d.%d\n", (media->cid[2] >> 20) & 0xf,
	       (media->cid[2] >> 16) & 0xf);
	return 0;
}

static int mmc_send_if_cond(MmcMedia *media)
{
	MmcCommand cmd;
	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	// Set if host supports voltages between 2.7 and 3.6 V.
	cmd.cmdarg = ((media->ctrlr->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;
	cmd.flags = 0;
	int err = mmc_send_cmd(media->ctrlr, &cmd, NULL);
	if (err)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return MMC_UNUSABLE_ERR;
	else
		media->version = SD_VERSION_2;
	return 0;
}

int mmc_setup_media(MmcCtrlr *ctrlr)
{
	int err;

	MmcMedia *media = xzalloc(sizeof(*media));
	media->ctrlr = ctrlr;

	mmc_set_bus_width(ctrlr, 1);
	mmc_set_clock(ctrlr, 1);

	/* Reset the Card */
	err = mmc_go_idle(media);
	if (err) {
		free(media);
		return err;
	}

	/* Test for SD version 2 */
	err = mmc_send_if_cond(media);

	/* Get SD card operating condition */
	err = sd_send_op_cond(media);

	/* If the command timed out, we check for an MMC card */
	if (err == MMC_TIMEOUT) {
		err = mmc_send_op_cond(media);

		if (err && err != MMC_IN_PROGRESS) {
			mmc_error("Card did not respond to voltage select!\n");
			free(media);
			return MMC_UNUSABLE_ERR;
		}
	}

	if (err && err != MMC_IN_PROGRESS) {
		free(media);
		return err;
	}

	if (err == MMC_IN_PROGRESS)
		err = mmc_complete_op_cond(media);

	if (!err) {
		err = mmc_startup(media);
		if (!err) {
			ctrlr->media = media;
			return 0;
		}
	}

	free(media);
	return err;
}

/////////////////////////////////////////////////////////////////////////////
// BlockDevice utilities and callbacks

static inline MmcMedia *mmc_media(BlockDevOps *me)
{
	return container_of(me, MmcMedia, dev.ops);
}

static inline MmcCtrlr *mmc_ctrlr(MmcMedia *media)
{
	return media->ctrlr;
}

static int block_mmc_setup(BlockDevOps *me, lba_t start, lba_t count,
			   int is_read)
{
	MmcMedia *media = mmc_media(me);
	MmcCtrlr *ctrlr = mmc_ctrlr(media);

	if (count == 0)
		return 0;

	if (start > media->dev.block_count ||
	    start + count > media->dev.block_count)
		return 0;

	uint32_t bl_len = is_read ? media->read_bl_len :
		media->write_bl_len;

	if (mmc_set_blocklen(ctrlr, bl_len))
		return 0;

	return 1;
}

lba_t block_mmc_read(BlockDevOps *me, lba_t start, lba_t count, void *buffer)
{
	uint8_t *dest = (uint8_t *)buffer;

	if (block_mmc_setup(me, start, count, 1) == 0)
		return 0;

	lba_t todo = count;
	MmcMedia *media = mmc_media(me);
	MmcCtrlr *ctrlr = mmc_ctrlr(media);
	do {
		lba_t cur = MIN(todo, ctrlr->b_max);
		if (mmc_read(media, dest, start, cur) != cur)
			return 0;
		todo -= cur;
		mmc_debug("%s: Got %d blocks, more %d (total %d) to go.\n",
			  __func__, (int)cur, (int)todo, (int)count);
		start += cur;
		dest += cur * media->read_bl_len;
	} while (todo > 0);
	return count;
}

lba_t block_mmc_write(BlockDevOps *me, lba_t start, lba_t count,
		      const void *buffer)
{
	const uint8_t *src = (const uint8_t *)buffer;

	if (block_mmc_setup(me, start, count, 0) == 0)
		return 0;

	lba_t todo = count;
	MmcMedia *media = mmc_media(me);
	MmcCtrlr *ctrlr = mmc_ctrlr(media);
	do {
		lba_t cur = MIN(todo, ctrlr->b_max);
		if (mmc_write(media, start, cur, src) != cur)
			return 0;
		todo -= cur;
		start += cur;
		src += cur * media->write_bl_len;
	} while (todo > 0);
	return count;
}

lba_t block_mmc_fill_write(BlockDevOps *me, lba_t start, lba_t count,
			   uint8_t fill_byte)
{
	if (block_mmc_setup(me, start, count, 0) == 0)
		return 0;

	MmcMedia *media = mmc_media(me);
	MmcCtrlr *ctrlr = mmc_ctrlr(media);
	size_t block_size = media->dev.block_size;
	/*
	 * We allocate max 4 MiB buffer on heap and set it to fill_byte and
	 * perform mmc_write operation using this 4MiB buffer until requested
	 * size on disk is written by the fill byte.
	 *
	 * 4MiB was chosen after repeating several experiments with the max
	 * buffer size to be used. Using 1 lba i.e. block_size buffer results in
	 * very large fill_write time. On the other hand, choosing 4MiB, 8MiB or
	 * even 128 Mib resulted in similar write times. With 2MiB, the
	 * fill_write time increased by several seconds. So, 4MiB was chosen as
	 * the default max buffer size.
	 */
	lba_t heap_lba = (4 * MiB) / block_size;
	/*
	 * Actual allocated buffer size is minimum of three entities:
	 * 1) 4MiB equivalent in lba
	 * 2) count: Number of lbas to erase
	 * 3) ctrlr->b_max: Max lbas that the block device allows write
	 * operation on at a time.
	 */
	lba_t buffer_lba = MIN(MIN(heap_lba, count), ctrlr->b_max);

	size_t buffer_bytes = buffer_lba * block_size;
	uint8_t *buffer = xmalloc(buffer_bytes);
	memset(buffer, fill_byte, buffer_bytes);

	lba_t todo = count;
	int ret = 0;

	do {
		lba_t curr_lba = MIN(buffer_lba, todo);

		if (mmc_write(media, start, curr_lba, buffer) != curr_lba)
			goto cleanup;
		todo -= curr_lba;
		start += curr_lba;
	} while (todo > 0);

	ret = count;

cleanup:
	free(buffer);
	return ret;
}
