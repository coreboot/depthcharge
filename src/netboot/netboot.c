/*
 * Copyright 2013 Google LLC
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
 */

#include <libpayload.h>
#include <lp_vboot.h>
#include <stdbool.h>
#include <vb2_api.h>

#include "base/init_funcs.h"
#include "base/late_init_funcs.h"
#include "base/timestamp.h"
#include "drivers/input/input.h"
#include "drivers/net/net.h"
#include "drivers/power/power.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/video/display.h"
#include "net/uip.h"
#include "net/uip_arp.h"
#include "netboot/dhcp.h"
#include "netboot/netboot.h"
#include "netboot/params.h"
#include "netboot/tftp.h"
#include "vboot/boot.h"
#include "vboot/crossystem/crossystem.h"
#include "vboot/nvdata.h"

static void enable_graphics(void)
{
	struct vb2_context *ctx = vboot_get_context();
	display_init();
	backlight_update(true);

	if (!vb2api_need_reboot_for_display(ctx))
		return;

	printf("Enabling graphics.\n");
	nvdata_write(ctx);

	reboot();
}

static void * const payload = (void *)(uintptr_t)CONFIG_KERNEL_START;
static const uint32_t MaxPayloadSize = CONFIG_KERNEL_SIZE;

static char cmd_line[4096] = "lsm.module_locking=0 cros_netboot_ramfs "
			     "cros_factory_install cros_secure cros_netboot";

void netboot(uip_ipaddr_t *tftp_ip, char *bootfile, char *argsfile, char *args,
	     char *ramdiskfile)
{
	net_wait_for_link();

	// Start up the network stack.
	uip_init();

	// Find out who we are.
	uip_ipaddr_t next_ip, server_ip;
	const char *dhcp_bootfile;
	while (dhcp_request(&next_ip, &server_ip, &dhcp_bootfile))
		printf("Dhcp failed, retrying.\n");

	if (!tftp_ip) {
		tftp_ip = &next_ip;
		printf("TFTP server IP supplied by DHCP server: ");
	} else {
		printf("TFTP server IP predefined by user: ");
	}
	printf("%d.%d.%d.%d\n", uip_ipaddr_to_quad(tftp_ip));

	// Download the bootfile.
	uint32_t size;
	if (!bootfile) {
		bootfile = (char *)dhcp_bootfile;
		printf("Bootfile supplied by DHCP server: %s\n", bootfile);
	} else {
		printf("Bootfile predefined by user: %s\n", bootfile);
	}

	if (tftp_read(payload, tftp_ip, bootfile, &size, MaxPayloadSize)) {
		printf("Tftp failed.\n");
		if (dhcp_release(server_ip))
			printf("Dhcp release failed.\n");
		halt();
	}
	printf("The bootfile was %d bytes long.\n", size);

	void *ramdisk = NULL;
	uint32_t ramdisk_size = 0;

	if (ramdiskfile) {
		ramdisk = (void *)(uintptr_t)ALIGN_UP(
			CONFIG_KERNEL_START + size, 4096);
		size = (uintptr_t)ramdisk - CONFIG_KERNEL_START;

		if (size >= MaxPayloadSize) {
			printf("No space left for ramdisk\n");
			ramdisk = NULL;
		} else if (tftp_read(ramdisk, tftp_ip, ramdiskfile,
				     &ramdisk_size, MaxPayloadSize - size)) {
			printf("TFTP failed for ramdisk.\n");
			ramdisk = NULL;
			ramdisk_size = 0;
		}

	}

	// Try to download command line file via TFTP if argsfile is specified
	if (argsfile && !(tftp_read(cmd_line, tftp_ip, argsfile, &size,
			sizeof(cmd_line) - 1))) {
		while (cmd_line[size - 1] <= ' ')  // strip trailing whitespace
			if (!--size) break;	   // and control chars (\n, \r)
		cmd_line[size] = '\0';
		while (size--)			   // replace inline control
			if (cmd_line[size] < ' ')  // chars with spaces
				cmd_line[size] = ' ';
		printf("Command line loaded dynamically from TFTP file: %s\n",
				argsfile);
	// If that fails or file wasn't specified fall back to args parameter
	} else if (args) {
		if (args != cmd_line)
			strncpy(cmd_line, args, sizeof(cmd_line) - 1);
		printf("Command line predefined by user.\n");
	}

	// We're done on the network, so release our IP.
	if (dhcp_release(server_ip)) {
		printf("Dhcp release failed.\n");
		halt();
	}

	// Add tftp server IP into command line.
	static const char def_tftp_cmdline[] = " tftpserverip=xxx.xxx.xxx.xxx";
	int cmd_line_size = strlen(cmd_line);
	if (cmd_line_size + sizeof(def_tftp_cmdline) - 1 >= sizeof(cmd_line)) {
		printf("Out of space adding TFTP server IP to the command line.\n");
		return;
	}
	sprintf(&cmd_line[cmd_line_size], " tftpserverip=%d.%d.%d.%d",
		uip_ipaddr1(tftp_ip), uip_ipaddr2(tftp_ip),
		uip_ipaddr3(tftp_ip), uip_ipaddr4(tftp_ip));
	printf("The command line is: %s\n", cmd_line);

	// Boot.
	struct boot_info bi = {
		.kernel = payload,
		.cmd_line = cmd_line,
		.ramdisk_addr = ramdisk,
		.ramdisk_size = ramdisk_size,
	};

	if (crossystem_setup(FIRMWARE_TYPE_NETBOOT))
		return;

	// Video console scrolling is SLOOOW so don't dump all the FIT load spam
	if (!CONFIG_HEADLESS)
		console_remove_output_driver(video_console_putchar);

	boot(&bi);
}

int main(void) __attribute__((weak, alias("netboot_entry")));
int netboot_entry(void)
{
	// Initialize some consoles.
	serial_console_init();
	cbmem_console_init();
	if (!CONFIG(HEADLESS))
		video_console_init();
	input_init();

	struct cb_mainboard *mainboard =
		phys_to_virt(lib_sysinfo.cb_mainboard);
	const char *mb_part_string = cb_mb_part_string(mainboard);
	printf("\n\nStarting netboot on %s...\n", mb_part_string);

	// Set up time keeping.
	timestamp_init();

	// Run any generic initialization functions that are compiled in.
	if (run_init_funcs())
		halt();

	// Force init USB regardless of vboot mode.
	dc_usb_initialize();

	// Make sure graphics are available if they aren't already.
	enable_graphics();

	// Run any late initialization functions before netboot takes control.
	if (run_late_init_funcs())
		halt();

	srand(timer_raw_value());

	uip_ipaddr_t *tftp_ip;
	char *bootfile, *argsfile;
	if (netboot_params_read(&tftp_ip, cmd_line, sizeof(cmd_line),
				&bootfile, &argsfile))
		printf("ERROR: Failed to read netboot parameters from flash\n");

	netboot(tftp_ip, bootfile, argsfile, cmd_line, NULL);

	// We should never get here.
	printf("Got to the end!\n");
	halt();
	return 0;
}
