/*
 * Copyright 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libpayload.h>
#include <stdint.h>

typedef struct TegraUart {
	union {
		uint32_t thr; // Transmit holding register.
		uint32_t rbr; // Receive buffer register.
		uint32_t dll; // Divisor latch lsb.
	};
	union {
		uint32_t ier; // Interrupt enable register.
		uint32_t dlm; // Divisor latch msb.
	};
	union {
		uint32_t iir; // Interrupt identification register.
		uint32_t fcr; // FIFO control register.
	};
	uint32_t lcr; // Line control register.
	uint32_t mcr; // Modem control register.
	uint32_t lsr; // Line status register.
	uint32_t msr; // Modem status register.
} __attribute__ ((packed)) TegraUart;

enum {
	TEGRA_UART_LSR_DR = 0x1 << 0, // Data ready.
	TEGRA_UART_LSR_OE = 0x1 << 1, // Overrun.
	TEGRA_UART_LSR_PE = 0x1 << 2, // Parity error.
	TEGRA_UART_LSR_FE = 0x1 << 3, // Framing error.
	TEGRA_UART_LSR_BI = 0x1 << 4, // Break.
	TEGRA_UART_LSR_THRE = 0x1 << 5, // Xmit holding register empty.
	TEGRA_UART_LSR_TEMT = 0x1 << 6, // Xmitter empty.
	TEGRA_UART_LSR_ERR = 0x1 << 7 // Error.
};

static TegraUart * tegra_uart;

static void tegra_putchar(unsigned int c)
{
	while (!(readb(&tegra_uart->lsr) & TEGRA_UART_LSR_THRE));
	writeb(c, &tegra_uart->thr);
}

static int tegra_havekey(void)
{
	uint8_t lsr = readb(&tegra_uart->lsr);
	return (lsr & TEGRA_UART_LSR_DR) == TEGRA_UART_LSR_DR;
}

static int tegra_getchar(void)
{
	while (!tegra_havekey())
	{;}

	return readb(&tegra_uart->rbr);
}

static struct console_output_driver tegra_serial_output =
{
	.putchar = &tegra_putchar
};

static struct console_input_driver tegra_serial_input =
{
	.havekey = &tegra_havekey,
	.getchar = &tegra_getchar
};

void serial_init(void)
{
	if (!lib_sysinfo.serial || !lib_sysinfo.serial->baseaddr)
		return;

	tegra_uart = (TegraUart *)lib_sysinfo.serial->baseaddr;
}

void serial_console_init(void)
{
	serial_init();

	if (tegra_uart) {
		console_add_output_driver(&tegra_serial_output);
		console_add_input_driver(&tegra_serial_input);
	}
}
