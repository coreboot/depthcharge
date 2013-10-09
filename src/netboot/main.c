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
#include "drivers/flash/flash.h"
#include "drivers/input/input.h"
#include "drivers/net/net.h"
#include "drivers/timer/timer.h"
#include "drivers/power/power.h"
#include "image/fmap.h"
#include "net/uip.h"
#include "net/uip_arp.h"
#include "netboot/dhcp.h"
#include "netboot/params.h"
#include "netboot/tftp.h"
#include "vboot/boot.h"
#include "vboot/util/flag.h"

static void enable_graphics(void)
{
	if (!CONFIG_OPROM_MATTERS)
		return;

	int oprom_loaded = flag_fetch(FLAG_OPROM);

	// Manipulating vboot's internal data and calling its internal
	// functions is NOT NICE and will give you athlete's foot and make
	// you unpopular at parties. Right now it's the only way to ensure
	// graphics are enabled, though, so it's a necessary evil.
	if (!oprom_loaded) {
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
static const uint32_t MaxPayloadSize = CONFIG_KERNEL_SIZE;

static char cmd_line[4096] = CONFIG_NETBOOT_DEFAULT_COMMAND_LINE;
static const int cmd_line_def_size =
	sizeof(CONFIG_NETBOOT_DEFAULT_COMMAND_LINE);

int main(void)
{
	NetbootParam *param;

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

	// Find out who we are.
	uip_ipaddr_t my_ip, next_ip, server_ip;
	const char *dhcp_bootfile;
	while (dhcp_request(&next_ip, &server_ip, &dhcp_bootfile))
		printf("Dhcp failed, retrying.\n");

	printf("My ip is ");
	uip_gethostaddr(&my_ip);
	print_ip_addr(&my_ip);
	printf("\nThe DHCP server ip is ");
	print_ip_addr(&server_ip);
	printf("\n");

	// Retrieve settings from the shared data area.
	FmapArea shared_data;
	if (fmap_find_area("SHARED_DATA", &shared_data)) {
		printf("Couldn't find the shared data area.\n");
		halt();
	}
	void *data = flash_read(shared_data.offset, shared_data.size);
	netboot_params_init(data, shared_data.size);

	uip_ipaddr_t *tftp_ip = NULL;
	const char *bootfile = NULL;
	// Use some settings either from DHCP or from shared_data.
        if (!CONFIG_NETBOOT_DYNAMIC_PARAMS) {
		param = netboot_params_val(NetbootParamIdTftpServerIp);
		if (param->size >= sizeof(uip_ipaddr_t))
			tftp_ip = (uip_ipaddr_t *)param->data;
	} else {
		tftp_ip = &next_ip;
		bootfile = dhcp_bootfile;
	}

	// Use bootfile setting from shared data if it's set.
	param = netboot_params_val(NetbootParamIdBootfile);
	if (param->data && param->size > 0 &&
			strnlen((const char *)param->data,
				param->size) < param->size) {
		bootfile = (const char *)param->data;
	}

	if (!tftp_ip) {
		printf("No TFTP server IP.\n");
		if (dhcp_release(server_ip))
			printf("Dhcp release failed.\n");
		halt();
	} else {
		printf("The TFTP server ip is ");
		print_ip_addr(tftp_ip);
		printf("\n");
	}
	if (!bootfile) {
		printf("No bootfile.\n");
		if (dhcp_release(server_ip))
			printf("Dhcp release failed.\n");
		halt();
	} else {
		printf("The boot file is %s\n", bootfile);
	}

	// Download the bootfile.
	uint32_t size;
	if (tftp_read(payload, tftp_ip, bootfile, &size, MaxPayloadSize)) {
		printf("Tftp failed.\n");
		if (dhcp_release(server_ip))
			printf("Dhcp release failed.\n");
		halt();
	}

	// We're done on the network, so release our IP.
	if (dhcp_release(server_ip)) {
		printf("Dhcp release failed.\n");
		halt();
	}

	printf("The bootfile was %d bytes long.\n", size);

	// Add user supplied parameters to the command line.
	param = netboot_params_val(NetbootParamIdKernelArgs);
	cmd_line[cmd_line_def_size - 1] = '\0';
	if (param->data && param->size > 0) {
		int args_len = strnlen((const char *)param->data, param->size);

		if (args_len < param->size &&
				args_len + cmd_line_def_size <
				sizeof(cmd_line)) {
			cmd_line[cmd_line_def_size - 1] = ' ';
			strncpy(&cmd_line[cmd_line_def_size], param->data,
				sizeof(cmd_line) - cmd_line_def_size);
		}
	}

	// Add tftp server IP into command line.
	static const char def_tftp_cmdline[] = " tftpserverip=xxx.xxx.xxx.xxx";
	const int tftp_cmdline_def_size = sizeof(def_tftp_cmdline) - 1;
	int cmd_line_size = strlen(cmd_line);
	if (cmd_line_size + tftp_cmdline_def_size > sizeof(cmd_line)) {
		printf("Out of space adding TFTP server IP to the command line.\n");
		return 1;
	}
	sprintf(&cmd_line[cmd_line_size], " tftpserverip=%d.%d.%d.%d",
		uip_ipaddr1(tftp_ip), uip_ipaddr2(tftp_ip),
		uip_ipaddr3(tftp_ip), uip_ipaddr4(tftp_ip));
	printf("The command line is %s.\n", cmd_line);

	// Boot.
	boot(payload, cmd_line, NULL, NULL);

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}
