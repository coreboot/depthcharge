/*
 * Copyright (C) 2010-2014 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "config.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/ipq806x_mmc.h"

#define USEC_PER_SEC	(1000000L)
#define MAX_DELAY_MS	100

static int mmc_boot_send_command(MmcCtrlr *ctrlr,
				 MmcCommand *cmd, MmcData *data);

/*
 * Read a register in a loop until the masked read value matches the expected
 * value, up to MAX_DELAY_MS milliseconds.
 *
 * Save the last read value at the user provided address, if there is one.
 * Return success or error on timeout.
 */
static int wait_on_reg(void *reg_addr, uint32_t mask,
		   uint32_t expected_value, uint32_t  *last_read_value)
{
	unsigned start_ts;
	unsigned value;
	unsigned d = MAX_DELAY_MS * timer_hz() / 1000;

	start_ts = timer_raw_value(); /* Get initial time stamp. */

	do {
		value = readl(reg_addr);

		if ((timer_raw_value() - start_ts) > d) {
			mmc_error("Timeout waiting on %p for %#x:%#x\n",
				  reg_addr, mask, expected_value);
			return MMC_INVALID_ERR;
		}
	} while ((value & mask) != expected_value);

	if (last_read_value)
		*last_read_value = value;

	return MMC_BOOT_E_SUCCESS;
}

/*
 * mmc_mclk_reg_wr_delay should be called after every writes to
 * the MCI_POWER, MCI_CLK, MCI_CMD, or MCI_DATA_CTL register.
 * todo: add special purpose writel for this to encapsulate
 */
static void mmc_mclk_reg_wr_delay(MmcCtrlr *ctrlr)
{
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);

	/* Wait for the MMC_BOOT_MCI register write to go through. */
	wait_on_reg(MMC_BOOT_MCI_STATUS2(mmc_host->mci_base),
		    MMC_BOOT_MCI_MCLK_REG_WR_ACTIVE,
		    0, NULL);
}

void mmc_boot_mci_clk_enable(MmcCtrlr *ctrlr)
{
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);
	uint32_t reg;

	reg = (MMC_BOOT_MCI_CLK_ENABLE | MMC_BOOT_MCI_CLK_ENA_FLOW |
		MMC_BOOT_MCI_CLK_IN_FEEDBACK);

	writel(reg, MMC_BOOT_MCI_CLK(mmc_host->mci_base));

	/* Wait for the MMC_BOOT_MCI_CLK write to go through. */
	mmc_mclk_reg_wr_delay(ctrlr);

	/* Enable power save */
	reg |= MMC_BOOT_MCI_CLK_PWRSAVE;
	writel(reg, MMC_BOOT_MCI_CLK(mmc_host->mci_base));

	/* Wait for the MMC_BOOT_MCI_CLK write to go through. */
	mmc_mclk_reg_wr_delay(ctrlr);
}

/*
 * A command to set the data bus width for card. Set width to either
 * 1 BIT, 4 BIT or 8 BIT.
 */
static int mmc_boot_set_bus_width(MmcCtrlr *ctrlr, unsigned width)
{
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);
	uint32_t mmc_reg;

	/* set MCI_CLK accordingly */
	mmc_reg = readl(MMC_BOOT_MCI_CLK(mmc_host->mci_base));
	mmc_reg &= ~MMC_BOOT_MCI_CLK_WIDEBUS_MODE;

	switch (width) {
	case 1:
		mmc_reg |= MMC_BOOT_MCI_CLK_WIDEBUS_1_BIT;
		break;
	case 4:
		mmc_reg |= MMC_BOOT_MCI_CLK_WIDEBUS_4_BIT;
		break;
	case 8:
		mmc_reg |= MMC_BOOT_MCI_CLK_WIDEBUS_8_BIT;
		break;
	default:
		mmc_error("Invalid MMC bus width.");
		return MMC_UNUSABLE_ERR;
	}

	writel(mmc_reg, MMC_BOOT_MCI_CLK(mmc_host->mci_base));

	/* Wait for the MMC_BOOT_MCI_CLK write to go through. */
	mmc_mclk_reg_wr_delay(ctrlr);

	return MMC_BOOT_E_SUCCESS;
}

/*
 * Decode type of error caused during read and write
 */
static int mmc_boot_data_status_decode(unsigned mmc_status)
{
	if (!(mmc_status & (MMC_READ_ERROR|MMC_WRITE_ERROR)))
		return 0;

	/*
	 * If DATA_CRC_FAIL bit is set to 1 then CRC error was detected by
	 * card/device during the data transfer
	 */
	if (mmc_status & MMC_BOOT_MCI_STAT_DATA_CRC_FAIL)
		return MMC_COMM_ERR;

	/*
	 * If DATA_TIMEOUT bit is set to 1 then the data transfer time
	 * exceeded the data timeout period without completing the transfer
	 */
	if (mmc_status & MMC_BOOT_MCI_STAT_DATA_TIMEOUT)
		return MMC_TIMEOUT;

	/*
	 * If RX_OVERRUN bit is set to 1 then SDCC2 tried to receive data from
	 * the card before empty storage for new received data was available.
	 * Verify that bit FLOW_ENA in MCI_CLK is set to 1 during the data
	 * xfer.
	 */
	if (mmc_status & MMC_BOOT_MCI_STAT_RX_OVRRUN) {
		/*
		 * Note: We've set FLOW_ENA bit in MCI_CLK to 1. so no need to
		 * verify for now
		 */
		return MMC_COMM_ERR;
	}
	/*
	 * If TX_UNDERRUN bit is set to 1 then SDCC2 tried to send data to
	 * the card before new data for sending was available. Verify that bit
	 * FLOW_ENA in MCI_CLK is set to 1 during the data xfer.
	 */
	if (mmc_status & MMC_BOOT_MCI_STAT_TX_UNDRUN) {
		/*
		 * Note: We've set FLOW_ENA bit in MCI_CLK to 1.
		 * so skipping it now
		 */
		return MMC_COMM_ERR;
	}
	return MMC_INVALID_ERR;
}

/*
 * Read data from SDC FIFO.
 */
static int mmc_boot_fifo_read(MmcCtrlr *ctrlr, MmcData *data)
{
	int mmc_ret = MMC_BOOT_E_SUCCESS;
	uint32_t mmc_status;
	unsigned mmc_count = 0;
	uint32_t *mmc_ptr = (uint32_t *)data->dest;
	uint32_t data_len = data->blocks * data->blocksize;
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);

	/*
	 * Read the data from the MCI_FIFO register as long as RXDATA_AVLBL
	 * bit of MCI_STATUS register is set to 1 and bits DATA_CRC_FAIL,
	 * DATA_TIMEOUT, RX_OVERRUN of MCI_STATUS register are cleared to 0.
	 * Continue the reads until the whole transfer data is received
	 */
	while (1) {
		mmc_status = readl(MMC_BOOT_MCI_STATUS(mmc_host->mci_base));
		mmc_ret = mmc_boot_data_status_decode(mmc_status);
		if (mmc_ret != MMC_BOOT_E_SUCCESS)
			break;

		if (mmc_status & MMC_BOOT_MCI_STAT_RX_DATA_AVLBL) {
			unsigned i, read_count = 1;

			if (mmc_status & MMC_BOOT_MCI_STAT_RX_FIFO_HFULL)
				read_count = MMC_BOOT_MCI_HFIFO_COUNT;

			for (i = 0; i < read_count; i++) {
				/*
				 * FIFO contains 16 32-bit data buffer on
				 * 16 sequential addresses.
				 */
				*mmc_ptr = readl
					(MMC_BOOT_MCI_FIFO(mmc_host->mci_base)
					 +
					 (mmc_count % MMC_BOOT_MCI_FIFO_SIZE));
				mmc_ptr++;
				/* increase mmc_count by word size */
				mmc_count += sizeof(uint32_t);
			}
			/* quit if we have read enough of data */
			if (mmc_count == data_len)
				break;
		} else if (mmc_status & MMC_BOOT_MCI_STAT_DATA_END) {
			break;
		}
	}

	return mmc_ret;
}

/*
 * Write data to SDC FIFO.
 */
static int mmc_boot_fifo_write(MmcCtrlr *ctrlr, MmcData *data)
{

	int mmc_ret = MMC_BOOT_E_SUCCESS;
	uint32_t mmc_status;
	unsigned mmc_count = 0;
	unsigned count;
	uint32_t *mmc_ptr = (uint32_t *) data->src;
	uint32_t data_len = data->blocks * data->blocksize;
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);
	uint32_t mmc_reg;

	/*
	 * Set appropriate fields and write the MCI_DATA_CTL register.
	 * Set ENABLE bit to enable the data transfer.
	 */
	mmc_reg = MMC_BOOT_MCI_DATA_ENABLE;
	/*
	 * Clear DIRECTION bit to enable transfer from host to card.
	 * Clear MODE bit to enable block oriented data transfer.
	 * Set DM_ENABLE bit in order to enable DMA, otherwise clear it.
	 * Write size of block to be used during the data transfer to
	 * BLOCKSIZE field
	 *
	 * For MMC cards only, if stream data transfer mode is desired, set
	 * MODE bit.
	 */
	mmc_reg |= ctrlr->media->write_bl_len << MMC_BOOT_MCI_BLKSIZE_POS;
	writel(mmc_reg, MMC_BOOT_MCI_DATA_CTL(mmc_host->mci_base));

	/* Wait for the MMC_BOOT_MCI_DATA_CTL write to go through. */
	mmc_mclk_reg_wr_delay(ctrlr);

	/* Write the transfer data to SDCC3 FIFO */
	while (1) {
		mmc_status = readl(MMC_BOOT_MCI_STATUS(mmc_host->mci_base));

		/* Bytes left to write */
		count = data_len - mmc_count;

		/* Break if whole data is transferred */
		if (!count)
			break;

		/*
		 * Write half FIFO or less (remaining) words in MCI_FIFO as
		 * long as either TX_FIFO_EMPTY or TX_FIFO_HFULL bits of
		 * MCI_STATUS register are set.
		 */
		if ((mmc_status & MMC_BOOT_MCI_STAT_TX_FIFO_EMPTY) ||
			(mmc_status & MMC_BOOT_MCI_STAT_TX_FIFO_HFULL)) {
			/* Write minimum of half FIFO and remaining words */
			unsigned sz = ((count >> 2) >  MMC_BOOT_MCI_HFIFO_COUNT)
				? MMC_BOOT_MCI_HFIFO_COUNT : (count >> 2);

			for (int i = 0; i < sz; i++) {
				writel(*mmc_ptr,
				       MMC_BOOT_MCI_FIFO(mmc_host->mci_base));
				mmc_ptr++;
				/* increase mmc_count by word size */
				mmc_count += sizeof(uint32_t);
			}
		}
	}

	do {
		mmc_status = readl(MMC_BOOT_MCI_STATUS(mmc_host->mci_base));
		mmc_ret = mmc_boot_data_status_decode(mmc_status);
		if (mmc_ret != MMC_BOOT_E_SUCCESS)
			break;
	} while (!(mmc_status & MMC_BOOT_MCI_STAT_DATA_END));

	return mmc_ret;

}

static void mmc_boot_setup_data_transfer(MmcCtrlr *ctrlr, MmcData *data)
{
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);
	/* Set the FLOW_ENA bit of MCI_CLK register to 1 */
	uint32_t mmc_reg = readl(MMC_BOOT_MCI_CLK(mmc_host->mci_base));

	mmc_reg |= MMC_BOOT_MCI_CLK_ENA_FLOW;
	writel(mmc_reg, MMC_BOOT_MCI_CLK(mmc_host->mci_base));
	mmc_mclk_reg_wr_delay(ctrlr);

	/*
	 * If Data Mover is used for data transfer then prepare Command
	 * List Entry and enable the Data mover to work with SDCC2.
	 * Write data timeout period to MCI_DATA_TIMER register.
	 * Data timeout period should be in card bus clock periods
	 */
	mmc_reg = 0xFFFFFFFF;
	writel(mmc_reg, MMC_BOOT_MCI_DATA_TIMER(mmc_host->mci_base));

	/*
	 * Write the total size of the transfer data to MCI_DATA_LENGTH
	 * register. For block xfer it must be multiple of the block
	 * size.
	 */
	writel(data->blocks * data->blocksize,
	       MMC_BOOT_MCI_DATA_LENGTH(mmc_host->mci_base));

	if (data->flags & MMC_DATA_READ) {
		uint32_t mmc_reg;

		/*
		 * Set appropriate fields and write the MCI_DATA_CTL register.
		 * Set ENABLE bit to enable the data transfer.
		 */
		mmc_reg = MMC_BOOT_MCI_DATA_ENABLE;
		/* Set DIRECTION bit to enable transfer from card to host */
		mmc_reg |= MMC_BOOT_MCI_DATA_DIR;
		/*
		 * Clear MODE bit to enable block oriented data transfer. For
		 * MMC cards only, if stream data transfer mode is desired,
		 * set MODE bit.
		 * If DMA is to be used, Set DM_ENABLE bit.
		 * Write size of block to be used during the data transfer to
		 * BLOCKSIZE field
		 */
		mmc_reg |= (data->blocksize << MMC_BOOT_MCI_BLKSIZE_POS);
		writel(mmc_reg, MMC_BOOT_MCI_DATA_CTL(mmc_host->mci_base));

		/* Wait for the MMC_BOOT_MCI_DATA_CTL write to go through. */
		mmc_mclk_reg_wr_delay(ctrlr);
	}
}

/*
 * Check to ensure that there is no alignment or data length errors.
 */
static int mmc_boot_check_read_data(MmcCommand *cmd)
{
	if (cmd->response[0] & MMC_BOOT_R1_BLOCK_LEN_ERR)
		return MMC_UNUSABLE_ERR;

	/* Misaligned address not matching block length. */
	if (cmd->response[0] & MMC_BOOT_R1_ADDR_ERR)
		return MMC_UNUSABLE_ERR;

	return MMC_BOOT_E_SUCCESS;
}

/*
 * Wait for command to be executed.
 */
static int mmc_boot_wait_cmd_exec(MmcCtrlr *ctrlr,
				 MmcCommand *cmd, MmcData *data)
{
	uint32_t mmc_status;
	uint32_t mmc_resp;
	int mmc_return = MMC_BOOT_E_SUCCESS;
	unsigned cmd_index;
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);
	int i;

	cmd_index = cmd->cmdidx;
	while (1) {
		/* 3a. Read MCI_STATUS register */
		wait_on_reg(MMC_BOOT_MCI_STATUS(mmc_host->mci_base),
			    MMC_BOOT_MCI_STAT_CMD_ACTIVE,
			    0, NULL);

		mmc_status = readl(MMC_BOOT_MCI_STATUS(mmc_host->mci_base));

		/*
		 * 3b. CMD_SENT bit supposed to be set to 1 only after
		 * CMD0 is sent - no response required.
		 */
		if ((cmd->resp_type == MMC_RSP_NONE) &&
		    (mmc_status & MMC_BOOT_MCI_STAT_CMD_SENT)) {
			break;
		}

		/*
		 * 3c. If CMD_TIMEOUT bit is set then no response was
		 * received.
		 */
		if (mmc_status & MMC_BOOT_MCI_STAT_CMD_TIMEOUT) {
			mmc_return = MMC_TIMEOUT;
			break;
		}
		/*
		 * 3d. If CMD_RESPONSE_END bit is set to 1 then command's
		 * response was received and CRC check passed Special case for
		 * ACMD41: it seems to always fail CRC even if the response is
		 * valid.
		 */
		if ((mmc_status & MMC_BOOT_MCI_STAT_CMD_RESP_END)
			 || (cmd_index == MMC_CMD_SEND_OP_COND)
			 || (cmd_index == SD_CMD_SEND_IF_COND)) {
			/*
			 * 3i. Read MCI_RESP_CMD register to verify
			 * that response index is equal to command index.
			 */
			mmc_resp = readl
				(MMC_BOOT_MCI_RESP_CMD(mmc_host->mci_base)) &
				0x3F;

			/*
			 * However, long response does not contain the command
			 * index field. In that case, response index field must
			 * be set to 111111b (0x3F).
			 */
			if ((mmc_resp == cmd_index) ||
			    (cmd->resp_type == MMC_RSP_R2 ||
			     cmd->resp_type == MMC_RSP_R3 ||
			     cmd->resp_type == MMC_RSP_R6 ||
			     cmd->resp_type == MMC_RSP_R7)) {
				/*
				 * 3j. If resp index is equal to cmd index,
				 * read command resp from MCI_RESPn registers
				 * - MCI_RESP0/1/2/3 for CMD2/9/10 - MCI_RESP0
				 * for all other registers.
				 */
				if (cmd->resp_type == MMC_RSP_R2) {
					/*
					 * Matching the words to be in
					 * framework order: Last word first.
					 */
					for (i = 0; i < 4; i++) {
						cmd->response[i] =
						    readl(MMC_BOOT_MCI_RESP_0
						    (mmc_host->mci_base)
						    + (i * 4));
					}
				} else {
					cmd->response[0] =
					    readl(MMC_BOOT_MCI_RESP_0
					    (mmc_host->mci_base));
				}
			} else {
				/* command index mis-match. */
				mmc_return = MMC_COMM_ERR;
			}

			break;
		}

		/*
		 * 3e. If CMD_CRC_FAIL bit is set to 1 then cmd's response was
		 * recvd,but CRC check failed.
		 */
		if ((mmc_status & MMC_BOOT_MCI_STAT_CMD_CRC_FAIL)) {
			if (cmd_index == SD_CMD_APP_SEND_OP_COND)
				cmd->response[0] =
				readl
				(MMC_BOOT_MCI_RESP_0(mmc_host->mci_base));
			else
				mmc_return = MMC_COMM_ERR;
			break;
		}
	}

	return mmc_return;
}

/*
 * Sends specified command to a card and waits for a response.
 */
static int mmc_boot_send_command(MmcCtrlr *ctrlr,
				 MmcCommand *cmd, MmcData *data)
{
	uint32_t mmc_cmd;
	int mmc_return;
	uint32_t data_transfer = 0;
	uint32_t prg_enabled = 0;
	QcomMmcHost *mmc_host = container_of(ctrlr, QcomMmcHost, mmc);

	/* basic check */
	if (cmd == NULL)
		return MMC_UNUSABLE_ERR;

	/* data transfer */
	if (data != NULL)
		if (data->flags == MMC_DATA_READ ||
			data->flags == MMC_DATA_WRITE) {
			mmc_boot_setup_data_transfer(ctrlr, data);
			data_transfer = 1;
		}

	/* 1. Write command argument to MMC_BOOT_MCI_ARGUMENT register */
	writel(cmd->cmdarg, MMC_BOOT_MCI_ARGUMENT(mmc_host->mci_base));

	/*
	 * 2. Set appropriate fields and write MMC_BOOT_MCI_CMD.
	 * 2a. Write command index in CMD_INDEX field.
	 */
	mmc_cmd = cmd->cmdidx;
	/*
	 * 2b. Set RESPONSE bit for all cmds except CMD0. And dont set
	 * RESPONSE bit for commands with no response.
	 */
	if (cmd->resp_type != MMC_RSP_NONE)
		mmc_cmd |= MMC_BOOT_MCI_CMD_RESPONSE;

	/* 2c. Set LONGRESP bit for CMD2, CMD9 and CMD10. */
	if (cmd->resp_type == MMC_RSP_R2)
		mmc_cmd |= MMC_BOOT_MCI_CMD_LONGRSP;

	/*
	 * 2d. Set INTERRUPT bit to disable command timeout.
	 * 2e. Set PENDING bit for CMD12 in the beginning of stream mode data
	 * transfer

	if (cmd->xfer_mode == MMC_BOOT_XFER_MODE_STREAM) {
		mmc_cmd |= MMC_BOOT_MCI_CMD_PENDING;
	}
	*/

	/* 2f. Set ENABLE bit. */
	mmc_cmd |= MMC_BOOT_MCI_CMD_ENABLE;

	/* 2g. Set PROG_ENA bit */
	if (cmd->resp_type == MMC_RSP_R1b ||
		cmd->cmdidx == MMC_CMD_SEND_STATUS) {
		prg_enabled = 1;
		mmc_cmd |= MMC_BOOT_MCI_CMD_PROG_ENA;
	}

	/*
	 * 2h. Set MCIABORT bit for CMD12 when working with SDIO card. 2i.
	 * Set CCS_ENABLE bit for CMD61 when Command Completion Signal of
	 * CE-ATA device is enabled.
	 * 2j. clear all static status bits.
	 */
	writel(MMC_BOOT_MCI_STATIC_STATUS,
	       MMC_BOOT_MCI_CLEAR(mmc_host->mci_base));

	/* 2k. Write to MMC_BOOT_MCI_CMD register. */
	writel(mmc_cmd, MMC_BOOT_MCI_CMD(mmc_host->mci_base));

	/* Wait for the MMC_BOOT_MCI_CMD write to go through. */
	mmc_mclk_reg_wr_delay(ctrlr);

	mmc_trace("Command sent: CMD%d MCI_CMD_REG:%x MCI_ARG:%x\n",
		  cmd->cmdidx, mmc_cmd, cmd->cmdarg);

	/*
	 * 3. Wait for interrupt or poll on the following bits of MCI_STATUS
	 * register.
	 */
	mmc_return = mmc_boot_wait_cmd_exec(ctrlr, cmd, data);

	if (mmc_return) {
		/* Timeout is a valid condition for some commands. */
		if  (mmc_return != MMC_TIMEOUT)
			mmc_error("Command execution failed: %d\n", mmc_return);
		return mmc_return;
	}

	if (prg_enabled) {
		/*
		 * Wait for interrupt or poll on PROG_DONE bit of MCI_STATUS
		 * register.
		 * PROG_DONE bit set to 1 it means that the card finished its
		 * programming and stopped driving DAT0 line to 0.
		 */
		wait_on_reg(MMC_BOOT_MCI_STATUS(mmc_host->mci_base),
			    MMC_BOOT_MCI_STAT_PROG_DONE,
			    MMC_BOOT_MCI_STAT_PROG_DONE,
			    NULL);
	}

	if (data_transfer) {
		if (data->flags & MMC_DATA_READ) {
			mmc_return = mmc_boot_check_read_data(cmd);
			if (mmc_return) {
				mmc_error("Alignment/data length errors:%d\n",
					mmc_return);
				return mmc_return;
			}
		}

		/* Read the transfer data from SDCC FIFO. */
		if (data->flags == MMC_DATA_READ)
			mmc_return = mmc_boot_fifo_read(ctrlr, data);
		else
			mmc_return = mmc_boot_fifo_write(ctrlr, data);

		if (mmc_return != MMC_BOOT_E_SUCCESS) {
			mmc_error("RXFIFO ERROR:%d\n", mmc_return);
			return mmc_return;
		}

		/* Reset DPSM. */
		writel(0, MMC_BOOT_MCI_DATA_CTL(mmc_host->mci_base));

		/* Wait for the MMC_BOOT_MCI_DATA_CTL write to go through. */
		mmc_mclk_reg_wr_delay(ctrlr);
	}

	/* 2k. Write to MMC_BOOT_MCI_CMD register. */
	writel(0, MMC_BOOT_MCI_CMD(mmc_host->mci_base));

	/* Wait for the MMC_BOOT_MCI_CMD write to go through. */
	mmc_mclk_reg_wr_delay(ctrlr);

	return MMC_BOOT_E_SUCCESS;
}

static void mmc_boot_set_ios(struct MmcCtrlr *ctrlr)
{
	int mmc_ret = MMC_BOOT_E_SUCCESS;

	/* Clock frequency should not exceed 400KHz in identification mode. */
	clock_config_mmc(ctrlr, ctrlr->bus_hz);

	mmc_ret = mmc_boot_set_bus_width(ctrlr, ctrlr->bus_width);
	if (mmc_ret != MMC_BOOT_E_SUCCESS)
		mmc_error("Set bus width error %d\n", mmc_ret);
}

/*
 * Initialize host structure, set and enable clock-rate and power mode.
 */
static int mmc_boot_init(QcomMmcHost *host)
{
	uint32_t mmc_pwr;

	/* Initialize any clocks needed for SDC controller */
	clock_init_mmc(host->instance);

	/* Initialize the GPIO's required for the board. */
	board_mmc_gpio_config();

	/* Save the version of the mmc controller. */
	host->mmc_cont_version = readl(MMC_BOOT_MCI_VERSION(host->mci_base));

	/* Setup initial freq to 400KHz. */
	clock_config_mmc(&(host->mmc), MMC_CLK_400KHZ);

	/* set power mode. */
	mmc_pwr = (MMC_BOOT_MCI_PWR_ON | MMC_BOOT_MCI_PWR_UP);
	writel(mmc_pwr, MMC_BOOT_MCI_POWER(host->mci_base));

	/* Wait for the MMC_BOOT_MCI_POWER write to go through. */
	mmc_mclk_reg_wr_delay(&(host->mmc));

	/* Clear interrupt in mci status. */
	writel(0xFFFFFFFF, MMC_BOOT_MCI_CLEAR(host->mci_base));

	return MMC_BOOT_E_SUCCESS;
}

/*
 * Entry point to MMC boot process.
 */
static int qcom_mmc_update(BlockDevCtrlrOps *me)
{
	QcomMmcHost *mmc_host = container_of(me, QcomMmcHost, mmc.ctrlr.ops);
	int mmc_ret = MMC_BOOT_E_SUCCESS;

	/* Initialize necessary data structure and enable/set clock and power */
	mmc_trace("Initializing MMC host data structure and clock!\n");
	mmc_ret = mmc_boot_init(mmc_host);
	if (mmc_ret != MMC_BOOT_E_SUCCESS) {
		mmc_error("MMC Boot: Error Initializing MMC Card!!!\n");
		return -1;
	}

	if (mmc_setup_media(&(mmc_host->mmc)))
		return -1;

	mmc_host->mmc.media->dev.name = "qcom_mmc";
	mmc_host->mmc.media->dev.removable = 0;
	mmc_host->mmc.media->dev.ops.read = &block_mmc_read;
	mmc_host->mmc.media->dev.ops.write = &block_mmc_write;
	mmc_host->mmc.media->dev.ops.new_stream = &new_simple_stream;

	list_insert_after(&(mmc_host->mmc.media->dev.list_node),
				  &fixed_block_devices);
		mmc_host->mmc.ctrlr.need_update = 0;

	return 0;
}

QcomMmcHost *new_qcom_mmc_host(unsigned slot, uint32_t base, int bus_width)
{
	QcomMmcHost *new_host = xzalloc(sizeof(QcomMmcHost));

	if (new_host) {
		new_host->instance = slot;
		new_host->mci_base = (uint8_t *)base;

		new_host->mmc.ctrlr.ops.update = qcom_mmc_update;
		new_host->mmc.ctrlr.need_update = 1;
		new_host->mmc.set_ios = &mmc_boot_set_ios;
		new_host->mmc.send_cmd = mmc_boot_send_command;
		new_host->mmc.bus_hz = MMC_CLK_400KHZ;
		new_host->mmc.f_max = MMC_CLK_48MHZ;
		new_host->mmc.f_min = MMC_CLK_400KHZ;
		new_host->mmc.hardcoded_voltage =
			MMC_BOOT_OCR_27_36 | MMC_BOOT_OCR_SEC_MODE;
		new_host->mmc.voltages = MMC_BOOT_OCR_27_36;
		/* Some controllers use 16-bit regs.*/
		new_host->mmc.b_max = 0xFFFF;
		new_host->mmc.bus_width = bus_width;
		new_host->mmc.bus_hz = new_host->mmc.f_min;
		new_host->mmc.caps = (bus_width == 8) ?
					MMC_MODE_8BIT : MMC_MODE_4BIT;
		new_host->mmc.caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz |
			MMC_MODE_HC;
	}

	return new_host;
}
