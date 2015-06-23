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
#include <vboot_api.h>

#include "config.h"
#include "drivers/video/coreboot_fb.h"
#include "drivers/video/display.h"
#include "fastboot/backend.h"
#include "fastboot/capabilities.h"
#include "fastboot/fastboot.h"
#include "fastboot/udc.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

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

static void print_string(const char *str)
{
	int str_len = strlen(str);
	while (str_len--) {
		if (*str == '\n')
			video_console_putchar('\r');
		video_console_putchar(*str++);
	}
}

/*
 * TODO(furquan): Get rid of this once the vboot flows and fastboot interactions
 * are finalized.
 */
static void fb_print_on_screen(const char *msg)
{
	unsigned int rows, cols;

	if (display_init())
		return;

	if (backlight_update(1))
		return;

	video_init();
	video_console_cursor_enable(0);

	video_get_rows_cols(&rows, &cols);
	video_console_set_cursor((cols - strlen(msg)) / 2, rows / 2);

	print_string(msg);
}

/********************* Stubs *************************/

int  __attribute__((weak)) board_should_enter_device_mode(void)
{
	return 0;
}

int __attribute__((weak)) board_user_confirmation(void)
{
	/* Default weak implementation. Returns 0 = no user confirmation. */
	return 0;
}

/************* Responses to Host **********************/
/*
 * Func: fb_send
 * Desc: Send message from output buffer after attaching the prefix. It also
 * resets the output buffer.
 *
 * This function expects that the output buffer has first 4 bytes unused so that
 * it can prepend the message type (INFO, DATA, FAIL, OKAY). Also, the output
 * buffer is expected to be in the following state:
 * head = 0, tail = pointing to end of data
 * Thus, length = tail - head (So, length = PREFIX_LEN + output str len).
 *
 * On return, buffer state is: head = 0, tail = PREFIX_LEN.
 */
static void fb_send(struct fb_buffer *output, const char *prefix)
{
	const size_t prefix_size = PREFIX_LEN;
	size_t response_length = fb_buffer_length(output);

	fb_buffer_rewind(output);

	char *resp = fb_buffer_tail(output);
	memcpy(resp, prefix, prefix_size);

	FB_LOG("Response: %.*s\n", (int)response_length, resp);

	usb_gadget_send(resp, response_length);

	fb_buffer_push(output, PREFIX_LEN);
}

/*
 * Func: fb_execute_send
 * Desc: Execute send command based on the cmd response type. It also performs
 * rewind operation on the output buffer.
 */
static void fb_execute_send(struct fb_cmd *cmd)
{
	static const char prefix[][PREFIX_LEN + 1] = {
		[FB_DATA] = "DATA",
		[FB_FAIL] = "FAIL",
		[FB_INFO] = "INFO",
		[FB_OKAY] = "OKAY",
	};

	if (cmd->type == FB_NONE)
		return;

	fb_send(&cmd->output, prefix[cmd->type]);
}

/************** Command Handlers ***********************/
/*
 * Default weak implementation returning -1. Board should implement this
 * function.
 */
int __attribute__((weak)) get_board_var(struct fb_cmd *cmd, fb_getvar_t var)
{
	return -1;
}

void fb_add_string(struct fb_buffer *buff, const char *str, const char *args)
{
	if (str == NULL)
		return;

	char *data = fb_buffer_tail(buff);
	size_t rem_len = fb_buffer_remaining(buff);
	int ret;

	ret = snprintf(data, rem_len, str, args);

	if (ret > rem_len)
		ret = rem_len;
	else if (ret < 0)
		ret = 0;

	fb_buffer_push(buff, ret);
}

void fb_add_number(struct fb_buffer *buff, const char *format,
		   unsigned long long num)
{
	if (format == NULL)
		return;

	char *data = fb_buffer_tail(buff);
	size_t rem_len = fb_buffer_remaining(buff);

	int ret = snprintf(data, rem_len, format, num);

	if (ret > rem_len)
		ret = rem_len;
	else if (ret < 0)
		ret = 0;

	fb_buffer_push(buff, ret);
}

static void fb_copy_buffer_bytes(struct fb_buffer *out, const char *src,
				size_t len)
{
	char *dst = fb_buffer_tail(out);
	memcpy(dst, src, len);
	fb_buffer_push(out, len);
}

static void fb_copy_buffer_data(struct fb_buffer *out, struct fb_buffer *in)
{
	const char *src = fb_buffer_head(in);
	size_t len = fb_buffer_length(in);
	fb_copy_buffer_bytes(out, src, len);
}

static char *fb_get_string(const char *src, size_t len)
{
	char *dst = xmalloc(len + 1);
	memcpy(dst, src, len);
	dst[len] = '\0';
	return dst;
}

static inline void fb_free_string(char *dst)
{
	free(dst);
}

static inline int fb_device_unlocked(void)
{
	return vboot_in_developer();
}

static int fb_read_var(struct fb_cmd *cmd, fb_getvar_t var)
{
	size_t input_len = fb_buffer_length(&cmd->input);

	struct fb_buffer *output = &cmd->output;

	switch (var) {
	case FB_VERSION:
		fb_add_string(output, FB_VERSION_STRING, NULL);
		break;

	case FB_DWNLD_SIZE:
		/* Max download size set to half of heap size */
		fb_add_number(output, "0x%x", CONFIG_FASTBOOT_HEAP_SIZE/2);
		break;

	case FB_PART_SIZE: {
		if (input_len == 0) {
			fb_add_string(output, "invalid partition", NULL);
			return -1;
		}

		char *data = fb_buffer_pull(&cmd->input, input_len);
		char *part_name = fb_get_string(data, input_len);
		unsigned long long part_size;

		part_size = backend_get_part_size_bytes(part_name);
		fb_free_string(part_name);

		if (part_size == 0) {
			fb_add_string(output, "invalid partition", NULL);
			return -1;
		}

		fb_add_number(output, "0x%llx", part_size);
		break;
	}
	case FB_PART_TYPE: {
		if (input_len == 0) {
			fb_add_string(output, "invalid partition", NULL);
			return -1;
		}

		char *data = fb_buffer_pull(&cmd->input, input_len);

		char *part_name = fb_get_string(data, input_len);
		const char *str = backend_get_part_fs_type(part_name);
		fb_free_string(part_name);

		if (str == NULL) {
			fb_add_string(output, "invalid partition", NULL);
			return -1;
		}

		fb_add_string(output, str, NULL);
		break;
	}
	case FB_BDEV_SIZE: {
		if (input_len == 0) {
			fb_add_string(output, "invalid bdev", NULL);
			return -1;
		}

		char *data = fb_buffer_pull(&cmd->input, input_len);
		char *bdev_name = fb_get_string(data, input_len);
		unsigned long long bdev_size;

		bdev_size = backend_get_bdev_size_bytes(bdev_name);
		fb_free_string(bdev_name);

		if (bdev_size == 0) {
			fb_add_string(output, "invalid bdev", NULL);
			return -1;
		}

		fb_add_number(output, "%llu", bdev_size);
		break;
	}
	case FB_SECURE: {
		if (fb_cap_func_allowed(FB_ID_FLASH) == FB_CAP_FUNC_NOT_ALLOWED)
			fb_add_string(output, "yes", NULL);
		else
			fb_add_string(output, "no", NULL);
		break;
	}
	case FB_UNLOCKED: {
		if (fb_device_unlocked())
			fb_add_string(output, "yes", NULL);
		else
			fb_add_string(output, "no", NULL);
		break;
	}
	default:
		goto board_read;
	}

	return 0;

board_read:
	return get_board_var(cmd, var);
}

struct name_string {
	const char *str;
	int expects_args;
	char delim;
};

#define NAME_NO_ARGS(s)	{.str = s, .expects_args = 0}
#define NAME_ARGS(s, d)	{.str = s, .expects_args = 1, .delim = d}

static size_t name_check_match(const char *str, size_t len,
			       const struct name_string *name)
{
	size_t str_len = strlen(name->str);

	/* If name len is greater than input, return 0. */
	if (str_len > len)
		return 0;

	/* If name str does not match input string, return 0. */
	if (memcmp(name->str, str, str_len))
		return 0;

	if (name->expects_args) {
		/* string should have space for delim */
		if (len == str_len)
			return 0;

		/* Check delim match */
		if (name->delim != str[str_len])
			return 0;
	} else {
		/* Name str len should match input len */
		if (str_len != len)
			return 0;
	}

	return str_len + name->expects_args;
}

static const struct {
	struct name_string name;
	fb_getvar_t var;
} getvar_table[] = {
	{ NAME_NO_ARGS("version"), FB_VERSION},
	{ NAME_NO_ARGS("version-bootloader"), FB_BOOTLOADER_VERSION},
	{ NAME_NO_ARGS("version-baseband"), FB_BASEBAND_VERSION},
	{ NAME_NO_ARGS("product"), FB_PRODUCT},
	{ NAME_NO_ARGS("serialno"), FB_SERIAL_NO},
	{ NAME_NO_ARGS("secure"), FB_SECURE},
	{ NAME_NO_ARGS("max-download-size"), FB_DWNLD_SIZE},
	{ NAME_ARGS("partition-type", ':'), FB_PART_TYPE},
	{ NAME_ARGS("partition-size", ':'), FB_PART_SIZE},
	{ NAME_NO_ARGS("unlocked"), FB_UNLOCKED},
	/*
	 * OEM specific :
	 * Spec says names starting with lowercase letter are reserved.
	 */
	{ NAME_ARGS("Bdev-size", ':'), FB_BDEV_SIZE},
};

/*
 * Func: fb_getvar_single
 * Desc: Returns value of a single variable requested by host.
 */
static fb_ret_type fb_getvar_single(struct fb_cmd *cmd)
{
	int i;
	size_t match_len = 0;
	const char *input = fb_buffer_head(&cmd->input);
	size_t len = fb_buffer_length(&cmd->input);
	size_t out_len = fb_buffer_length(&cmd->output);

	for (i = 0; i < ARRAY_SIZE(getvar_table); i++) {
		match_len = name_check_match(input, len, &getvar_table[i].name);
		if (match_len)
			break;
	}

	if (match_len == 0) {
		fb_add_string(&cmd->output, "unknown variable", NULL);
		cmd->type = FB_OKAY;
		return FB_SUCCESS;
	}

	fb_buffer_pull(&cmd->input, match_len);

	if (fb_read_var(cmd, getvar_table[i].var) == 0)
		return FB_SUCCESS;

	/*
	 * Since fb_read_var returned non-zero value it means that we were not
	 * able to read variable value. Thus, send message type to be sent back
	 * to host as FAIL.
	 */
	cmd->type = FB_OKAY;

	/*
	 * If no new information was added by board about the failure, put in
	 * nonexistent string.
	 */
	if (fb_buffer_length(&cmd->output) == out_len)
		fb_add_string(&cmd->output, "unknown variable", NULL);

	return FB_SUCCESS;
}

/*
 * Func: fb_getvar_all
 * Desc: Send to host values of all possible variables.
 */
static fb_ret_type fb_getvar_all(struct fb_cmd *host_cmd)
{
	int i;

	char pkt[MAX_COMMAND_LENGTH];
	char rsp[MAX_RESPONSE_LENGTH];

	struct fb_cmd cmd = {
		.input = {.data = pkt, .capacity = MAX_COMMAND_LENGTH},
		.output = {.data = rsp, .capacity = MAX_RESPONSE_LENGTH},
	};

	struct fb_buffer *cmd_in = &cmd.input;
	struct fb_buffer *cmd_out = &cmd.output;

	for (i = 0; i < ARRAY_SIZE(getvar_table); i++) {

		fb_getvar_t var = getvar_table[i].var;

		/* Leave space for prefix. */
		fb_buffer_push(cmd_out, PREFIX_LEN);

		const struct name_string *vname = &getvar_table[i].name;

		/*
		 * Add variable name to input string to mimic a real host
		 * command.
		 */
		fb_add_string(cmd_in, "%s", vname->str);
		if (vname->expects_args)
			fb_copy_buffer_bytes(cmd_in, &vname->delim, 1);

		/*
		 * For the getvar all command from host, we need to send back
		 * multiple INFO packets, with each INFO describing a variable
		 * and its value. We mimic a host command in the input buffer
		 * here.
		 *
		 * However, for variables that require additional argument, we
		 * need to ensure that it is filled in properly in the input
		 * buffer. Thus, we check for the variables that we know have an
		 * extra argument and handle them accordingly. Since, the
		 * argument can have different values, we run a loop to append
		 * different arguments one at a time to the input buffer. Then,
		 * we call getvar_single with our own host command.
		 * e.g.
		 * var:a
		 * var:b
		 * var:c
		 *
		 * The output buffer needs to contain the input string in
		 * addition to the variable value. Thus, we use
		 * fb_copy_buffer_data to copy data from input buffer to eoutput
		 * buffer and append ": " and the value of the variable. Then,
		 * the output buffer is sent back to the host as INFO message.
		 * e.g.
		 * INFOvar:a: x
		 * INFOvar:b: y
		 * INFOvar:c: z
		 */

		switch(var) {
		case FB_PART_TYPE:
		case FB_PART_SIZE:
		case FB_BDEV_SIZE: {
			int j;

			struct fb_cmd curr_cmd;

			struct fb_buffer *input = &curr_cmd.input;
			struct fb_buffer *output = &curr_cmd.output;

			int count = (var == FB_BDEV_SIZE)? fb_bdev_count:
				fb_part_count;

			for (j = 0; j < count; j++) {
				const char *pname;
				if (var == FB_BDEV_SIZE)
					pname = fb_bdev_list[j].name;
				else
					pname = fb_part_list[j].part_name;

				fb_buffer_clone(cmd_in, input);
				fb_buffer_clone(cmd_out, output);

				fb_add_string(input, pname, NULL);
				fb_copy_buffer_data(output, input);
				fb_add_string(output, ": ", NULL);

				curr_cmd.type = FB_INFO;
				fb_getvar_single(&curr_cmd);

				if (curr_cmd.type == FB_INFO)
					fb_execute_send(&curr_cmd);
			}
			break;
		}
		default:
			fb_copy_buffer_data(cmd_out, cmd_in);
			fb_add_string(cmd_out, ": ", NULL);
			cmd.type = FB_INFO;
			fb_getvar_single(&cmd);
			if (cmd.type == FB_INFO)
				fb_execute_send(&cmd);
		}

		/* Reset cmd buffers */
		fb_buffer_rewind(cmd_out);
		fb_buffer_rewind(cmd_in);
	}

	fb_add_string(&host_cmd->output, "Done!", NULL);
	host_cmd->type = FB_OKAY;

	return FB_SUCCESS;
}

static fb_ret_type fb_getvar(struct fb_cmd *cmd)
{
	const char *input = fb_buffer_head(&cmd->input);
	size_t len = fb_buffer_length(&cmd->input);

	if (len == 0) {
		fb_add_string(&cmd->output, "invalid length", NULL);
		cmd->type = FB_FAIL;
		return FB_SUCCESS;
	}

	const char *str_read_all = "all";

	if ((len == strlen(str_read_all)) &&
	    (strncmp(input, str_read_all, len) == 0))
		return fb_getvar_all(cmd);
	else {
		cmd->type = FB_OKAY;
		return fb_getvar_single(cmd);
	}
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

/*
 * Func: fb_recv_data
 * Desc: Download data from host and store it in image_addr
 *
 */
static int fb_recv_data(struct fb_cmd *cmd)
{
	size_t curr_len = 0;

	while (curr_len < image_size) {
		void *curr = (uint8_t *)image_addr + curr_len;

		size_t ret = usb_gadget_recv(curr, image_size - curr_len);

		if (ret == 0) {
			curr_len = 0;
			cmd->type = FB_NONE;
			return curr_len;
		}

		curr_len += ret;
	}

	cmd->type = FB_OKAY;
	return curr_len;
}

/*
 * Func: fb_download
 * Desc: Allocate space for downloading image and receive image from host.
 */
static fb_ret_type fb_download(struct fb_cmd *cmd)
{
	const char *input = fb_buffer_head(&cmd->input);
	size_t len = fb_buffer_length(&cmd->input);
	struct fb_buffer *output = &cmd->output;

	cmd->type = FB_FAIL;

	/* Length should be 8 bytes */
	if (len != 8) {
		fb_add_string(output, "invalid length", NULL);
		return FB_SUCCESS;
	}

	char *num = fb_get_string(input, len);

	/* num of bytes are passed in hex(0x) format */
	unsigned long bytes = strtoul(num, NULL, 16);

	fb_free_string(num);

	alloc_image_space(bytes);

	if ((image_addr == NULL) || (image_size == 0)) {
		fb_add_string(output, "not sufficient memory", NULL);
		return FB_SUCCESS;
	}

	cmd->type = FB_DATA;
	fb_add_number(output, "%08lx", bytes);
	fb_execute_send(cmd);

	if (fb_recv_data(cmd) == 0) {
		FB_LOG("Freeing memory.. failed to download data\n");
		free_image_space();
	}

	return FB_SUCCESS;
}

/* TODO(furquan): Do we need this? */
static fb_ret_type fb_verify(struct fb_cmd *cmd)
{
	fb_add_string(&cmd->output, "unsupported command", NULL);
	cmd->type = FB_FAIL;

	return FB_SUCCESS;
}

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

static fb_ret_type fb_erase(struct fb_cmd *cmd)
{
	/* Check if there is an override. If yes, do not erase partition. */
	if (fb_check_gbb_override()) {
		FB_LOG("skipping erase operation\n");
		cmd->type = FB_INFO;
		fb_add_string(&cmd->output, "skipping erase", NULL);
		fb_execute_send(cmd);
		cmd->type = FB_OKAY;
		return FB_SUCCESS;
	}

	backend_ret_t ret;
	struct fb_buffer *input = &cmd->input;
	size_t len = fb_buffer_length(input);
	char *data = fb_buffer_pull(input, len);

	FB_LOG("erasing flash\n");
	cmd->type = FB_INFO;
	fb_add_string(&cmd->output, "erasing flash", NULL);
	fb_execute_send(cmd);
	fb_print_on_screen("Erasing flash....\n");

	cmd->type = FB_OKAY;

	char *partition = fb_get_string(data, len);
	ret = backend_erase_partition(partition);
	fb_free_string(partition);

	if (ret != BE_SUCCESS) {
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output, backend_error_string[ret], NULL);
	}

	free(partition);

	return FB_SUCCESS;
}

static fb_ret_type fb_flash(struct fb_cmd *cmd)
{
	backend_ret_t ret;

	if (image_addr == NULL) {
		fb_add_string(&cmd->output, "no image downloaded", NULL);
		cmd->type = FB_FAIL;
		return FB_SUCCESS;
	}

	FB_LOG("writing flash\n");
	cmd->type = FB_INFO;
	fb_add_string(&cmd->output, "writing flash", NULL);
	fb_execute_send(cmd);
	fb_print_on_screen("Writing flash....\n");

	struct fb_buffer *input = &cmd->input;
	size_t len = fb_buffer_length(input);
	char *data = fb_buffer_pull(input, len);

	cmd->type = FB_OKAY;

	char *partition = fb_get_string(data, len);

	ret = board_write_partition(partition, image_addr, image_size);

	if (ret == BE_NOT_HANDLED)
		ret = backend_write_partition(partition, image_addr,
					      image_size);

	fb_free_string(partition);

	if (ret != BE_SUCCESS) {
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output, backend_error_string[ret], NULL);
	}

	free(partition);

	return FB_SUCCESS;
}

/*
 * fb_boot allows user to send a signed recovery image from host directly to
 * device memory and boot from it. It calls vboot function
 * VbVerifyMemoryBootImage to check if the given image is okay to boot from
 * memory in current mode.
 */
static fb_ret_type fb_boot(struct fb_cmd *cmd)
{
	VbSelectAndLoadKernelParams kparams;

	cmd->type = FB_FAIL;

	if (image_addr == NULL) {
		fb_add_string(&cmd->output, "no image downloaded", NULL);
		return FB_SUCCESS;
	}

	size_t kernel_size;
	void *kernel = bootimg_get_kernel_ptr(image_addr, image_size);

	if (kernel == NULL) {
		fb_add_string(&cmd->output, "bootimg format not recognized",
			      NULL);
		return FB_SUCCESS;
	}

	kernel_size = (uintptr_t)kernel - (uintptr_t)image_addr;

	if (kernel_size >= image_size) {
		fb_add_string(&cmd->output, "bootimg kernel not found", NULL);
		return FB_SUCCESS;
	}

	kernel_size = image_size - kernel_size;

	if (VbVerifyMemoryBootImage(&cparams, &kparams, kernel, kernel_size) !=
	    VBERROR_SUCCESS) {
		fb_add_string(&cmd->output, "image verification failed", NULL);
		return FB_SUCCESS;
	}

	kparams.flags = KERNEL_IMAGE_BOOTIMG;

	cmd->type = FB_OKAY;
	fb_execute_send(cmd);

	vboot_boot_kernel(&kparams);

	/* We should never reach here, if boot successful. */
	return FB_SUCCESS;
}

static fb_ret_type fb_continue(struct fb_cmd *cmd)
{
	FB_LOG("Continue booting\n");
	cmd->type = FB_OKAY;
	return FB_NORMAL_BOOT;
}

static fb_ret_type fb_reboot(struct fb_cmd *cmd)
{
	FB_LOG("Rebooting device into normal mode\n");
	cmd->type = FB_OKAY;
	return FB_REBOOT;
}

static fb_ret_type fb_reboot_bootloader(struct fb_cmd *cmd)
{
	FB_LOG("Rebooting into bootloader\n");
	cmd->type = FB_OKAY;
	return FB_REBOOT_BOOTLOADER;
}

static fb_ret_type fb_powerdown(struct fb_cmd *cmd)
{
	FB_LOG("Powering off device\n");
	cmd->type = FB_OKAY;
	return FB_POWEROFF;
}

static int fb_user_confirmation()
{
	/* If GBB is set, we don't need to get user confirmation. */
	if (fb_check_gbb_override())
		return 1;

	return board_user_confirmation();
}

static fb_ret_type fb_lock(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	if (!fb_user_confirmation()) {
		FB_LOG("User cancelled\n");
		fb_add_string(&cmd->output, "User cancelled request", NULL);
		return FB_SUCCESS;
	}

	FB_LOG("Locking device\n");
	if (!fb_device_unlocked()) {
		fb_add_string(&cmd->output, "Device already locked", NULL);
		return FB_SUCCESS;
	}

	if (VbLockDevice() != VBERROR_SUCCESS) {
		fb_add_string(&cmd->output, "Lock device failed", NULL);
		return FB_SUCCESS;
	}

	cmd->type = FB_OKAY;
	return FB_REBOOT;
}

static fb_ret_type fb_unlock(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	if (!fb_user_confirmation()) {
		FB_LOG("User cancelled\n");
		fb_add_string(&cmd->output, "User cancelled request", NULL);
		return FB_SUCCESS;
	}

	FB_LOG("Unlocking device\n");
	if (fb_device_unlocked()) {
		fb_add_string(&cmd->output, "Device already unlocked", NULL);
		return FB_SUCCESS;
	}

	if (VbUnlockDevice() != VBERROR_SUCCESS) {
		fb_add_string(&cmd->output, "Unlock device failed", NULL);
		return FB_SUCCESS;
	}

	cmd->type = FB_OKAY;
	return FB_REBOOT;
}

/************** Command Function Table *****************/
struct fastboot_func {
	struct name_string name;
	fb_func_id_t id;
	fb_ret_type (*fn)(struct fb_cmd *cmd);
};

const struct fastboot_func fb_func_table[] = {
	{ NAME_ARGS("getvar", ':'), FB_ID_GETVAR, fb_getvar},
	{ NAME_ARGS("download", ':'), FB_ID_DOWNLOAD, fb_download},
	{ NAME_ARGS("verify", ':'), FB_ID_VERIFY, fb_verify},
	{ NAME_ARGS("flash", ':'), FB_ID_FLASH, fb_flash},
	{ NAME_ARGS("erase", ':'), FB_ID_ERASE, fb_erase},
	{ NAME_NO_ARGS("boot"), FB_ID_BOOT, fb_boot},
	{ NAME_NO_ARGS("continue"), FB_ID_CONTINUE, fb_continue},
	{ NAME_NO_ARGS("reboot"), FB_ID_REBOOT, fb_reboot},
	{ NAME_NO_ARGS("reboot-bootloader"), FB_ID_REBOOT_BOOTLOADER,
	  fb_reboot_bootloader},
	{ NAME_NO_ARGS("powerdown"), FB_ID_POWERDOWN, fb_powerdown},
	{ NAME_NO_ARGS("oem unlock"), FB_ID_UNLOCK, fb_unlock},
	{ NAME_NO_ARGS("oem lock"), FB_ID_LOCK, fb_lock},
};

/************** Protocol Handler ************************/

/*
 * Func: fastboot_proto_handler
 * Desc: Handles fastboot commands received from host, takes appropriate action
 * (if required) and sends back response packets to host
 * Input is an ascii string without a trailing 0 byte. Max len is 64 bytes.
 */
static fb_ret_type fastboot_proto_handler(struct fb_cmd *cmd)
{
	struct fb_buffer *in_buff = &cmd->input;
	struct fb_buffer *out_buff = &cmd->output;

	size_t len = fb_buffer_length(in_buff);

	/* Ignore zero-size packets */
	if (len == 0) {
		FB_LOG("Ignoring zero-size packets\n");
		cmd->type = FB_OKAY;
		return FB_SUCCESS;
	}

	int i;
	size_t match_len = 0;
	const char *input = fb_buffer_head(in_buff);

	for (i = 0; i < ARRAY_SIZE(fb_func_table); i++) {
		match_len = name_check_match(input, len,
					     &fb_func_table[i].name);
		if (match_len)
			break;
	}

	if (match_len == 0) {
		FB_LOG("Unknown command\n");
		fb_add_string(out_buff, "unknown command", NULL);
		cmd->type = FB_FAIL;
		return FB_SUCCESS;
	}

	/* Check if this function is allowed to be executed */
	if (fb_cap_func_allowed(fb_func_table[i].id) ==
	    FB_CAP_FUNC_NOT_ALLOWED) {
		FB_LOG("Unsupported command\n");
		fb_add_string(out_buff, "unsupported command", NULL);
		cmd->type = FB_FAIL;
		return FB_SUCCESS;
	}

	fb_buffer_pull(in_buff, match_len);

	return fb_func_table[i].fn(cmd);
}

static void print_input(struct fb_cmd *cmd)
{
#ifdef FASTBOOT_DEBUG
	const char *pkt = fb_buffer_head(&cmd->input);
	size_t len = fb_buffer_length(&cmd->input);
	printf("Received: %.*s\n", (int)len, pkt);
#endif
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
	fb_ret_type ret = FB_CONTINUE_RECOVERY;

	/* Initialize USB gadget driver */
	if (!usb_gadget_init()) {
		FB_LOG("Gadget not initialized\n");
		return ret;
	}

	/*
	 * Extra +1 is to store the '\0' character to ease string
	 * operations. Host never ends a string with '\0'.
	 */
	char pkt[MAX_COMMAND_LENGTH];
	char rsp[MAX_RESPONSE_LENGTH];

	struct fb_cmd cmd = {
		.input = {.data = pkt, .capacity = MAX_COMMAND_LENGTH},
		.output = {.data = rsp, .capacity = MAX_RESPONSE_LENGTH},
	};

	/* Leave space for prefix */
	fb_buffer_push(&cmd.output, PREFIX_LEN);

	FB_LOG("********** Entered fastboot mode *****************\n");

	/*
	 * Keep looping until we get boot, reboot or poweroff command from host.
	 */
	do {
		size_t len;

		fb_print_on_screen("Waiting for fastboot command....\n");
		/* Receive a packet from the host */
		len = usb_gadget_recv(pkt, MAX_COMMAND_LENGTH);

		if (len == 0) {
			/*
			 * No packet received from host. We should reach here
			 * either because packet_recv timed out or the usb
			 * connection with host was disconnected. Continue
			 * waiting for fastboot command.
			 *
			 * Update ret to FB_SUCCESS so that we don't break out
			 * of the loop.
			 */
			ret = FB_SUCCESS;
			continue;
		}

		fb_buffer_push(&cmd.input, len);

		print_input(&cmd);

		fb_print_on_screen("Processing fastboot command....\n");

		/* Process the packet as per fastboot protocol */
		ret = fastboot_proto_handler(&cmd);

		fb_execute_send(&cmd);

		fb_buffer_rewind(&cmd.input);
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
