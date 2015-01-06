/*
 * Copyright 2015 Google Inc.
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

#include "fastboot/backend.h"
#include "fastboot/fastboot.h"
#include "fastboot/udc.h"

#define FASTBOOT_DEBUG

#ifdef FASTBOOT_DEBUG
#define FB_LOG(args...)		printf(args);
#else
#define FB_LOG(args...)
#endif

#define MAX_COMMAND_LENGTH	64
#define MAX_RESPONSE_LENGTH	64

/* Pointer to memory location where image is downloaded for further action */
static void *image_addr;
static size_t image_size;

const char *backend_error_string[] = {
	[BE_PART_NOT_FOUND] = "partition not found",
	[BE_BDEV_NOT_FOUND] = "block device not found",
	[BE_IMAGE_SIZE_MULTIPLE_ERR] = "image not multiple of block size",
	[BE_IMAGE_OVERFLOW_ERR] = "image greater than partition size",
	[BE_WRITE_ERR] = "image write failed",
	[BE_SPARSE_HDR_ERR] = "sparse header error",
	[BE_CHUNK_HDR_ERR] = "sparse chunk header error",
	[BE_GPT_ERR] = "GPT error",
};

/********************* Stubs *************************/

int  __attribute__((weak)) board_should_enter_device_mode(void)
{
	return 0;
}

/************* Responses to Host **********************/
static void fb_send(const char *msg, const char *add)
{
	char resp[MAX_RESPONSE_LENGTH] = "";

	strncat(resp, msg, MAX_RESPONSE_LENGTH - 1);
	strncat(resp, add, MAX_RESPONSE_LENGTH - 1);

	usb_gadget_send(resp, strlen(resp));
}

static void fb_send_info(const char *str)
{
	FB_LOG("Response:INFO%s\n", str);
	fb_send("INFO", str);
}

static void fb_send_fail(const char *str)
{
	FB_LOG("Response:FAIL%s\n", str);
	fb_send("FAIL", str);
}

static void fb_send_okay(const char *str)
{
	FB_LOG("Response:OKAY%s\n", str);
	fb_send("OKAY", str);
}

static void fb_send_data(const char *str)
{
	FB_LOG("Response:DATA%s\n", str);
	fb_send("DATA", str);
}

/************** Command Handlers ***********************/
/*
 * Default weak implementation returning -1. Board should implement this
 * function.
 */
int __attribute__((weak)) get_board_var(fb_getvar_t var, const char *input,
					size_t input_len, char *str,
					size_t str_len)
{
	return -1;
}

static void fb_read_var(fb_getvar_t var, const char *input, size_t input_len)
{
	char str[MAX_COMMAND_LENGTH];

	switch (var) {
	case FB_VERSION:
		fb_send_okay("0.3");
		break;
	default:
		goto board_read;
	}

	return;

board_read:
	if (get_board_var(var, input, input_len, str, MAX_COMMAND_LENGTH) == -1)
		fb_send_fail("nonexistent");
	else
		fb_send_okay(str);
}

static fb_ret_type fb_getvar(const char *input, size_t len)
{

	if (len == 0) {
		fb_send_fail("nonexistent");
		return FB_SUCCESS;
	}

	static const struct {
		char name[MAX_COMMAND_LENGTH];
		fb_getvar_t var;
	}getvar_table[] = {
		{"version", FB_VERSION},
		{"version-bootloader", FB_BOOTLOADER_VERSION},
		{"version-baseband", FB_BASEBAND_VERSION},
		{"product", FB_PRODUCT},
		{"serialno", FB_SERIAL_NO},
		{"secure", FB_SECURE},
		{"max-download-size", FB_DWNLD_SIZE},
	};

	int i;
	int match_index = -1, match_len = 0;

	for (i = 0; i < ARRAY_SIZE(getvar_table); i++) {
		size_t str_len = strlen(getvar_table[i].name);

		if (str_len > len)
			continue;

		if (memcmp(getvar_table[i].name, input, str_len))
			continue;

		if (str_len > match_len) {
			match_index = i;
			match_len = str_len;
		}
	}

	if (match_index == -1) {
		char *name = xmalloc(len+1);
		memcpy(name, input, len);
		name[len] = '\0';
		FB_LOG("%s\n", name);
		free(name);
		fb_send_fail("nonexistent");
	} else {
		FB_LOG("%s\n",getvar_table[match_index].name);
		fb_read_var(getvar_table[match_index].var, input + match_len,
			    len - match_len);
	}

	return FB_SUCCESS;

}

static void free_image_space(void)
{
	if (image_addr) {
		free(image_addr);
		image_addr = NULL;
		image_size = 0;
	}
}

static void alloc_image_space(size_t bytes)
{
	free_image_space();
	image_addr = xmalloc(bytes);
	if (image_addr)
		image_size = bytes;
}

static fb_ret_type fb_download(const char *input, size_t len)
{
	/* Length should be 8 bytes */
	if (len != 8) {
		fb_send_fail("invalid length argument");
		return FB_SUCCESS;
	}

	char str[9];
	memcpy(str, input, len);
	str[len] = '\0';

	/* num of bytes are passed in hex(0x) format */
	size_t bytes = strtoul(str, NULL, 16);
	FB_LOG("%zx\n", bytes);

	alloc_image_space(bytes);

	if (image_addr == NULL) {
		fb_send_fail("couldn't allocate memory");
		return FB_SUCCESS;
	}

	fb_send_data(input);

	return FB_RECV_DATA;
}

/* TODO(furquan): Do we need this? */
static fb_ret_type fb_verify(const char *input, size_t len)
{
	fb_send_fail("unsupported command");
	return FB_SUCCESS;
}

static char *get_name(const char *input, size_t len)
{
	char *partition = xmalloc(len + 1);
	memcpy(partition, input, len);
	partition[len] = '\0';

	return partition;
}

static fb_ret_type fb_erase(const char *input, size_t len)
{
	backend_ret_t ret;
	char *partition = get_name(input, len);

	FB_LOG("erasing flash\n");
	fb_send_info("erasing flash");

	ret = backend_erase_partition(partition);

	if (ret != BE_SUCCESS)
		fb_send_fail(backend_error_string[ret]);
	else
		fb_send_okay("");

	free(partition);

	return FB_SUCCESS;
}

static fb_ret_type fb_flash(const char *input, size_t len)
{
	backend_ret_t ret;
	char *partition = get_name(input, len);

	if (image_addr == NULL) {
		fb_send_fail("no image downloaded");
		return FB_SUCCESS;
	}

	FB_LOG("writing flash\n");
	fb_send_info("writing flash");
	/* TODO(furquan): Check if the image has a bootimg header */
	ret = backend_write_partition(partition, image_addr, image_size);
	if (ret != BE_SUCCESS) {
		fb_send_fail(backend_error_string[ret]);
		goto fail;
	}

	fb_send_okay("");

fail:
	free(partition);

	return FB_SUCCESS;
}

/* TODO(furquan): How do we boot from memory? */
static fb_ret_type fb_boot(const char *input, size_t len)
{
	fb_send_fail("unsupported command");
	return FB_SUCCESS;
}

static fb_ret_type fb_continue(const char *input, size_t len)
{
	fb_send_okay("");
	FB_LOG("Continue booting\n");
	return FB_NORMAL_BOOT;
}

static fb_ret_type fb_reboot(const char *input, size_t len)
{
	fb_send_okay("");
	FB_LOG("Rebooting device into normal mode\n");
	return FB_REBOOT;
}

static fb_ret_type fb_reboot_bootloader(const char *input, size_t len)
{
	fb_send_okay("");
	FB_LOG("Rebooting into bootloader\n");
	return FB_REBOOT_BOOTLOADER;
}

static fb_ret_type fb_powerdown(const char *input, size_t len)
{
	fb_send_okay("");
	FB_LOG("Powering off device\n");
	return FB_POWEROFF;
}

/************** Command Function Table *****************/
struct fastboot_func {
	char name[MAX_COMMAND_LENGTH];
	fb_ret_type (*fn)(const char *input, size_t len);
};

const struct fastboot_func fb_func_table[] = {
	{"getvar:", fb_getvar},
	{"download:", fb_download},
	{"verify:", fb_verify},
	{"flash:", fb_flash},
	{"erase:", fb_erase},
	{"boot", fb_boot},
	{"continue", fb_continue},
	{"reboot", fb_reboot},
	{"reboot-bootloader", fb_reboot_bootloader},
	{"powerdown", fb_powerdown},
};

/************** Protocol Handler ************************/

/*
 * Func: fastboot_proto_handler
 * Desc: Handles fastboot commands received from host, takes appropriate action
 * (if required) and sends back response packets to host
 * Input is an ascii string without a trailing 0 byte. Max len is 64 bytes.
 */
static fb_ret_type fastboot_proto_handler(const char *input, size_t len)
{
	/* Ignore zero-size packets */
	if (len == 0) {
		FB_LOG("Ignoring zero-size packets\n");
		return FB_SUCCESS;
	}

	int i;
	int match_index = -1, match_len = 0;

	for (i = 0; i < ARRAY_SIZE(fb_func_table); i++) {

		size_t str_len = strlen(fb_func_table[i].name);

		if (str_len > len)
			continue;

		if (memcmp(fb_func_table[i].name, input, str_len))
			continue;

		if (str_len > match_len) {
			match_index = i;
			match_len = str_len;
		}
	}

	if (match_index == -1) {
		FB_LOG("Unknown command\n");
		fb_send_fail("unknown command");
		return FB_SUCCESS;
	}

	FB_LOG("Received command: %s", fb_func_table[match_index].name);

	return fb_func_table[match_index].fn(input + match_len,
					     len - match_len);
}

/*
 * Func: fastboot_recv_data
 * Desc: Download data from host and store it in image_addr
 *
 */
static fb_ret_type fastboot_recv_data(void)
{
	size_t curr_len = 0;

	if ((image_addr == NULL) || (image_size == 0)) {
		FB_LOG("No space to recv data\n");
		return FB_SUCCESS;
	}

	while (curr_len < image_size) {
		void *curr = (uint8_t *)image_addr + curr_len;
		curr_len += usb_gadget_recv(curr, image_size - curr_len);
	}

	fb_send_okay("");

	return FB_SUCCESS;
}

/*
 * Func: device_mode_enter
 * Desc: This function handles the entry into the device mode. It is responsible
 * for:
 * 1) Driving the USB gadget.
 * 2) Calling the protocol handler.
 * 3) Exiting fastboot and booting / rebooting / powering off the device.
 */
fb_ret_type device_mode_enter(void)
{
	fb_ret_type ret = FB_SUCCESS;

	FB_LOG("********** Entered fastboot mode *****************\n");

	/* Initialize USB gadget driver */
	if (!usb_gadget_init()) {
		FB_LOG("Gadget not initialized\n");
		return FB_SUCCESS;
	}

	/*
	 * Keep looping until we get boot, reboot or poweroff command from host.
	 */
	do {
		char pkt[MAX_COMMAND_LENGTH];
		size_t len;

		/* Receive a packet from the host */
		len = usb_gadget_recv(pkt, MAX_COMMAND_LENGTH);

		/* Process the packet as per fastboot protocol */
		ret = fastboot_proto_handler(pkt, len);

		if (ret == FB_RECV_DATA) {
			FB_LOG("Going to recv data\n");
			ret = fastboot_recv_data();
			FB_LOG("Recv data done\n");
		}

	} while (ret == FB_SUCCESS);

	/* Done with usb gadget driver - Stop it */
	usb_gadget_stop();

	/* Ret should be either poweroff, reboot or reboot bootloader */
	if ((ret != FB_POWEROFF) && (ret != FB_REBOOT) &&
	    (ret != FB_REBOOT_BOOTLOADER))
		ret = FB_REBOOT;

	FB_LOG("********** Exit fastboot mode *****************\n");
	return ret;
}
