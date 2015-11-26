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

#ifndef __DRIVERS_STORAGE_IPQ40XX_MMC_H__
#define __DRIVERS_STORAGE_IPQ40XX_MMC_H__

#include "drivers/storage/mmc.h"

#ifndef MMC_SLOT
#define MMC_SLOT            0
#endif

extern int __mmc_debug;
extern int __mmc_trace;

#define MMC_BOOT_MCI_REG(base, offset)          ((base) + offset)

/*
 * Define Macros for SDCC Registers
 */
#define MMC_BOOT_MCI_POWER(base)  MMC_BOOT_MCI_REG(base, 0x000)	/* 8 bit */

#define MMC_BOOT_MCI_OPEN_DRAIN           (1 << 6)
#define MMC_BOOT_MCI_PWR_OFF              0x00
#define MMC_BOOT_MCI_PWR_UP               0x02
#define MMC_BOOT_MCI_PWR_ON               0x03
#define MMC_BOOT_MCI_SW_RST		(1 << 7)

#define MMC_BOOT_MCI_CLK(base)    MMC_BOOT_MCI_REG(base, 0x004)	/* 16 bits */
/* Enable MCI bus clock - 0: clock disabled 1: enabled */
#define MMC_BOOT_MCI_CLK_ENABLE           (1 << 8)
#define MMC_BOOT_MCI_CLK_DISABLE          (0 << 8)
/* Disable clk o/p when bus idle- 0:always enabled 1:enabled when bus active */
#define MMC_BOOT_MCI_CLK_PWRSAVE          (1 << 9)
/* Enable Widebus mode - 00: 1 bit mode 10:4 bit mode 01/11: 8 bit mode */
#define MMC_BOOT_MCI_CLK_WIDEBUS_MODE     (3 << 10)
#define MMC_BOOT_MCI_CLK_WIDEBUS_1_BIT    0
#define MMC_BOOT_MCI_CLK_WIDEBUS_4_BIT    (2 << 10)
#define MMC_BOOT_MCI_CLK_WIDEBUS_8_BIT    (1 << 10)
/* Enable flow control- 0: disable 1: enable */
#define MMC_BOOT_MCI_CLK_ENA_FLOW         (1 << 12)
/* Set/clear to select rising/falling edge for data/cmd output */
#define MMC_BOOT_MCI_CLK_INVERT_OUT       (1 << 13)
/* Select to lach data/cmd coming in falling/rising/feedbk/loopbk of MCIclk */
#define MMC_BOOT_MCI_CLK_IN_FALLING       0x0
#define MMC_BOOT_MCI_CLK_IN_RISING        (1 << 14)
#define MMC_BOOT_MCI_CLK_IN_FEEDBACK      (2 << 14)
#define MMC_BOOT_MCI_CLK_IN_LOOPBACK      (3 << 14)

/* Bus Width */
#define MMC_BOOT_BUS_WIDTH_1_BIT          0
#define MMC_BOOT_BUS_WIDTH_4_BIT          2
#define MMC_BOOT_BUS_WIDTH_8_BIT          3

/* Bus width support for DDR mode */
#define MMC_DDR_BUS_WIDTH_4_BIT           6
#define MMC_DDR_BUS_WIDTH_8_BIT           7

/* DDR mode select */
#define MMC_MCI_MODE_SELECT               14
#define MMC_MCI_DDR_MODE_EN               0x3

#define MMC_DEVICE_TYPE                   196

/* 32 bits */
#define MMC_BOOT_MCI_ARGUMENT(base) MMC_BOOT_MCI_REG(base, 0x008)

/* 16 bits */
#define MMC_BOOT_MCI_CMD(base)      MMC_BOOT_MCI_REG(base, 0x00C)

/* Command Index: 0 -5 */
/* Waits for response if set */
#define MMC_BOOT_MCI_CMD_RESPONSE         (1 << 6)
/* Receives a 136-bit long response if set */
#define MMC_BOOT_MCI_CMD_LONGRSP          (1 << 7)
/* If set, CPSM disables command timer and waits for interrupt */
#define MMC_BOOT_MCI_CMD_INTERRUPT        (1 << 8)
/* If set waits for CmdPend before starting to send a command */
#define MMC_BOOT_MCI_CMD_PENDING          (1 << 9)
/* CPSM is enabled if set */
#define MMC_BOOT_MCI_CMD_ENABLE           (1 << 10)
/* If set PROG_DONE status bit asserted when busy is de-asserted */
#define MMC_BOOT_MCI_CMD_PROG_ENA         (1 << 11)
/* To indicate that this is a Command with Data (for SDIO interrupts) */
#define MMC_BOOT_MCI_CMD_DAT_CMD          (1 << 12)
/* Signals the next command to be an abort (stop) command. Always read 0 */
#define MMC_BOOT_MCI_CMD_MCIABORT         (1 << 13)
/* Waits for Command Completion Signal if set */
#define MMC_BOOT_MCI_CMD_CCS_ENABLE       (1 << 14)
/* If set sends CCS disable sequence */
#define MMC_BOOT_MCI_CMD_CCS_DISABLE      (1 << 15)

#define MMC_BOOT_MCI_RESP_CMD(base)             MMC_BOOT_MCI_REG(base, 0x010)

#define MMC_BOOT_MCI_RESP_0(base)               MMC_BOOT_MCI_REG(base, 0x014)
#define MMC_BOOT_MCI_RESP_1(base)               MMC_BOOT_MCI_REG(base, 0x018)
#define MMC_BOOT_MCI_RESP_2(base)               MMC_BOOT_MCI_REG(base, 0x01C)
#define MMC_BOOT_MCI_RESP_3(base)               MMC_BOOT_MCI_REG(base, 0x020)

#define MMC_BOOT_MCI_DATA_TIMER(base)           MMC_BOOT_MCI_REG(base, 0x024)
#define MMC_BOOT_MCI_DATA_LENGTH(base)          MMC_BOOT_MCI_REG(base, 0x028)

/* 16 bits */
#define MMC_BOOT_MCI_DATA_CTL(base)             MMC_BOOT_MCI_REG(base, 0x02C)

/* Data transfer enabled */
#define MMC_BOOT_MCI_DATA_ENABLE          (1 << 0)
/* Data transfer direction - 0: controller to card 1:card to controller */
#define MMC_BOOT_MCI_DATA_DIR             (1 << 1)
/* Data transfer mode - 0: block data transfer 1: stream data transfer */
#define MMC_BOOT_MCI_DATA_MODE            (1 << 2)
/* Enable DM interface - 0: DM disabled 1: DM enabled */
#define MMC_BOOT_MCI_DATA_DM_ENABLE       (1 << 3)
/* Data block length in bytes (1-4096) */
#define MMC_BOOT_MCI_BLKSIZE_POS          4
#define MMC_BOOT_MCI_DATA_COUNT(base)           MMC_BOOT_MCI_REG(base, 0x030)
#define MMC_BOOT_MCI_STATUS(base)               MMC_BOOT_MCI_REG(base, 0x034)
/* Command response received - CRC check failed */
#define MMC_BOOT_MCI_STAT_CMD_CRC_FAIL    (1 << 0)
/* Data block sent/received - CRC check failed */
#define MMC_BOOT_MCI_STAT_DATA_CRC_FAIL   (1 << 1)
/* Command resonse timeout */
#define MMC_BOOT_MCI_STAT_CMD_TIMEOUT     (1 << 2)
/* Data timeout */
#define MMC_BOOT_MCI_STAT_DATA_TIMEOUT    (1 << 3)
/* Transmit FIFO underrun error */
#define MMC_BOOT_MCI_STAT_TX_UNDRUN       (1 << 4)
/* Receive FIFO overrun error */
#define MMC_BOOT_MCI_STAT_RX_OVRRUN       (1 << 5)
/* Command response received - CRC check passed */
#define MMC_BOOT_MCI_STAT_CMD_RESP_END    (1 << 6)
/* Command sent - no response required */
#define MMC_BOOT_MCI_STAT_CMD_SENT        (1 << 7)
/* Data end - data counter zero */
#define MMC_BOOT_MCI_STAT_DATA_END        (1 << 8)
/* Start bit not detected on all data signals in wide bus mode */
#define MMC_BOOT_MCI_STAT_START_BIT_ERR   (1 << 9)
/* Data block sent/received - CRC check passed */
#define MMC_BOOT_MCI_STAT_DATA_BLK_END    (1 << 10)
/* Command transfer in progress */
#define MMC_BOOT_MCI_STAT_CMD_ACTIVE      (1 << 11)
/* Data transmit in progress */
#define MMC_BOOT_MCI_STAT_TX_ACTIVE       (1 << 12)
/* Data receive in progress */
#define MMC_BOOT_MCI_STAT_RX_ACTIVE       (1 << 13)
/* Transmit FIFO half full */
#define MMC_BOOT_MCI_STAT_TX_FIFO_HFULL   (1 << 14)
/* Receive FIFO half full */
#define MMC_BOOT_MCI_STAT_RX_FIFO_HFULL   (1 << 15)
/* Transmit FIFO full */
#define MMC_BOOT_MCI_STAT_TX_FIFO_FULL    (1 << 16)
/* Receive FIFO full */
#define MMC_BOOT_MCI_STAT_RX_FIFO_FULL    (1 << 17)
/* Transmit FIFO empty */
#define MMC_BOOT_MCI_STAT_TX_FIFO_EMPTY   (1 << 18)
/* Receive FIFO empty */
#define MMC_BOOT_MCI_STAT_RX_FIFO_EMPTY   (1 << 19)
/* Data available in transmit FIFO */
#define MMC_BOOT_MCI_STAT_TX_DATA_AVLBL   (1 << 20)
/* Data available in receive FIFO */
#define MMC_BOOT_MCI_STAT_RX_DATA_AVLBL   (1 << 21)
/* SDIO interrupt indicator for wake-up */
#define MMC_BOOT_MCI_STAT_SDIO_INTR       (1 << 22)
/* Programming done */
#define MMC_BOOT_MCI_STAT_PROG_DONE       (1 << 23)
/* CE-ATA command completion signal detected */
#define MMC_BOOT_MCI_STAT_ATA_CMD_CMPL    (1 << 24)
/* SDIO interrupt indicator for normal operation */
#define MMC_BOOT_MCI_STAT_SDIO_INTR_OP    (1 << 25)
/* Commpand completion signal timeout */
#define MMC_BOOT_MCI_STAT_CCS_TIMEOUT     (1 << 26)

#define MMC_BOOT_MCI_STATIC_STATUS        (MMC_BOOT_MCI_STAT_CMD_CRC_FAIL| \
		MMC_BOOT_MCI_STAT_DATA_CRC_FAIL| \
		MMC_BOOT_MCI_STAT_CMD_TIMEOUT| \
		MMC_BOOT_MCI_STAT_DATA_TIMEOUT| \
		MMC_BOOT_MCI_STAT_TX_UNDRUN| \
		MMC_BOOT_MCI_STAT_RX_OVRRUN| \
		MMC_BOOT_MCI_STAT_CMD_RESP_END| \
		MMC_BOOT_MCI_STAT_CMD_SENT| \
		MMC_BOOT_MCI_STAT_DATA_END| \
		MMC_BOOT_MCI_STAT_START_BIT_ERR| \
		MMC_BOOT_MCI_STAT_DATA_BLK_END| \
		MMC_BOOT_MCI_SDIO_INTR_CLR| \
		MMC_BOOT_MCI_STAT_PROG_DONE| \
		MMC_BOOT_MCI_STAT_ATA_CMD_CMPL |\
		MMC_BOOT_MCI_STAT_SDIO_INTR |\
		MMC_BOOT_MCI_STAT_SDIO_INTR_OP |\
		MMC_BOOT_MCI_STAT_CCS_TIMEOUT)

#define MMC_BOOT_MCI_CLEAR(base)                MMC_BOOT_MCI_REG(base, 0x038)
#define MMC_BOOT_MCI_CMD_CRC_FAIL_CLR     (1 << 0)
#define MMC_BOOT_MCI_DATA_CRC_FAIL_CLR    (1 << 1)
#define MMC_BOOT_MCI_CMD_TIMEOUT_CLR      (1 << 2)
#define MMC_BOOT_MCI_DATA_TIMEOUT_CLR     (1 << 3)
#define MMC_BOOT_MCI_TX_UNDERRUN_CLR      (1 << 4)
#define MMC_BOOT_MCI_RX_OVERRUN_CLR       (1 << 5)
#define MMC_BOOT_MCI_CMD_RESP_END_CLR     (1 << 6)
#define MMC_BOOT_MCI_CMD_SENT_CLR         (1 << 7)
#define MMC_BOOT_MCI_DATA_END_CLR         (1 << 8)
#define MMC_BOOT_MCI_START_BIT_ERR_CLR    (1 << 9)
#define MMC_BOOT_MCI_DATA_BLK_END_CLR     (1 << 10)
#define MMC_BOOT_MCI_SDIO_INTR_CLR        (1 << 22)
#define MMC_BOOT_MCI_PROG_DONE_CLR        (1 << 23)
#define MMC_BOOT_MCI_ATA_CMD_COMPLR_CLR   (1 << 24)
#define MMC_BOOT_MCI_CCS_TIMEOUT_CLR      (1 << 25)

#define MMC_BOOT_MCI_INT_MASK0(base)            MMC_BOOT_MCI_REG(base, 0x03C)
#define MMC_BOOT_MCI_CMD_CRC_FAIL_MASK    (1 << 0)
#define MMC_BOOT_MCI_DATA_CRC_FAIL_MASK   (1 << 1)
#define MMC_BOOT_MCI_CMD_TIMEOUT_MASK     (1 << 2)
#define MMC_BOOT_MCI_DATA_TIMEOUT_MASK    (1 << 3)
#define MMC_BOOT_MCI_TX_OVERRUN_MASK      (1 << 4)
#define MMC_BOOT_MCI_RX_OVERRUN_MASK      (1 << 5)
#define MMC_BOOT_MCI_CMD_RESP_END_MASK    (1 << 6)
#define MMC_BOOT_MCI_CMD_SENT_MASK        (1 << 7)
#define MMC_BOOT_MCI_DATA_END_MASK        (1 << 8)
#define MMC_BOOT_MCI_START_BIT_ERR_MASK   (1 << 9)
#define MMC_BOOT_MCI_DATA_BLK_END_MASK    (1 << 10)
#define MMC_BOOT_MCI_CMD_ACTIVE_MASK      (1 << 11)
#define MMC_BOOT_MCI_TX_ACTIVE_MASK       (1 << 12)
#define MMC_BOOT_MCI_RX_ACTIVE_MASK       (1 << 13)
#define MMC_BOOT_MCI_TX_FIFO_HFULL_MASK   (1 << 14)
#define MMC_BOOT_MCI_RX_FIFO_HFULL_MASK   (1 << 15)
#define MMC_BOOT_MCI_TX_FIFO_FULL_MASK    (1 << 16)
#define MMC_BOOT_MCI_RX_FIFO_FULL_MASK    (1 << 17)
#define MMC_BOOT_MCI_TX_FIFO_EMPTY_MASK   (1 << 18)
#define MMC_BOOT_MCI_RX_FIFO_EMPTY_MASK   (1 << 19)
#define MMC_BOOT_MCI_TX_DATA_AVLBL_MASK   (1 << 20)
#define MMC_BOOT_MCI_RX_DATA_AVLBL_MASK   (1 << 21)
#define MMC_BOOT_MCI_SDIO_INT_MASK        (1 << 22)
#define MMC_BOOT_MCI_PROG_DONE_MASK       (1 << 23)
#define MMC_BOOT_MCI_ATA_CMD_COMPL_MASK   (1 << 24)
#define MMC_BOOT_MCI_SDIO_INT_OPER_MASK   (1 << 25)
#define MMC_BOOT_MCI_CCS_TIME_OUT_MASK    (1 << 26)

#define MMC_BOOT_MCI_INT_MASK1(base)            MMC_BOOT_MCI_REG(base, 0x040)

#define MMC_BOOT_MCI_FIFO_COUNT(base)           MMC_BOOT_MCI_REG(base, 0x044)

#define MMC_BOOT_MCI_VERSION(base)              MMC_BOOT_MCI_REG(base, 0x050)

#define MMC_BOOT_MCI_CCS_TIMER(base)            MMC_BOOT_MCI_REG(base, 0x0058)

#define MMC_BOOT_MCI_STATUS2(base)              MMC_BOOT_MCI_REG(base, 0x06C)
#define MMC_BOOT_MCI_MCLK_REG_WR_ACTIVE   (1 << 0)

#define MMC_BOOT_MCI_FIFO(base)                 MMC_BOOT_MCI_REG(base, 0x080)

/* OCR Register */
#define MMC_BOOT_OCR_17_19                (1 << 7)
#define MMC_BOOT_OCR_27_36                (0x1FF << 15)
#define MMC_BOOT_OCR_SEC_MODE             (2 << 29)
#define MMC_BOOT_OCR_BUSY                 (1 << 31)

/* Card Status bits (R1 register) */
#define MMC_BOOT_R1_AKE_SEQ_ERROR         (1 << 3)
#define MMC_BOOT_R1_APP_CMD               (1 << 5)
#define MMC_BOOT_R1_RDY_FOR_DATA          (1 << 6)
#define MMC_BOOT_R1_CURR_STATE_IDLE       (0 << 9)
#define MMC_BOOT_R1_CURR_STATE_RDY        (1 << 9)
#define MMC_BOOT_R1_CURR_STATE_IDENT      (2 << 9)
#define MMC_BOOT_R1_CURR_STATE_STBY       (3 << 9)
#define MMC_BOOT_R1_CURR_STATE_TRAN       (4 << 9)
#define MMC_BOOT_R1_CURR_STATE_DATA       (5 << 9)
#define MMC_BOOT_R1_CURR_STATE_RCV        (6 << 9)
#define MMC_BOOT_R1_CURR_STATE_PRG        (7 << 9)
#define MMC_BOOT_R1_CURR_STATE_DIS        (8 << 9)
#define MMC_BOOT_R1_ERASE_RESET           (1 << 13)
#define MMC_BOOT_R1_CARD_ECC_DISABLED     (1 << 14)
#define MMC_BOOT_R1_WP_ERASE_SKIP         (1 << 15)
#define MMC_BOOT_R1_ERROR                 (1 << 19)
#define MMC_BOOT_R1_CC_ERROR              (1 << 20)
#define MMC_BOOT_R1_CARD_ECC_FAILED       (1 << 21)
#define MMC_BOOT_R1_ILLEGAL_CMD           (1 << 22)
#define MMC_BOOT_R1_COM_CRC_ERR           (1 << 23)
#define MMC_BOOT_R1_LOCK_UNLOCK_FAIL      (1 << 24)
#define MMC_BOOT_R1_CARD_IS_LOCKED        (1 << 25)
#define MMC_BOOT_R1_WP_VIOLATION          (1 << 26)
#define MMC_BOOT_R1_ERASE_PARAM           (1 << 27)
#define MMC_BOOT_R1_ERASE_SEQ_ERR         (1 << 28)
#define MMC_BOOT_R1_BLOCK_LEN_ERR         (1 << 29)
#define MMC_BOOT_R1_ADDR_ERR              (1 << 30)
#define MMC_BOOT_R1_OUT_OF_RANGE          (1 << 31)

/* Macro for Success*/
#define MMC_BOOT_E_SUCCESS                0

typedef struct {
	MmcCtrlr mmc;
	unsigned instance;
	uint8_t *mci_base;
	uint32_t mmc_cont_version;
	int clk_mode;
} QcomMmcHost;

/* We have 16 32-bits FIFO registers */
#define MMC_BOOT_MCI_FIFO_DEPTH       16
#define MMC_BOOT_MCI_HFIFO_COUNT      (MMC_BOOT_MCI_FIFO_DEPTH / 2)
#define MMC_BOOT_MCI_FIFO_SIZE        (MMC_BOOT_MCI_FIFO_DEPTH * 4)

/* MMC clock speeds. */
#define MMC_CLK_400KHZ                400000
#define MMC_CLK_144KHZ                144000
#define MMC_CLK_20MHZ                 20000000
#define MMC_CLK_25MHZ                 25000000
#define MMC_CLK_48MHZ                 48000000
#define MMC_CLK_50MHZ                 49152000
#define MMC_CLK_96MHZ                 96000000
#define MMC_CLK_200MHZ                200000000

#define MMC_CLK_ENABLE      1
#define MMC_CLK_DISABLE     0

#define MMC_IDENTIFY_MODE 1
#define MMC_DATA_TRANSFER_MODE 2

#define MMC_READ_ERROR		(MMC_BOOT_MCI_STAT_DATA_CRC_FAIL|\
				MMC_BOOT_MCI_STAT_DATA_TIMEOUT|\
				MMC_BOOT_MCI_STAT_RX_OVRRUN)

#define MMC_WRITE_ERROR		(MMC_BOOT_MCI_STAT_DATA_CRC_FAIL|\
				MMC_BOOT_MCI_STAT_DATA_TIMEOUT|\
				MMC_BOOT_MCI_STAT_TX_UNDRUN)

void clock_config_mmc(MmcCtrlr *ctrlr, int mode);
void clock_disable_mmc(void);
void board_mmc_gpio_config(void);
QcomMmcHost *new_qcom_mmc_host(unsigned slot, uint32_t base, int bus_width);

#endif /* __DRIVERS_STORAGE_IPQ406X_MMC_H__ */
