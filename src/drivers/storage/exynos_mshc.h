/*
 * (C) Copyright 2012 SAMSUNG Electronics
 * Abhilash Kesavan <a.kesavan@samsung.com>
 *
 * Copyright 2013 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __DRIVERS_STORAGE_EXYNOS_MSHC_H__
#define __DRIVERS_STORAGE_EXYNOS_MSHC_H__

#include <stdint.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/mmc.h"

/*  Control Register  Register */
#define CTRL_RESET	(0x1 << 0)
#define FIFO_RESET	(0x1 << 1)
#define DMA_RESET	(0x1 << 2)
#define DMA_ENABLE	(0x1 << 5)
#define SEND_AS_CCSD	(0x1 << 10)
#define ENABLE_IDMAC    (0x1 << 25)

/*  Power Enable Register */
#define POWER_ENABLE	(0x1 << 0)

/*  Clock Enable Register */
#define CLK_ENABLE	(0x1 << 0)
#define CLK_DISABLE	(0x0 << 0)

/* Timeout Register */
#define TMOUT_MAX	0xffffffff

/*  Card Type Register */
#define PORT0_CARD_WIDTH1	0
#define PORT0_CARD_WIDTH4	(0x1 << 0)
#define PORT0_CARD_WIDTH8	(0x1 << 16)

/*  Interrupt Mask Register */
#define INTMSK_ALL	0xffffffff
#define INTMSK_RE	(0x1 << 1)
#define INTMSK_CDONE	(0x1 << 2)
#define INTMSK_DTO	(0x1 << 3)
#define INTMSK_DCRC	(0x1 << 7)
#define INTMSK_RTO	(0x1 << 8)
#define INTMSK_DRTO	(0x1 << 9)
#define INTMSK_HTO	(0x1 << 10)
#define INTMSK_FRUN	(0x1 << 11)
#define INTMSK_HLE	(0x1 << 12)
#define INTMSK_SBE	(0x1 << 13)
#define INTMSK_ACD	(0x1 << 14)
#define INTMSK_EBE	(0x1 << 15)

/* Command Register */
#define CMD_RESP_EXP_BIT	(0x1 << 6)
#define CMD_RESP_LENGTH_BIT	(0x1 << 7)
#define CMD_CHECK_CRC_BIT	(0x1 << 8)
#define CMD_DATA_EXP_BIT	(0x1 << 9)
#define CMD_RW_BIT		(0x1 << 10)
#define CMD_SENT_AUTO_STOP_BIT	(0x1 << 12)
#define CMD_WAIT_PRV_DAT_BIT	(0x1 << 13)
#define CMD_SEND_CLK_ONLY	(0x1 << 21)
#define CMD_USE_HOLD_REG	(0x1 << 29)
#define CMD_STRT_BIT		(0x1 << 31)
#define CMD_ONLY_CLK		(CMD_STRT_BIT | CMD_SEND_CLK_ONLY | \
				CMD_WAIT_PRV_DAT_BIT)

/*  Raw Interrupt Register */
#define DATA_ERR	(INTMSK_EBE | INTMSK_SBE | INTMSK_HLE |	\
			INTMSK_FRUN | INTMSK_EBE | INTMSK_DCRC)
#define DATA_TOUT	(INTMSK_HTO | INTMSK_DRTO)

/*  Status Register */
#define DATA_BUSY	(0x1 << 9)

/*  FIFO Threshold Watermark Register */
#define TX_WMARK	(0xFFF << 0)
#define RX_WMARK	(0xFFF << 16)
#define MSIZE_MASK	(0x7 << 28)

/* DW DMA Mutiple Transaction Size */
#define MSIZE_8		(2 << 28)

/*  Bus Mode Register */
#define BMOD_IDMAC_RESET	(0x1 << 0)
#define BMOD_IDMAC_FB		(0x1 << 1)
#define BMOD_IDMAC_ENABLE	(0x1 << 7)

/* IDMAC bits */
#define MSHCI_IDMAC_OWN		(0x1 << 31)
#define MSHCI_IDMAC_CH		(0x1 << 4)
#define MSHCI_IDMAC_FS		(0x1 << 3)
#define MSHCI_IDMAC_LD		(0x1 << 2)

#define MAX_MSHCI_CLOCK	52000000 /* Max limit for mshc clock is 52MHz */
#define MIN_MSHCI_CLOCK	400000 /* Lower limit for mshc clock is 400KHz */

#define S5P_MSHC_COMMAND_TIMEOUT 10000
#define S5P_MSHC_TIMEOUT_MS	100

typedef struct S5pMshci {
	uint32_t ctrl;
	uint32_t pwren;
	uint32_t clkdiv;
	uint32_t clksrc;
	uint32_t clkena;
	uint32_t tmout;
	uint32_t ctype;
	uint32_t blksiz;
	uint32_t bytcnt;
	uint32_t intmask;
	uint32_t cmdarg;
	uint32_t cmd;
	uint32_t resp0;
	uint32_t resp1;
	uint32_t resp2;
	uint32_t resp3;
	uint32_t mintsts;
	uint32_t rintsts;
	uint32_t status;
	uint32_t fifoth;
	uint32_t cdetect;
	uint32_t wrtprt;
	uint32_t gpio;
	uint32_t tcbcnt;
	uint32_t tbbcnt;
	uint32_t debnce;
	uint32_t usrid;
	uint32_t verid;
	uint32_t hcon;
	uint32_t uhs_reg;
	uint32_t rst_n;
	uint8_t	reserved1[4];
	uint32_t bmod;
	uint32_t pldmnd;
	uint32_t dbaddr;
	uint32_t idsts;
	uint32_t idinten;
	uint32_t dscaddr;
	uint32_t bufaddr;
	uint32_t clksel;
	uint8_t	reserved2[460];
	uint32_t cardthrctl;
} S5pMshci;

// Descriptor list for DMA
typedef struct MshciIdmac {
	uint32_t des0;
	uint32_t des1;
	uint32_t des2;
	uint32_t des3;
} MshciIdmac;

typedef struct MshciHost {
	MmcCtrlr mmc;

	S5pMshci *regs;  // Mapped address
	uint32_t clock;  // Current clock in MHz
	uint32_t src_hz; // The frequency of the source clock
	uint32_t clksel_val;

	int initialized;
} MshciHost;

MshciHost *new_mshci_host(uintptr_t ioaddr, uint32_t src_hz, int bus_width,
			  int removable, uint32_t clksel_val);

#endif /* __DRIVERS_STORAGE_EXYNOS_MSHC_H__ */
