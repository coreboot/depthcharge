/*
 * Copyright 2013 Google Inc.
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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <stdint.h>

typedef struct S5pUart
{
	uint32_t ulcon;		// line control
	uint32_t ucon;		// control
	uint32_t ufcon;		// FIFO control
	uint32_t umcon;		// modem control
	uint32_t utrstat;	// Tx/Rx status
	uint32_t uerstat;	// Rx error status
	uint32_t ufstat;	// FIFO status
	uint32_t umstat;	// modem status
	uint32_t utxh;		// transmit buffer
	uint32_t urxh;		// receive buffer
	uint32_t ubrdiv;	// baud rate divisor
	uint32_t ufracval;	// divisor fractional value
	uint32_t uintp;		// interrupt pending
	uint32_t uints;		// interrupt source
	uint32_t uintm;		// interrupt mask
} S5pUart;

static S5pUart *s5p_uart;

static void s5p_putchar(unsigned int c)
{
	const uint32_t TxFifoFullBit = (0x1 << 24);

	while (readl(&s5p_uart->ufstat) & TxFifoFullBit)
	{;}

	writeb(c, &s5p_uart->utxh);
	if (c == '\n')
		s5p_putchar('\r');
}

static int s5p_havekey(void)
{
	const uint32_t DataReadyMask = (0xf << 0) | (0x1 << 8);

	return (readl(&s5p_uart->ufstat) & DataReadyMask) != 0;
}

static int s5p_getchar(void)
{
	while (!s5p_havekey())
	{;}

	return readb(&s5p_uart->urxh);
}

static struct console_output_driver s5p_serial_output =
{
	.putchar = &s5p_putchar
};

static struct console_input_driver s5p_serial_input =
{
	.havekey = &s5p_havekey,
	.getchar = &s5p_getchar
};

void serial_init(void)
{
	if (!lib_sysinfo.serial || !lib_sysinfo.serial->baseaddr)
		return;

	s5p_uart = (S5pUart *)lib_sysinfo.serial->baseaddr;
	if (s5p_uart) {
		console_add_output_driver(&s5p_serial_output);
		console_add_input_driver(&s5p_serial_input);
	}
}
