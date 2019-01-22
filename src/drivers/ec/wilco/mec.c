// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Microchip EC interface for Wilco Embedded Controller.
 */

#include <libpayload.h>

#include "drivers/ec/wilco/mec.h"

typedef enum MecAccessMode {
	/* 8-bit access */
	ACCESS_TYPE_BYTE,
	/* 16-bit access */
	ACCESS_TYPE_WORD,
	/* 32-bit access */
	ACCESS_TYPE_LONG,
	/*
	 * 32-bit access, read or write of MEC_EMI_EC_DATA_B3 causes the
	 * EC data register to be incremented.
	 */
	ACCESS_TYPE_LONG_AUTO_INCREMENT
} MecAccessMode;

/* EMI registers are relative to base */
#define MEC_EMI_HOST_TO_EC(base)	((base) + 0)
#define MEC_EMI_EC_TO_HOST(base)	((base) + 1)
#define MEC_EMI_EC_ADDRESS_B0(base)	((base) + 2)
#define MEC_EMI_EC_ADDRESS_B1(base)	((base) + 3)
#define MEC_EMI_EC_DATA_B0(base)	((base) + 4)
#define MEC_EMI_EC_DATA_B1(base)	((base) + 5)
#define MEC_EMI_EC_DATA_B2(base)	((base) + 6)
#define MEC_EMI_EC_DATA_B3(base)	((base) + 7)

static void mec_emi_write_address(uint16_t base, uint16_t offset,
				  MecAccessMode access_mode)
{
	outb((offset & 0xfc) | access_mode, MEC_EMI_EC_ADDRESS_B0(base));
	outb((offset >> 8) & 0x7f, MEC_EMI_EC_ADDRESS_B1(base));
}

uint8_t mec_io_bytes(MecIoType type, uint16_t base, uint16_t offset,
		     uint8_t *buf, size_t size)
{
	MecAccessMode access_mode, new_access_mode;
	int i = 0;
	uint16_t io_addr;
	uint8_t checksum = 0;

	if (base == 0 || size == 0)
		return 0;

	/*
	 * Long access cannot be used on misaligned data since reading B0 loads
	 * the data register and writing B3 flushes it.
	 */
	if ((offset & 0x3) || (size < 4))
		access_mode = ACCESS_TYPE_BYTE;
	else
		access_mode = ACCESS_TYPE_LONG_AUTO_INCREMENT;

	/* Initialize I/O at desired address */
	mec_emi_write_address(base, offset, access_mode);

	/* Skip bytes in case of misaligned offset */
	io_addr = MEC_EMI_EC_DATA_B0(base) + (offset & 0x3);
	while (i < size) {
		while (io_addr <= MEC_EMI_EC_DATA_B3(base)) {
			if (type == MEC_IO_WRITE)
				outb(buf[i], io_addr++);
			else
				buf[i] = inb(io_addr++);
			checksum += buf[i++];
			offset++;

			/* Extra bounds check in case of misaligned size */
			if (i == size)
				return checksum;
		}

		/*
		 * Use long auto-increment access except for misaligned write,
		 * since writing B3 triggers the flush.
		 */
		if ((size - i) < 4 && type == MEC_IO_WRITE)
			new_access_mode = ACCESS_TYPE_BYTE;
		else
			new_access_mode = ACCESS_TYPE_LONG_AUTO_INCREMENT;

		if (new_access_mode != access_mode ||
		    access_mode != ACCESS_TYPE_LONG_AUTO_INCREMENT) {
			access_mode = new_access_mode;
			mec_emi_write_address(base, offset, access_mode);
		}

		/* Access [B0, B3] on each loop pass */
		io_addr = MEC_EMI_EC_DATA_B0(base);
	}

	return checksum;
}
