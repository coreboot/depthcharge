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

#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "config.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/input/input.h"
#include "drivers/timer/timer.h"
#include "net/uip.h"
#include "netboot/bootp.h"
#include "netboot/tftp.h"

static void print_ip_addr(const uip_ipaddr_t *ip)
{
	printf("%d.%d.%d.%d", uip_ipaddr1(ip), uip_ipaddr2(ip),
		uip_ipaddr3(ip), uip_ipaddr4(ip));
}

static void * const payload = (void *)(uintptr_t)CONFIG_KERNEL_START;

int main(void)
{
	// Initialize some consoles.
	serial_init();
	cbmem_console_init();
	input_init();

	printf("\n\nStarting netboot...\n");

	timestamp_init();

	if (run_init_funcs())
		halt();

	dc_usb_initialize();

	srand(get_raw_timer_value());

	// Start up the network stack.
	uip_init();

	// Find out who we are and what we should boot.
	uip_ipaddr_t my_ip, server_ip;
	const char *bootfile;
	if (bootp(&server_ip, &bootfile)) {
		printf("Bootp failed.\n");
		halt();
	}
	printf("My ip is ");
	uip_gethostaddr(&my_ip);
	print_ip_addr(&my_ip);
	printf("\nThe server ip is ");
	print_ip_addr(&server_ip);
	printf("\nThe boot file is %s\n", bootfile);

	if (tftp_read(payload, &server_ip, bootfile)) {
		printf("Tftp failed.\n");
		halt();
	}

	/* Jump into payload. */

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}
