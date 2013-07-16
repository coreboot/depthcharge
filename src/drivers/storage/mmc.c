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

#include "base/time.h"
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

int mmc_send_cmd(MmcDevice *mmc, MmcCommand *cmd, MmcData *data)
{
	int ret;
	mmc_trace("CMD_SEND:%d\n", cmd->cmdidx);
	mmc_trace("\tARG\t\t\t %#08X\n", cmd->cmdarg);
	mmc_trace("\tFLAG\t\t\t %d\n", cmd->flags);
	ret = mmc->send_cmd(mmc, cmd, data);

	switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			mmc_trace("\tMMC_RSP_NONE\n");
			break;

		case MMC_RSP_R1:
			mmc_trace("\tMMC_RSP_R1,5,6,7 \t %#08X \n",
				  cmd->response[0]);
			break;

		case MMC_RSP_R1b:
			mmc_trace("\tMMC_RSP_R1b\t\t %#08X \n",
				  cmd->response[0]);
			break;

		case MMC_RSP_R2:
			mmc_trace("\tMMC_RSP_R2\t\t %#08X \n",
				  cmd->response[0]);
			mmc_trace("\t          \t\t %#08X \n",
				  cmd->response[1]);
			mmc_trace("\t          \t\t %#08X \n",
				  cmd->response[2]);
			mmc_trace("\t          \t\t %#08X \n",
				  cmd->response[3]);
			break;

		case MMC_RSP_R3:
			mmc_trace("\tMMC_RSP_R3,4\t\t %#08X \n",
				  cmd->response[0]);
			break;

		default:
			mmc_trace("\tERROR MMC rsp not supported\n");
			break;
	}
	return ret;
}

int mmc_send_status(MmcDevice *mmc, int timeout)
{
	MmcCommand cmd;
	int err;

	cmd.cmdidx = MMC_CMD_SEND_STATUS;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	do {
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
		else if (cmd.response[0] & MMC_STATUS_RDY_FOR_DATA)
			break;

		mdelay(1);
		if (cmd.response[0] & MMC_STATUS_MASK) {
			mmc_error("Status Error: %#08X\n", cmd.response[0]);
			return MMC_COMM_ERR;
		}
	} while (timeout--);

	mmc_trace("CURR STATE:%d\n",
		  (cmd.response[0] & MMC_STATUS_CURR_STATE) >> 9);

	if (!timeout) {
		mmc_error("Timeout waiting card ready\n");
		return MMC_TIMEOUT;
	}
	return 0;
}

int mmc_set_blocklen(MmcDevice *mmc, int len)
{
	MmcCommand cmd;

	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;
	cmd.flags = 0;
	return mmc_send_cmd(mmc, &cmd, NULL);
}

uint32_t mmc_write(MmcDevice *mmc, uint32_t start, lba_t block_count,
		   const void *src)
{
	int timeout = MMC_IO_RETRIES;
	MmcCommand cmd;
	MmcData data;

	if ((start + block_count) > mmc->lba) {
		mmc_error("block number %#x exceeds max(%#x)\n",
			(int)(start + block_count), (int)mmc->lba);
		return 0;
	}

	if (block_count > 1)
		cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;

	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->write_bl_len;

	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	data.src = src;
	data.blocks = block_count;
	data.blocksize = mmc->write_bl_len;
	data.flags = MMC_DATA_WRITE;

	if (mmc_send_cmd(mmc, &cmd, &data)) {
		mmc_error("mmc write failed\n");
		return 0;
	}

	/* SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	if (block_count > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		if (mmc_send_cmd(mmc, &cmd, NULL)) {
			mmc_error("mmc fail to send stop cmd\n");
			return 0;
		}

		/* Waiting for the ready status */
		mmc_send_status(mmc, timeout);
	}

	return block_count;
}

int mmc_read(MmcDevice *mmc, void *dest, uint32_t start, lba_t block_count)
{
	int timeout = MMC_IO_RETRIES;
	MmcCommand cmd;
	MmcData data;

	if ((start + block_count) > mmc->lba) {
		mmc_error("block number %#x exceeds max(%#x)\n",
		      (int)(start + block_count), (int)mmc->lba);
		return 0;
	}

	if (block_count > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->read_bl_len;

	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	data.dest = dest;
	data.blocks = block_count;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (mmc_send_cmd(mmc, &cmd, &data))
		return 0;

	if (block_count > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		if (mmc_send_cmd(mmc, &cmd, NULL)) {
			mmc_error("mmc fail to send stop cmd\n");
			return 0;
		}

		/* Waiting for the ready status */
		mmc_send_status(mmc, timeout);
	}

	return block_count;
}

int mmc_go_idle(MmcDevice *mmc)
{
	MmcCommand cmd;
	int err;

	// TODO(hungte) Find out if we can get rid of this initial delay.
	mdelay(1);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	udelay(2000);
	return 0;
}

static int sd_send_op_cond(MmcDevice *mmc)
{
	int timeout = MMC_IO_RETRIES;
	int err;
	MmcCommand cmd;

	do {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);

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
		cmd.cmdarg = (mmc->voltages & 0xff8000);

		if (mmc->version == SD_VERSION_2)
			cmd.cmdarg |= OCR_HCS;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		udelay(1000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0)
		return MMC_UNUSABLE_ERR;

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	mmc->ocr = cmd.response[0];
	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;
	return 0;
}

/* We pass in the cmd since otherwise the init seems to fail */
static int mmc_send_op_cond_iter(MmcDevice *mmc, MmcCommand *cmd, int use_arg)
{
	int err;

	cmd->cmdidx = MMC_CMD_SEND_OP_COND;
	cmd->resp_type = MMC_RSP_R3;
	cmd->cmdarg = 0;

	if (use_arg) {
		cmd->cmdarg = ((mmc->voltages &
				(mmc->op_cond_response & OCR_VOLTAGE_MASK)) |
			       (mmc->op_cond_response & OCR_ACCESS_MODE));

		if (mmc->host_caps & MMC_MODE_HC)
			cmd->cmdarg |= OCR_HCS;
	}
	cmd->flags = 0;
	err = mmc_send_cmd(mmc, cmd, NULL);

	if (err)
		return err;

	mmc->op_cond_response = cmd->response[0];
	return 0;
}

static int mmc_send_op_cond(MmcDevice *mmc)
{
	MmcCommand cmd;
	int err, i;

	/* Some cards seem to need this */
	mmc_go_idle(mmc);

	/* Ask the card for its capabilities */
	mmc->op_cond_pending = 1;
	for (i = 0; i < 2; i++) {
		err = mmc_send_op_cond_iter(mmc, &cmd, i != 0);
		if (err)
			return err;

		/* exit if not busy (flag seems to be inverted) */
		if (mmc->op_cond_response & OCR_BUSY)
			return 0;
	}
	return MMC_IN_PROGRESS;
}

static int mmc_complete_op_cond(MmcDevice *mmc)
{
	MmcCommand cmd;
	int timeout = MMC_IO_RETRIES;
	uint32_t start;
	int err;

	mmc->op_cond_pending = 0;
	start = timer_us(0);
	do {
		err = mmc_send_op_cond_iter(mmc, &cmd, 1);
		if (err)
			return err;
		if (timer_us(start) > timeout * 1000)
			return MMC_UNUSABLE_ERR;
		udelay(100);
	} while (!(mmc->op_cond_response & OCR_BUSY));

	mmc->version = MMC_VERSION_UNKNOWN;
	mmc->ocr = cmd.response[0];

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;
	return 0;
}

static int mmc_send_ext_csd(MmcDevice *mmc, unsigned char *ext_csd)
{
	MmcCommand cmd;
	MmcData data;
	int err;

	/* Get the Card Status Register */
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	data.dest = (char *)ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);
	return err;
}

static int mmc_switch(MmcDevice *mmc, uint8_t set, uint8_t index, uint8_t value)
{
	int timeout = MMC_IO_RETRIES;
	int ret;
	MmcCommand cmd;

	cmd.cmdidx = MMC_CMD_SWITCH;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = ((MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		      (index << 16) | (value << 8));
	cmd.flags = 0;

	ret = mmc_send_cmd(mmc, &cmd, NULL);

	/* Waiting for the ready status */
	mmc_send_status(mmc, timeout);
	return ret;

}

static int mmc_change_freq(MmcDevice *mmc)
{
	char cardtype;
	int err;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, ext_csd, 512);

	mmc->card_caps = 0;

	/* Only version 4 supports high-speed */
	if (mmc->version < MMC_VERSION_4)
		return 0;

	err = mmc_send_ext_csd(mmc, ext_csd);

	if (err)
		return err;

	cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;
	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);

	if (err)
		return err;

	/* Now check to see that it worked */
	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err)
		return err;

	/* No high-speed support */
	if (!ext_csd[EXT_CSD_HS_TIMING])
		return 0;

	/* High Speed is set, there are two types: 52MHz and 26MHz */
	if (cardtype & MMC_HS_52MHZ)
		mmc->card_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;
	else
		mmc->card_caps |= MMC_MODE_HS;
	return 0;
}

int mmc_is_card_present(MmcDevice *mmc)
{
	if (mmc->is_card_present)
		return mmc->is_card_present(mmc);
	return 0;
}

static int sd_switch(MmcDevice *mmc, int mode, int group, uint8_t value,
		     uint8_t *resp)
{
	MmcCommand cmd;
	MmcData data;

	/* Switch the frequency */
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | 0xffffff;
	cmd.cmdarg &= ~(0xf << (group * 4));
	cmd.cmdarg |= value << (group * 4);
	cmd.flags = 0;

	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	return mmc_send_cmd(mmc, &cmd, &data);
}

static int sd_change_freq(MmcDevice *mmc)
{
	int err, timeout;
	MmcCommand cmd;
	MmcData data;
	ALLOC_CACHE_ALIGN_BUFFER(uint32_t, scr, 2);
	ALLOC_CACHE_ALIGN_BUFFER(uint32_t, switch_status, 16);

	mmc->card_caps = 0;

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
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
		err = mmc_send_cmd(mmc, &cmd, &data);
		if (!err)
			break;
	}
	if (err) {
		mmc_error("%s: return err (%d).\n", __func__, err);
		return err;
	}
	mmc_debug("%s: end SD_CMD_APP_SEND_SCR\n", __func__);

	mmc->scr[0] = betohl(scr[0]);
	mmc->scr[1] = betohl(scr[1]);

	switch ((mmc->scr[0] >> 24) & 0xf) {
		case 0:
			mmc->version = SD_VERSION_1_0;
			break;
		case 1:
			mmc->version = SD_VERSION_1_10;
			break;
		case 2:
			mmc->version = SD_VERSION_2;
			break;
		default:
			mmc->version = SD_VERSION_1_0;
			break;
	}

	if (mmc->scr[0] & SD_DATA_4BIT)
		mmc->card_caps |= MMC_MODE_4BIT;

	/* Version 1.0 doesn't support switching */
	if (mmc->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1,
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
	if (!((mmc->host_caps & MMC_MODE_HS_52MHz) &&
		(mmc->host_caps & MMC_MODE_HS)))
		return 0;

	err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, 1, (uint8_t *)switch_status);
	if (err)
		return err;

	if ((ntohl(switch_status[4]) & 0x0f000000) == 0x01000000)
		mmc->card_caps |= MMC_MODE_HS;
	return 0;
}

void mmc_set_ios(MmcDevice *mmc)
{
	mmc->set_ios(mmc);
}

void mmc_set_clock(MmcDevice *mmc, uint32_t clock)
{
	if (clock > mmc->f_max)
		clock = mmc->f_max;

	if (clock < mmc->f_min)
		clock = mmc->f_min;

	mmc->clock = clock;
	mmc_set_ios(mmc);
}

void mmc_set_bus_width(MmcDevice *mmc, uint32_t width)
{
	mmc->bus_width = width;
	mmc_set_ios(mmc);
}

uint32_t mmc_calculate_transfer_speed(uint32_t csd0)
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

int mmc_startup(MmcDevice *mmc)
{
	int err, width;
	uint64_t cmult, csize, capacity;
	int timeout = MMC_IO_RETRIES;
	uint32_t clock = MMC_CLOCK_DEFAULT_MHZ;

	MmcCommand cmd;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, ext_csd, EXT_CSD_SIZE);
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, test_csd, EXT_CSD_SIZE);

	/* Put the Card in Identify Mode */
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;
	memcpy(mmc->cid, cmd.response, sizeof(mmc->cid));

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */
	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;
	if (IS_SD(mmc))
		mmc->rca = (cmd.response[0] >> 16) & 0xffff;

	/* Get the Card-Specific Data */
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);

	/* Waiting for the ready status */
	mmc_send_status(mmc, timeout);
	if (err)
		return err;

	memcpy(mmc->csd, cmd.response, sizeof(mmc->csd));
	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = extract_uint32_bits(mmc->csd, 2, 4);
		switch (version) {
			case 0:
				mmc->version = MMC_VERSION_1_2;
				break;
			case 1:
				mmc->version = MMC_VERSION_1_4;
				break;
			case 2:
				mmc->version = MMC_VERSION_2_2;
				break;
			case 3:
				mmc->version = MMC_VERSION_3;
				break;
			case 4:
				mmc->version = MMC_VERSION_4;
				break;
			default:
				mmc->version = MMC_VERSION_1_2;
				break;
		}
	}

	mmc->tran_speed = mmc_calculate_transfer_speed(mmc->csd[0]);
	mmc->read_bl_len = 1 << extract_uint32_bits(mmc->csd, 44, 4);

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((mmc->csd[3] >> 22) & 0xf);

	if (mmc->high_capacity) {
		cmult = 8;
		csize = extract_uint32_bits(mmc->csd, 58, 22);

	} else {
		csize = extract_uint32_bits(mmc->csd, 54, 12);
		cmult = extract_uint32_bits(mmc->csd, 78, 3);
	}

	mmc->capacity = (csize + 1) << (cmult + 2);
	mmc->capacity *= mmc->read_bl_len;

	if (mmc->read_bl_len > 512)
		mmc->read_bl_len = 512;

	if (mmc->write_bl_len > 512)
		mmc->write_bl_len = 512;

	mmc_debug("mmc info: version=%#x, tran_speed=%d\n",
	      mmc->version, (int)mmc->tran_speed);

	/* Select the card, and put it into Transfer Mode */
	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	if (!IS_SD(mmc) && (mmc->version >= MMC_VERSION_4)) {
		/* check  ext_csd version and capacity */
		err = mmc_send_ext_csd(mmc, ext_csd);
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
				mmc->capacity = capacity;
		}
	}

	if (IS_SD(mmc))
		err = sd_change_freq(mmc);
	else
		err = mmc_change_freq(mmc);

	if (err)
		return err;

	/* Restrict card's capabilities by what the host can do */
	mmc->card_caps &= mmc->host_caps;

	if (IS_SD(mmc)) {
		if (mmc->card_caps & MMC_MODE_4BIT) {
			cmd.cmdidx = MMC_CMD_APP_CMD;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = mmc->rca << 16;
			cmd.flags = 0;

			err = mmc_send_cmd(mmc, &cmd, NULL);
			if (err)
				return err;

			cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = 2;
			cmd.flags = 0;
			err = mmc_send_cmd(mmc, &cmd, NULL);
			if (err)
				return err;

			mmc_set_bus_width(mmc, 4);
		}

		if (mmc->card_caps & MMC_MODE_HS)
			clock = MMC_CLOCK_50MHZ;
		else
			clock = MMC_CLOCK_25MHZ;
	} else {

		for (width = EXT_CSD_BUS_WIDTH_8; width >= 0; width--) {
			/* Set the card to use 4 bit*/
			err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
					 EXT_CSD_BUS_WIDTH, width);
			if (err)
				continue;

			if (!width) {
				mmc_set_bus_width(mmc, 1);
				break;
			} else
				mmc_set_bus_width(mmc, 4 * width);

			err = mmc_send_ext_csd(mmc, test_csd);
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
				mmc->card_caps |= width;
				break;
			}
		}

		if (mmc->card_caps & MMC_MODE_HS) {
			if (mmc->card_caps & MMC_MODE_HS_52MHz)
				clock = MMC_CLOCK_52MHZ;
			else
				clock = MMC_CLOCK_26MHZ;
		}
	}
	mmc_set_clock(mmc, clock);
	mmc->lba = mmc->capacity / mmc->read_bl_len;
	if (mmc->block_dev) {
		mmc->block_dev->block_count = mmc->lba;
		mmc->block_dev->block_size = mmc->read_bl_len;
	}
	return 0;
}

int mmc_send_if_cond(MmcDevice *mmc)
{
	MmcCommand cmd;
	int err;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* Set if host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((mmc->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return MMC_UNUSABLE_ERR;
	else
		mmc->version = SD_VERSION_2;
	return 0;
}

int mmc_start_init(MmcDevice *mmc)
{
	int err;

	if (mmc_is_card_present(mmc) == 0) {
		mmc->has_init = 0;
		mmc_debug("No card present\n");
		return MMC_NO_CARD_ERR;
	}

	if (mmc->has_init)
		return 0;

	err = mmc->init(mmc);
	if (err)
		return err;

	mmc_set_bus_width(mmc, 1);
	mmc_set_clock(mmc, 1);

	/* Reset the Card */
	err = mmc_go_idle(mmc);
	if (err)
		return err;

	/* Test for SD version 2 */
	err = mmc_send_if_cond(mmc);

	/* Get SD card operating condition */
	err = sd_send_op_cond(mmc);

	/* If the command timed out, we check for an MMC card */
	if (err == MMC_TIMEOUT) {
		err = mmc_send_op_cond(mmc);

		if (err && err != MMC_IN_PROGRESS) {
			mmc_error("Card did not respond to voltage select!\n");
			return MMC_UNUSABLE_ERR;
		}
	}

	if (err == MMC_IN_PROGRESS)
		mmc->init_in_progress = 1;
	return err;
}

static int mmc_complete_init(MmcDevice *mmc)
{
	int err = 0;

	if (mmc->op_cond_pending)
		err = mmc_complete_op_cond(mmc);

	if (!err)
		err = mmc_startup(mmc);
	if (err)
		mmc->has_init = 0;
	else
		mmc->has_init = 1;

	mmc->init_in_progress = 0;
	return err;
}

int mmc_init(MmcDevice *mmc)
{
	int err = MMC_IN_PROGRESS;

	if (mmc->has_init)
		return 0;
	if (!mmc->init_in_progress)
		err = mmc_start_init(mmc);
	if (!err || err == MMC_IN_PROGRESS)
		err = mmc_complete_init(mmc);
	return err;
}

/////////////////////////////////////////////////////////////////////////////
// BlockDevice utilities and callbacks

void block_mmc_refresh(ListNode *block_node, MmcDevice *mmc)
{
	assert(mmc && mmc->block_dev);
	mmc_debug("%s: %s\n",mmc->block_dev->name, __func__);
	if (mmc->has_init) {
		list_remove(&mmc->block_dev->list_node);
		mmc->has_init = 0;
	}
	if (mmc_init(mmc) == 0) {
		list_insert_after(&mmc->block_dev->list_node, block_node);
		printf("%s: %s: Card present.\n", __func__,
		       mmc->block_dev->name);
	} else {
		printf("%s: %s: No card present.\n", __func__,
		       mmc->block_dev->name);
	}
}

void block_mmc_ctrlr_refresh(BlockDevCtrlr *ctrlr)
{
	MmcDevice *mmc = (MmcDevice *)ctrlr->ctrlr_data;
	mmc_debug("%s: enter (root=%p).\n", __func__, mmc);
	for (; mmc; mmc = mmc->next) {
		block_mmc_refresh(&removable_block_devices, mmc);
	}
	mmc_debug("%s: leave.\n", __func__);
}

int block_mmc_register(BlockDev *dev, MmcDevice *mmc, MmcDevice **root)
{
	if (!mmc->b_max)
		mmc->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;
	mmc->block_dev = dev;
	dev->dev_data = mmc;
	dev->read = block_mmc_read;
	dev->write = block_mmc_write;
	if (root) {
		while (*root && (*root)->next)
			root = &(*root)->next;
		if (*root)
			(*root)->next = mmc;
		else
			*root = mmc;
	}
	return 0;
}

lba_t block_mmc_read(BlockDev *dev, lba_t start, lba_t count, void *buffer)
{
	MmcDevice *mmc = (MmcDevice *)dev->dev_data;
	lba_t cur, blocks_todo = count;
	uint8_t *dest = (uint8_t *)buffer;

	if (count == 0 || !mmc)
		return 0;
	if (mmc_set_blocklen(mmc, mmc->read_bl_len))
		return 0;

	do {
		cur = (blocks_todo > mmc->b_max) ?  mmc->b_max : blocks_todo;
		if(mmc_read(mmc, dest, start, cur) != cur)
			return 0;
		blocks_todo -= cur;
		mmc_debug("%s: Got %d blocks, more %d (total %d) to go.\n",
			  __func__, (int)cur, (int)blocks_todo, (int)count);
		start += cur;
		dest += cur * mmc->read_bl_len;
	} while (blocks_todo > 0);
	return count;
}

lba_t block_mmc_write(BlockDev *dev, lba_t start, lba_t count,
		      const void *buffer)
{
	MmcDevice *mmc = (MmcDevice *)dev->dev_data;
	const uint8_t *src = (const uint8_t *)buffer;
	lba_t cur, blocks_todo = count;

	if (count == 0 || !mmc)
		return 0;

	if (mmc_set_blocklen(mmc, mmc->write_bl_len))
		return 0;

	do {
		cur = (blocks_todo > mmc->b_max) ?  mmc->b_max : blocks_todo;
		if(mmc_write(mmc, start, cur, src) != cur)
			return 0;
		blocks_todo -= cur;
		start += cur;
		src += cur * mmc->write_bl_len;
	} while (blocks_todo > 0);
	return count;
}
