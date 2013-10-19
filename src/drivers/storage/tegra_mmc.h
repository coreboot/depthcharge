/*
 * (C) Copyright 2009 SAMSUNG Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Portions Copyright (C) 2011-2012 NVIDIA Corporation
 *
 * Copyright 2013 Google Inc.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __DRIVERS_STORAGE_TEGRA_MMC_H_
#define __DRIVERS_STORAGE_TEGRA_MMC_H_

#include <arch/io.h>

#include "drivers/gpio/gpio.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/mmc.h"

typedef struct {
	uint32_t	sysad;		// _SYSTEM_ADDRESS_0
	uint16_t	blksize;	// _BLOCK_SIZE_BLOCK_COUNT_0 15:00
	uint16_t	blkcnt;		// _BLOCK_SIZE_BLOCK_COUNT_0 31:16
	uint32_t	argument;	// _ARGUMENT_0
	uint16_t	trnmod;		// _CMD_XFER_MODE_0 15:00 xfer mode
	uint16_t	cmdreg;		// _CMD_XFER_MODE_0 31:16 cmd reg
	uint32_t	rspreg0;	// _RESPONSE_R0_R1_0 CMD RESP 31:00
	uint32_t	rspreg1;	// _RESPONSE_R2_R3_0 CMD RESP 63:32
	uint32_t	rspreg2;	// _RESPONSE_R4_R5_0 CMD RESP 95:64
	uint32_t	rspreg3;	// _RESPONSE_R6_R7_0 CMD RESP 127:96
	uint32_t	bdata;		// _BUFFER_DATA_PORT_0
	uint32_t	prnsts;		// _PRESENT_STATE_0
	uint8_t		hostctl;	// _POWER_CONTROL_HOST_0 7:00
	uint8_t		pwrcon;		// _POWER_CONTROL_HOST_0 15:8
	uint8_t		blkgap;		// _POWER_CONTROL_HOST_9 23:16
	uint8_t		wakcon;		// _POWER_CONTROL_HOST_0 31:24
	uint16_t	clkcon;		// _CLOCK_CONTROL_0 15:00
	uint8_t		timeoutcon;	// _TIMEOUT_CTRL 23:16
	uint8_t		swrst;		// _SW_RESET_ 31:24
	uint32_t	norintsts;	// _INTERRUPT_STATUS_0
	uint32_t	norintstsen;	// _INTERRUPT_STATUS_ENABLE_0
	uint32_t	norintsigen;	// _INTERRUPT_SIGNAL_ENABLE_0
	uint16_t	acmd12errsts;	// _AUTO_CMD12_ERR_STATUS_0 15:00
	uint8_t		res1[2];	// _RESERVED 31:16
	uint32_t	capareg;	// _CAPABILITIES_0
	uint8_t		res2[4];	// RESERVED, offset 44h-47h
	uint32_t	maxcurr;	// _MAXIMUM_CURRENT_0
	uint8_t		res3[4];	// RESERVED, offset 4Ch-4Fh
	uint16_t	setacmd12err;	// offset 50h
	uint16_t	setinterr;	// offset 52h
	uint8_t		admaerr;	// offset 54h
	uint8_t		res4[3];	// RESERVED, offset 55h-57h
	unsigned long	admaaddr;	// offset 58h-5Fh
	uint8_t		res5[0xa0];	// RESERVED, offset 60h-FBh
	uint16_t	slotintstatus;	// offset FCh
	uint16_t	hcver;		// HOST Version
	uint32_t	venclkctl;	// _VENDOR_CLOCK_CNTRL_0,    100h
	uint32_t	venspictl;	// _VENDOR_SPI_CNTRL_0,      104h
	uint32_t	venspiintsts;	// _VENDOR_SPI_INT_STATUS_0, 108h
	uint32_t	venceatactl;	// _VENDOR_CEATA_CNTRL_0,    10Ch
	uint32_t	venbootctl;	// _VENDOR_BOOT_CNTRL_0,     110h
	uint32_t	venbootacktout;	// _VENDOR_BOOT_ACK_TIMEOUT, 114h
	uint32_t	venbootdattout;	// _VENDOR_BOOT_DAT_TIMEOUT, 118h
	uint32_t	vendebouncecnt;	// _VENDOR_DEBOUNCE_COUNT_0, 11Ch
	uint32_t	venmiscctl;	// _VENDOR_MISC_CNTRL_0,     120h
	uint32_t	res6[47];	// 0x124 ~ 0x1DC
	uint32_t	sdmemcmppadctl;	// _SDMEMCOMPPADCTRL_0,      1E0h
	uint32_t	autocalcfg;	// _AUTO_CAL_CONFIG_0,       1E4h
	uint32_t	autocalintval;	// _AUTO_CAL_INTERVAL_0,     1E8h
	uint32_t	autocalsts;	// _AUTO_CAL_STATUS_0,       1ECh
} TegraMmcReg;

enum {
	TEGRA_MMC_PWRCTL_SD_BUS_POWER =				(1 << 0),
	TEGRA_MMC_PWRCTL_SD_BUS_VOLTAGE_V1_8 =			(5 << 1),
	TEGRA_MMC_PWRCTL_SD_BUS_VOLTAGE_V3_0 =			(6 << 1),
	TEGRA_MMC_PWRCTL_SD_BUS_VOLTAGE_V3_3 =			(7 << 1),

	TEGRA_MMC_HOSTCTL_DMASEL_MASK =				(3 << 3),
	TEGRA_MMC_HOSTCTL_DMASEL_SDMA =				(0 << 3),
	TEGRA_MMC_HOSTCTL_DMASEL_ADMA2_32BIT =			(2 << 3),
	TEGRA_MMC_HOSTCTL_DMASEL_ADMA2_64BIT =			(3 << 3),

	TEGRA_MMC_TRNMOD_DMA_ENABLE =				(1 << 0),
	TEGRA_MMC_TRNMOD_BLOCK_COUNT_ENABLE =			(1 << 1),
	TEGRA_MMC_TRNMOD_DATA_XFER_DIR_SEL_WRITE =		(0 << 4),
	TEGRA_MMC_TRNMOD_DATA_XFER_DIR_SEL_READ =		(1 << 4),
	TEGRA_MMC_TRNMOD_MULTI_BLOCK_SELECT =			(1 << 5),

	TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_MASK =		(3 << 0),
	TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_NO_RESPONSE =		(0 << 0),
	TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_136 =		(1 << 0),
	TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_48 =		(2 << 0),
	TEGRA_MMC_CMDREG_RESP_TYPE_SELECT_LENGTH_48_BUSY =	(3 << 0),

	TEGRA_MMC_TRNMOD_CMD_CRC_CHECK =			(1 << 3),
	TEGRA_MMC_TRNMOD_CMD_INDEX_CHECK =			(1 << 4),
	TEGRA_MMC_TRNMOD_DATA_PRESENT_SELECT_DATA_TRANSFER =	(1 << 5),

	TEGRA_MMC_PRNSTS_CMD_INHIBIT_CMD =			(1 << 0),
	TEGRA_MMC_PRNSTS_CMD_INHIBIT_DAT =			(1 << 1),

	TEGRA_MMC_CLKCON_INTERNAL_CLOCK_ENABLE =		(1 << 0),
	TEGRA_MMC_CLKCON_INTERNAL_CLOCK_STABLE =		(1 << 1),
	TEGRA_MMC_CLKCON_SD_CLOCK_ENABLE =			(1 << 2),

	TEGRA_MMC_CLKCON_SDCLK_FREQ_SEL_SHIFT =			8,
	TEGRA_MMC_CLKCON_SDCLK_FREQ_SEL_MASK =			(0xff << 8),

	TEGRA_MMC_SWRST_SW_RESET_FOR_ALL =			(1 << 0),
	TEGRA_MMC_SWRST_SW_RESET_FOR_CMD_LINE =			(1 << 1),
	TEGRA_MMC_SWRST_SW_RESET_FOR_DAT_LINE =			(1 << 2),

	TEGRA_MMC_NORINTSTS_CMD_COMPLETE =			(1 << 0),
	TEGRA_MMC_NORINTSTS_XFER_COMPLETE =			(1 << 1),
	TEGRA_MMC_NORINTSTS_DMA_INTERRUPT =			(1 << 3),
	TEGRA_MMC_NORINTSTS_ERR_INTERRUPT =			(1 << 15),
	TEGRA_MMC_NORINTSTS_CMD_TIMEOUT =			(1 << 16),

	TEGRA_MMC_NORINTSTSEN_CMD_COMPLETE =			(1 << 0),
	TEGRA_MMC_NORINTSTSEN_XFER_COMPLETE =			(1 << 1),
	TEGRA_MMC_NORINTSTSEN_DMA_INTERRUPT =			(1 << 3),
	TEGRA_MMC_NORINTSTSEN_BUFFER_WRITE_READY =		(1 << 4),
	TEGRA_MMC_NORINTSTSEN_BUFFER_READ_READY =		(1 << 5),

	TEGRA_MMC_NORINTSIGEN_XFER_COMPLETE =			(1 << 1),

	// SDMMC1/3 settings from section 24.6 of T30 TRM
	MEMCOMP_PADCTRL_VREF =					7,
	AUTO_CAL_ENABLED =					(1 << 29),
	AUTO_CAL_PD_OFFSET =					(0x70 << 8),
	AUTO_CAL_PU_OFFSET =					(0x62 << 0),
};

typedef struct {
	MmcCtrlr mmc;

	TegraMmcReg *reg;
	uint32_t clock;		// Current clock (MHz)
	uint32_t src_hz;	// Source clock (hz)

	GpioOps *cd_gpio;	// Change Detect GPIO

	int initialized;
	int removable;
} TegraMmcHost;

TegraMmcHost *new_tegra_mmc_host(uintptr_t ioaddr, int bus_width,
				 int removable,	GpioOps *card_detect);

#endif // __DRIVERS_STORAGE_TEGRA_MMC_H_
