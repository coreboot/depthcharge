/*
 *  drivers/mtd/nandids.c
 *
 *  Copyright (C) 2002 Thomas Gleixner (tglx@linutronix.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "drivers/storage/mtd/nand/nand.h"
/*
*	Chip ID list
*
*	Name. ID code, chipsize in MegaByte
*
*/
const struct nand_flash_dev nand_flash_ids[] = {
	/*512 Megabit */
	{0xA2, 64},
	{0xA0, 64},
	{0xF2, 64},
	{0xD0, 64},
	{0xB2, 64},
	{0xB0, 64},
	{0xC2, 64},
	{0xC0, 64},

	/* 1 Gigabit */
	{0xA1, 128},
	{0xF1, 128},
	{0xD1, 128},
	{0xB1, 128},
	{0xC1, 128},
	{0xAD, 128},

	/* 2 Gigabit */
	{0xAA, 256},
	{0xDA, 256},
	{0xBA, 256},
	{0xCA, 256},

	/* 4 Gigabit */
	{0xAC, 512},
	{0xDC, 512},
	{0xBC, 512},
	{0xCC, 512},

	/* 8 Gigabit */
	{0xA3, 1024},
	{0xD3, 1024},
	{0xB3, 1024},
	{0xC3, 1024},

	/* 16 Gigabit */
	{0xA5, 2048},
	{0xD5, 2048},
	{0xB5, 2048},
	{0xC5, 2048},

	/* 32 Gigabit */
	{0xA7, 4096},
	{0xD7, 4096},
	{0xB7, 4096},
	{0xC7, 4096},

	/* 64 Gigabit */
	{0xAE, 8192},
	{0xDE, 8192},
	{0xBE, 8192},
	{0xCE, 8192},

	/* 128 Gigabit */
	{0x1A, 16384},
	{0x3A, 16384},
	{0x2A, 16384},
	{0x4A, 16384},

	/* 256 Gigabit */
	{0x1C, 32768},
	{0x3C, 32768},
	{0x2C, 32768},
	{0x4C, 32768},

	/* 512 Gigabit */
	{0x1E, 65536},
	{0x3E, 65536},
	{0x2E, 65536},
	{0x4E, 65536},

	{0, 0}
};
