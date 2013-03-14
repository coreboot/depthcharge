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
#include <usb/usb.h>
#include <vboot_nvstorage.h>
#include <vboot_api.h>

#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "config.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/input/input.h"
#include "drivers/net/net.h"
#include "drivers/timer/timer.h"
#include "drivers/power/power.h"
#include "net/uip.h"
#include "net/uip_arp.h"
#include "netboot/bootp.h"
#include "netboot/tftp.h"
#include "vboot/boot.h"
#include "vboot/util/flag.h"

static void enable_graphics(void)
{
	int oprom_loaded = flag_fetch(FLAG_OPROM);

	// Manipulating vboot's internal data and calling its internal
	// functions is NOT NICE and will give you athlete's foot and make
	// you unpopular at parties. Right now it's the only way to ensure
	// graphics are enabled, though, so it's a necessary evil.
	if (CONFIG_OPROM_MATTERS && !oprom_loaded) {
		printf("Enabling graphics.\n");

		VbNvContext context;

		VbExNvStorageRead(context.raw);
		VbNvSetup(&context);

		VbNvSet(&context, VBNV_OPROM_NEEDED, 1);

		VbNvTeardown(&context);
		VbExNvStorageWrite(context.raw);

		printf("Rebooting.\n");
		cold_reboot();
	}
}

static void print_ip_addr(const uip_ipaddr_t *ip)
{
	printf("%d.%d.%d.%d", uip_ipaddr1(ip), uip_ipaddr2(ip),
		uip_ipaddr3(ip), uip_ipaddr4(ip));
}

static void print_mac_addr(const uip_eth_addr *mac)
{
	for (int i = 0; i < ARRAY_SIZE(mac->addr); i++)
		printf("%s%02x", i ? ":" : "", mac->addr[i]);
}

static void * const payload = (void *)(uintptr_t)CONFIG_KERNEL_START;

int main(void)
{
	// Initialize some consoles.
	serial_init();
	cbmem_console_init();
	video_console_init();
	input_init();

	printf("\n\nStarting netboot...\n");

	timestamp_init();

	if (run_init_funcs())
		halt();

	// Make sure graphics are available if they aren't already.
	enable_graphics();

	dc_usb_initialize();

	srand(timer_raw_value());

	printf("Looking for network device... ");
	while (!net_get_device())
		usb_poll();
	printf("done.\n");

	printf("Waiting for link... ");
	int ready = 0;
	while (!ready) {
		if (net_ready(&ready))
			halt();
	}
	printf("done.\n");

	// Start up the network stack.
	uip_init();

	// Plug in the MAC address.
	const uip_eth_addr *mac_addr = net_get_mac();
	if (!mac_addr)
		halt();
	printf("MAC: ");
	print_mac_addr(mac_addr);
	printf("\n");
	uip_setethaddr(*mac_addr);

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

	uint32_t size;
	if (tftp_read(payload, &server_ip, bootfile, &size)) {
		printf("Tftp failed.\n");
		halt();
	}
	printf("The bootfile was %d bytes long.\n", size);

	char *cmd_line = CONFIG_NETBOOT_DEFAULT_COMMAND_LINE;

	boot(payload, cmd_line, NULL, NULL);

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}
