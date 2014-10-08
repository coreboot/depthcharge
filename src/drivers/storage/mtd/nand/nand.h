/*
 *  linux/include/linux/mtd/nand.h
 *
 *  Copyright © 2000-2010 David Woodhouse <dwmw2@infradead.org>
 *                        Steven J. Hill <sjhill@realitydiluted.com>
 *		          Thomas Gleixner <tglx@linutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Info:
 *	Contains standard defines and IDs for NAND flash devices
 *
 * Changelog:
 *	See git changelog.
 */
#ifndef __DRIVERS_STORAGE_MTD_NAND_NAND_H__
#define __DRIVERS_STORAGE_MTD_NAND_NAND_H__

#include "drivers/storage/mtd/mtd.h"

/*
 * Standard NAND flash commands
 */
#define NAND_CMD_READ0		0
#define NAND_CMD_READ1		1
#define NAND_CMD_RNDOUT		5
#define NAND_CMD_PAGEPROG	0x10
#define NAND_CMD_READOOB	0x50
#define NAND_CMD_ERASE1		0x60
#define NAND_CMD_STATUS		0x70
#define NAND_CMD_STATUS_MULTI	0x71
#define NAND_CMD_SEQIN		0x80
#define NAND_CMD_RNDIN		0x85
#define NAND_CMD_READID		0x90
#define NAND_CMD_ERASE2		0xd0
#define NAND_CMD_PARAM		0xec
#define NAND_CMD_RESET		0xff

#define NAND_CMD_LOCK		0x2a
#define NAND_CMD_LOCK_TIGHT	0x2c
#define NAND_CMD_UNLOCK1	0x23
#define NAND_CMD_UNLOCK2	0x24
#define NAND_CMD_LOCK_STATUS	0x7a

/* Extended commands for large page devices */
#define NAND_CMD_READSTART	0x30
#define NAND_CMD_RNDOUTSTART	0xE0
#define NAND_CMD_CACHEDPROG	0x15

#define NAND_CMD_NONE		-1

/* Status bits */
#define NAND_STATUS_FAIL	0x01
#define NAND_STATUS_FAIL_N1	0x02
#define NAND_STATUS_TRUE_READY	0x20
#define NAND_STATUS_READY	0x40
#define NAND_STATUS_WP		0x80

struct nand_onfi_params {
	/* rev info and features block */
	/* 'O' 'N' 'F' 'I'  */
	u8 sig[4];
	u16 revision;
	u16 features;
	u16 opt_cmd;
	u8 reserved[22];

	/* manufacturer information block */
	char manufacturer[12];
	char model[20];
	u8 jedec_id;
	u16 date_code;
	u8 reserved2[13];

	/* memory organization block */
	u32 byte_per_page;
	u16 spare_bytes_per_page;
	u32 data_bytes_per_ppage;
	u16 spare_bytes_per_ppage;
	u32 pages_per_block;
	u32 blocks_per_lun;
	u8 lun_count;
	u8 addr_cycles;
	u8 bits_per_cell;
	u16 bb_per_lun;
	u16 block_endurance;
	u8 guaranteed_good_blocks;
	u16 guaranteed_block_endurance;
	u8 programs_per_page;
	u8 ppage_attr;
	u8 ecc_bits;
	u8 interleaved_bits;
	u8 interleaved_ops;
	u8 reserved3[13];

	/* electrical parameter block */
	u8 io_pin_capacitance_max;
	u16 async_timing_mode;
	u16 program_cache_timing_mode;
	u16 t_prog;
	u16 t_bers;
	u16 t_r;
	u16 t_ccs;
	u16 src_sync_timing_mode;
	u16 src_ssync_features;
	u16 clk_pin_capacitance_typ;
	u16 io_pin_capacitance_typ;
	u16 input_pin_capacitance_typ;
	u8 input_pin_capacitance_max;
	u8 driver_strenght_support;
	u16 t_int_r;
	u16 t_ald;
	u8 reserved4[7];

	/* vendor */
	u8 reserved5[90];

	u16 crc;
} __attribute__((packed));

#define ONFI_CRC_BASE	0x4F4E

/**
 * struct nand_flash_dev - NAND Flash Device ID Structure
 * Only newer NAND with the pagesize and erasesize implicit
 * in the ID are allowed.
 * @id:		least significant nibble of device ID code
 * @chipsize:	Total chipsize in Mega Bytes
 */
struct nand_flash_dev {
	int id;
	int chipsize;
};

extern const struct nand_flash_dev nand_flash_ids[];

#endif /* __DRIVERS_STORAGE_MTD_NAND_NAND_H__ */
