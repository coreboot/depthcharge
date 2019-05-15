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
 */

#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>

#include "base/cleanup_funcs.h"
#include "config.h"
#include "fastboot/backend.h"
#include "fastboot/capabilities.h"
#include "fastboot/fastboot.h"
#include "fastboot/udc.h"
#include "image/symbols.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"
#include "vboot/firmware_id.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/vbnv.h"

#define FASTBOOT_DEBUG

#ifdef FASTBOOT_DEBUG
#define FB_LOG(args...)		printf(args);
#else
#define FB_LOG(args...)
#endif

#define MAX_COMMAND_LENGTH	64
#define MAX_RESPONSE_LENGTH	64

/* Default fastboot function callback table. */
fb_callback_t __attribute__((weak)) fb_board_handler;

/* Size of currently loaded image (0 = no image loaded). */
static uint64_t image_size = 0;

static void fb_print_on_screen(fb_msg_t type, const char *msg)
{
	if (fb_board_handler.print_screen)
		fb_board_handler.print_screen(type, msg, strlen(msg));
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

void fb_add_number(struct fb_buffer *buff, const char *format, uint64_t num)
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

int fb_device_unlocked(void)
{
	return vboot_in_developer();
}

static inline uint64_t fb_get_max_download_size(void)
{
	return CONFIG_KERNEL_SIZE;
}

static inline void *fb_get_image_ptr(void)
{
	return &_kernel_start;
}

static int battery_soc_check(void)
{
	/*
	 * If board does not define this function, then by default it is
	 * assumed that there are no restrictions on flash/erase
	 * operation w.r.t. battery state-of-charge.
	 */
	if (fb_board_handler.battery_soc_ok == NULL) {
		FB_LOG("No handler defined for battery_soc_ok\n");
		return 1;
	}

	/*
	 * No check for battery state-of-charge is performed if GBB override is
	 * set.
	 */
	if (fb_check_gbb_override())
		return 1;

	return fb_board_handler.battery_soc_ok();
}

static const struct fw_type_to_index {
	const char *name;
	uint8_t index;
} fb_fw_type_to_index[] = {
	{"RO", VBSD_RO},
	{"RW_A", VBSD_RW_A},
	{"RW_B", VBSD_RW_B},
};

#if CONFIG_FASTBOOT_SLOTS
static const char *slot_get_suffix(int index)
{
	static char suffix[3];

	assert (strlen(CONFIG_FASTBOOT_SLOTS_STARTING_SUFFIX) == 2);
	strcpy(suffix, CONFIG_FASTBOOT_SLOTS_STARTING_SUFFIX);
	suffix[1] += index;

	return suffix;
}

static int slot_get_index(const char *suffix, size_t suffix_len)
{
	const char *starting_suffix = CONFIG_FASTBOOT_SLOTS_STARTING_SUFFIX;

	assert ((suffix_len == 2) &&
		(strlen(starting_suffix) == 2));

	return suffix[1] - starting_suffix[1];
}
#endif

static int fb_read_var(struct fb_cmd *cmd, fb_getvar_t var)
{
	size_t input_len = fb_buffer_length(&cmd->input);

	struct fb_buffer *output = &cmd->output;

	switch (var) {
	case FB_VERSION:
		fb_add_string(output, FB_VERSION_STRING, NULL);
		break;

	case FB_DWNLD_SIZE:
		fb_add_number(output, "0x%llx", fb_get_max_download_size());
		break;

	case FB_PART_SIZE: {
		if (input_len == 0) {
			fb_add_string(output, "invalid partition", NULL);
			return -1;
		}

		char *data = fb_buffer_pull(&cmd->input, input_len);
		char *part_name = fb_get_string(data, input_len);
		uint64_t part_size;

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
		uint64_t bdev_size;

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
	case  FB_OFF_MODE_CHARGE: {
		fb_add_number(output, "%lld",
			      !vbnv_read(VB2_NV_BOOT_ON_AC_DETECT));
		break;
	}
	case FB_BATT_VOLTAGE: {
		uint32_t val_mv;
		if ((fb_board_handler.read_batt_volt == NULL) ||
		    (fb_board_handler.read_batt_volt(&val_mv) != 0))
			fb_add_string(output, "Unknown", NULL);
		else
			fb_add_number(output, "%lld mV", val_mv);
		break;
	}
	case FB_BATT_SOC_OK: {
		/*
		 * This variable is supposed to return yes if device battery
		 * state-of-charge is acceptable for flashing.
		 */
		if (battery_soc_check())
			fb_add_string(output, "yes", NULL);
		else
			fb_add_string(output, "no", NULL);
		break;
	}
	case FB_GBB_FLAGS: {
		fb_add_number(output, "0x%llx", gbb_get_flags());
		break;
	}
	case FB_OEM_VERSION: {
		if (input_len == 0) {
			fb_add_string(output, "invalid arg", NULL);
			return -1;
		}

		char *data = fb_buffer_pull(&cmd->input, input_len);
		char *fw_type = fb_get_string(data, input_len);
		int i;

		for (i = 0; i < ARRAY_SIZE(fb_fw_type_to_index); i++) {
			if (strcmp(fb_fw_type_to_index[i].name, fw_type) == 0)
				break;
		}

		if (i == ARRAY_SIZE(fb_fw_type_to_index)) {
			fb_add_string(output, "invalid arg", NULL);
			return -1;
		}

		const char *version = get_fw_id(fb_fw_type_to_index[i].index);
		if (version == NULL)
			fb_add_string(output, "unknown", NULL);
		else
			fb_add_string(output, "%s", version);

		break;
	}
#if CONFIG_FASTBOOT_SLOTS
	case FB_HAS_SLOT: {
		if (input_len == 0) {
			fb_add_string(output, "invalid arg", NULL);
			return -1;
		}

		char *base = fb_buffer_pull(&cmd->input, input_len);
		int i;

		for (i = 0; i < fb_base_count; i++) {
			if (input_len != strlen(fb_base_list[i].base_name))
				continue;

			if (strncmp(base, fb_base_list[i].base_name,
				    input_len) == 0)
				break;
		}

		if (i == fb_base_count) {
			fb_add_string(output, "invalid arg", NULL);
			return -1;
		}

		if (fb_base_list[i].is_slotted == 1)
			fb_add_string(output, "yes", NULL);
		else
			fb_add_string(output, "no", NULL);

		break;
	}
	case FB_CURR_SLOT: {
		int slot = backend_get_curr_slot();

		if (slot < 0) {
			fb_add_string(output, "no valid curr slot", NULL);
			return -1;
		}

		assert(slot < CONFIG_FASTBOOT_SLOTS_COUNT);
		fb_add_string(output, slot_get_suffix(slot), NULL);
		break;
	}
	case FB_SLOT_SUFFIXES: {
		int i;

		fb_add_string(output, CONFIG_FASTBOOT_SLOTS_STARTING_SUFFIX,
			      NULL);

		for (i = 1; i < CONFIG_FASTBOOT_SLOTS_COUNT; i++)
			fb_add_string(output, ",%s", slot_get_suffix(i));
		break;
	}
	case FB_SLOT_SUCCESSFUL:
	case FB_SLOT_UNBOOTABLE:
	case FB_SLOT_RETRY_COUNT: {
		if (input_len != 2) {
			fb_add_string(output, "invalid arg", NULL);
			return -1;
		}

		char *data = fb_buffer_pull(&cmd->input, input_len);
		int index = slot_get_index(data, input_len);
		int ret = backend_get_slot_flags(var, index);

		if (ret < 0) {
			fb_add_string(output, "failed to get flags", NULL);
			return -1;
		}

		fb_add_number(output, "%lld", ret);
		break;
	}
#endif
	default:
		goto board_read;
	}

	return 0;

board_read:
	if (fb_board_handler.get_var)
		return fb_board_handler.get_var(cmd, var);
	else {
		FB_LOG("ERROR: get_var is not defined by board\n");
		return -1;
	}
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

/* Environment variables and routines to set the variables. */

/*
 * env_force_erase: Force/skip erase of partitions when full fastboot cap is
 * enabled in GBB.
 */
static unsigned char env_force_erase = 1;

static const struct {
	struct name_string name;
	fb_setenv_t var;
} setenv_table[] = {
	{ NAME_ARGS("force-erase", ':'), FB_SETENV_FORCE_ERASE},
};

static void fb_write_env(struct fb_cmd *cmd, fb_setenv_t var)
{
	const char *input = fb_buffer_head(&cmd->input);
	size_t input_len = fb_buffer_length(&cmd->input);

	switch (var) {
	case FB_SETENV_FORCE_ERASE:
		if (input_len != 1) {
			cmd->type = FB_FAIL;
			fb_add_string(&cmd->output, "invalid args", NULL);
			return;
		}
		if (*input == '1')
			env_force_erase = 1;
		else if (*input == '0')
			env_force_erase = 0;
		else {
			cmd->type = FB_FAIL;
			fb_add_string(&cmd->output, "invalid args", NULL);
		}
		break;
	default:
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output, "unsupported", NULL);
		break;
	}
}

static fb_ret_type fb_setenv(struct fb_cmd *cmd)
{
	int i;
	size_t match_len = 0;
	const char *input = fb_buffer_head(&cmd->input);
	size_t len = fb_buffer_length(&cmd->input);

	for (i = 0; i < ARRAY_SIZE(setenv_table); i++) {
		match_len = name_check_match(input, len, &setenv_table[i].name);
		if (match_len)
			break;
	}

	if (match_len == 0) {
		fb_add_string(&cmd->output, "unknown setenv command", NULL);
		cmd->type = FB_FAIL;
		return FB_SUCCESS;
	}

	fb_buffer_pull(&cmd->input, match_len);

	cmd->type = FB_OKAY;
	fb_write_env(cmd, setenv_table[i].var);
	return FB_SUCCESS;
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
	{ NAME_NO_ARGS("off-mode-charge"), FB_OFF_MODE_CHARGE},
	{ NAME_NO_ARGS("battery-voltage"), FB_BATT_VOLTAGE},
	{ NAME_NO_ARGS("variant"), FB_VARIANT},
	{ NAME_NO_ARGS("battery-soc-ok"), FB_BATT_SOC_OK},
#if CONFIG_FASTBOOT_SLOTS
	/* Slots related */
	{ NAME_ARGS("has-slot", ':'), FB_HAS_SLOT},
	{ NAME_NO_ARGS("current-slot"), FB_CURR_SLOT},
	{ NAME_NO_ARGS("slot-suffixes"), FB_SLOT_SUFFIXES},
	{ NAME_ARGS("slot-successful", ':'), FB_SLOT_SUCCESSFUL},
	{ NAME_ARGS("slot-unbootable", ':'), FB_SLOT_UNBOOTABLE},
	{ NAME_ARGS("slot-retry-count", ':'), FB_SLOT_RETRY_COUNT},
#endif
	/*
	 * OEM specific :
	 * Spec says names starting with lowercase letter are reserved.
	 */
	{ NAME_ARGS("Bdev-size", ':'), FB_BDEV_SIZE},
	{ NAME_NO_ARGS("Gbb-flags"), FB_GBB_FLAGS},
	{ NAME_ARGS("Oem-version", ':'), FB_OEM_VERSION},
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
 * Format getvar commands for variables with arguments.
 *
 * Parameters:
 * cmd_in : Current command input
 * cmd_out: Current command response
 * table: Table of different arguments to variable
 * record_size: Size of each record in 'table'
 * name_offset: Offset of name string in record
 * count: Number of records in table
 *
 */
static void fb_format_getvar_args(struct fb_buffer *cmd_in,
				  struct fb_buffer *cmd_out,
				  const void *table, size_t record_size,
				  size_t name_offset, size_t count)
{
	int j;
	struct fb_cmd curr_cmd;

	struct fb_buffer *input = &curr_cmd.input;
	struct fb_buffer *output = &curr_cmd.output;

	uintptr_t table_addr = (uintptr_t)table;

	for (j = 0; j < count; j++) {
		uintptr_t *pname_addr = (uintptr_t *)(table_addr +
						      record_size * j +
						      name_offset);

		const char *pname = (void *)*pname_addr;
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
		case FB_PART_TYPE: {
			fb_format_getvar_args(cmd_in, cmd_out, fb_part_list,
					      sizeof(fb_part_list[0]),
					      offsetof(struct part_info,
						       part_name),
					      fb_part_count);
			break;
		}

		case FB_PART_SIZE: {
			fb_format_getvar_args(cmd_in, cmd_out, fb_part_list,
					      sizeof(fb_part_list[0]),
					      offsetof(struct part_info,
						       part_name),
					      fb_part_count);
			break;
		}

		case FB_BDEV_SIZE: {
			fb_format_getvar_args(cmd_in, cmd_out, fb_bdev_list,
					      sizeof(fb_bdev_list[0]),
					      offsetof(struct bdev_info,
						       name),
					      fb_bdev_count);
			break;
		}

		case FB_OEM_VERSION: {
			fb_format_getvar_args(cmd_in, cmd_out,
					      &fb_fw_type_to_index,
					      sizeof(fb_fw_type_to_index[0]),
					      offsetof(struct fw_type_to_index,
						       name),
					      ARRAY_SIZE(fb_fw_type_to_index));
			break;
		}

#if CONFIG_FASTBOOT_SLOTS
		case FB_HAS_SLOT: {
			fb_format_getvar_args(cmd_in, cmd_out,
					      fb_base_list,
					      sizeof(fb_base_list[0]),
					      offsetof(struct part_base_info,
						       base_name),
					      fb_base_count);
			break;
		}

		case FB_SLOT_SUCCESSFUL:
		case FB_SLOT_UNBOOTABLE:
		case FB_SLOT_RETRY_COUNT: {
			struct fb_cmd curr_cmd;
			struct fb_buffer *input = &curr_cmd.input;
			struct fb_buffer *output = &curr_cmd.output;
			int i;

			for (i = 0; i < CONFIG_FASTBOOT_SLOTS_COUNT; i++) {
				fb_buffer_clone(cmd_in, input);
				fb_buffer_clone(cmd_out, output);

				fb_add_string(input, slot_get_suffix(i), NULL);
				fb_copy_buffer_data(output, input);
				fb_add_string(output, ": ", NULL);

				curr_cmd.type = FB_INFO;
				fb_getvar_single(&curr_cmd);

				if (curr_cmd.type == FB_INFO)
					fb_execute_send(&curr_cmd);
			}

			break;
		}
#endif

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

/*
 * Func: fb_recv_data
 * Desc: Download data from host
 *
 */
static int fb_recv_data(struct fb_cmd *cmd)
{
	uint64_t curr_len = 0;

	while (curr_len < image_size) {
		void *curr = (uint8_t *)fb_get_image_ptr() + curr_len;

		uint64_t ret = usb_gadget_recv(curr, image_size - curr_len);

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
	image_size = strtoul(num, NULL, 16);

	fb_free_string(num);

	if (image_size > fb_get_max_download_size()) {
		fb_add_string(output, "not sufficient memory", NULL);
		image_size = 0;
		return FB_SUCCESS;
	}

	cmd->type = FB_DATA;
	fb_add_number(output, "%08llx", image_size);
	fb_execute_send(cmd);

	if (fb_recv_data(cmd) == 0) {
		FB_LOG("Failed to download data\n");
		image_size = 0;
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
	[BE_IMAGE_INSUFFICIENT_DATA] = "image does not have enough data",
	[BE_WRITE_ERR] = "image write failed",
	[BE_SPARSE_HDR_ERR] = "sparse header error",
	[BE_CHUNK_HDR_ERR] = "sparse chunk header error",
	[BE_GPT_ERR] = "GPT error",
	[BE_INVALID_SLOT_INDEX] = "Invalid slot index",
};

static fb_ret_type fb_erase(struct fb_cmd *cmd)
{
	/* No guarantees if battery state changes during erase operation. */
	if (!battery_soc_check()) {
		FB_LOG("Battery state-of-charge not acceptable.\n");
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output,
			      "battery state-of-charge not acceptable", NULL);
		return FB_SUCCESS;
	}

	/*
	 * Check if there is an override and env_force_erase = 0, then skip
	 * erase operation. This is possible only when GBB flag
	 * FULL_FASTBOOT_CAP is true.
	 */
	if (fb_check_gbb_override() && (env_force_erase == 0)) {
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
	fb_print_on_screen(PRINT_WARN, "Erasing flash....\n");

	cmd->type = FB_OKAY;

	char *partition = fb_get_string(data, len);
	ret = backend_erase_partition(partition);
	fb_free_string(partition);

	if (ret != BE_SUCCESS) {
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output, backend_error_string[ret], NULL);
	}

	return FB_SUCCESS;
}

static fb_ret_type fb_flash(struct fb_cmd *cmd)
{
	/* No guarantees if battery state changes during flash operation. */
	if (!battery_soc_check()) {
		FB_LOG("Battery state-of-charge not acceptable.\n");
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output,
			      "battery state-of-charge not acceptable", NULL);
		return FB_SUCCESS;
	}

	backend_ret_t ret;

	if (image_size == 0) {
		fb_add_string(&cmd->output, "no image downloaded", NULL);
		cmd->type = FB_FAIL;
		return FB_SUCCESS;
	}

	FB_LOG("writing flash\n");
	cmd->type = FB_INFO;
	fb_add_string(&cmd->output, "writing flash", NULL);
	fb_execute_send(cmd);
	fb_print_on_screen(PRINT_WARN, "Writing flash....\n");

	struct fb_buffer *input = &cmd->input;
	size_t len = fb_buffer_length(input);
	char *data = fb_buffer_pull(input, len);

	cmd->type = FB_OKAY;

	char *partition = fb_get_string(data, len);

	ret = board_write_partition(partition, fb_get_image_ptr(), image_size);

	if (ret == BE_NOT_HANDLED)
		ret = backend_write_partition(partition, fb_get_image_ptr(),
					      image_size);

	fb_free_string(partition);

	if (ret != BE_SUCCESS) {
		cmd->type = FB_FAIL;
		fb_add_string(&cmd->output, backend_error_string[ret], NULL);
	}

	return FB_SUCCESS;
}

static int fb_boot_cleanup_func(struct CleanupFunc *cleanup, CleanupType type)
{
	if (type != CleanupOnHandoff)
		return -1;

	struct fb_cmd *cmd = cleanup->data;

	cmd->type = FB_OKAY;
	fb_execute_send(cmd);

	return 0;
}

/*
 * Behavior of "fastboot boot":
 * 1. Locked mode or unlocked mode with fastboot full cap not set:
 *    - Device will boot only an officially signed recovery image.
 * 2. Unlocked mode with fastboot full cap set:
 *    - Device will boot any of:
 *      a. Officially signed recovery image
 *      b. Dev-signed boot image (Hash only check)
 *      c. Any valid boot image (No signature / hash check)
 */
static fb_ret_type fb_boot(struct fb_cmd *cmd)
{
	struct vb2_context *ctx = vboot_get_context();
	VbSelectAndLoadKernelParams kparams;

	cmd->type = FB_FAIL;

	if (image_size == 0) {
		fb_add_string(&cmd->output, "no image downloaded", NULL);
		return FB_SUCCESS;
	}

	size_t kernel_size;
	void *kernel_base = &_kernel_start;

	if (kernel_base != fb_get_image_ptr())
		memcpy(kernel_base, fb_get_image_ptr(), image_size);

	void *kernel = bootimg_get_kernel_ptr(kernel_base, image_size);

	if (kernel == NULL) {
		fb_add_string(&cmd->output, "bootimg format not recognized",
			      NULL);
		return FB_SUCCESS;
	}

	kernel_size = (uintptr_t)kernel - (uintptr_t)kernel_base;

	if (kernel_size >= image_size) {
		fb_add_string(&cmd->output, "bootimg kernel not found", NULL);
		return FB_SUCCESS;
	}

	kernel_size = image_size - kernel_size;

	if (VbVerifyMemoryBootImage(ctx, &cparams, &kparams, kernel,
				    kernel_size) != VBERROR_SUCCESS) {
		/*
		 * Fail if:
		 * 1. Device is locked, or
		 * 2. Device is unlocked, but full fastboot cap is not set in
		 * GBB and VbNvStorage.
		 */
		if ((fb_device_unlocked() == 0) ||
		    ((vbnv_read(VB2_NV_DEV_BOOT_FASTBOOT_FULL_CAP) == 0) &&
		     (fb_check_gbb_override() == 0))) {
			fb_add_string(&cmd->output, "image verification failed",
				      NULL);
			return FB_SUCCESS;
		} else {
			/*
			 * If device is unlocked, required full fastboot cap
			 * flag is set and yet image verification fails, try
			 * passing the original image to vboot_boot_kernel.
			 * "fastboot boot" can simply pass a kernel image
			 * without any signature in unlocked mode with full
			 * fastboot cap set.
			 */
			kparams.kernel_buffer = kernel_base;
			kparams.kernel_buffer_size = image_size;
		}
	}

	kparams.flags = KERNEL_IMAGE_BOOTIMG;

	/* Define a cleanup function to send OKAY back if jumping to kernel. */
	static CleanupFunc fb_boot_cleanup = {
		&fb_boot_cleanup_func,
		CleanupOnHandoff,
		NULL,
	};
	fb_boot_cleanup.data = cmd;
	list_insert_after(&fb_boot_cleanup.list_node, &cleanup_funcs);

	vboot_boot_kernel(&kparams);

	/* We should never reach here, if boot successful. */
	list_remove(&fb_boot_cleanup.list_node);

	cmd->type = FB_FAIL;
	fb_add_string(&cmd->output, "Invalid boot image", NULL);

	/* We want to reset the boot state. So, reboot into fastboot mode. */
	return FB_REBOOT_BOOTLOADER;
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

static fb_ret_type fb_battery_cutoff(struct fb_cmd *cmd)
{
	FB_LOG("Cutting off battery\n");
	cmd->type = FB_FAIL;

	if (fb_board_handler.battery_cutoff == NULL) {
		fb_add_string(&cmd->output, "Unsupported cmd", NULL);
		return FB_SUCCESS;
	}

	if (fb_board_handler.battery_cutoff() != 0)
		fb_add_string(&cmd->output, "Failed to cut-off battery", NULL);
	else
		cmd->type = FB_OKAY;

	/*
	 * Don't power-off (FB_POWEROFF) immediately because we may still want
	 * to perform other commands, for example the finalization needs to
	 * run "oem lock" or "powerdown" as the last command.
	 */
	return FB_SUCCESS;
}


static int fb_user_confirmation()
{
	/* If GBB is set, we don't need to get user confirmation. */
	if (fb_check_gbb_override())
		return 1;

	if ((fb_board_handler.read_input == NULL) ||
	    (fb_board_handler.get_button_str == NULL)) {
		FB_LOG("ERROR: Read input or get str not defined by board.\n");
		return 0;
	}

	const char * const str = "Press %s to confirm, %s to cancel.";
	const char *confirm_str, *cancel_str;
	/* Wait for 1 minute to get user confirmation. */
	uint64_t read_timeout_us = 60 * 1000 * 1000;

	confirm_str = fb_board_handler.get_button_str(FB_BUTTON_CONFIRM);
	cancel_str =fb_board_handler.get_button_str(FB_BUTTON_CANCEL);

	/*
	 * Total length = (Length of str - length of 2 %s + 1 byte for \0) +
	 * 		   (Length of confirm str) + (Length of cancel str)
	 */
	size_t print_str_len = (strlen(str) - 4 + 1) + strlen(confirm_str) +
		strlen(cancel_str);
	char *print_str = malloc(print_str_len);

	snprintf(print_str, print_str_len, str, confirm_str, cancel_str);

	fb_print_on_screen(PRINT_WARN, print_str);

	free(print_str);

	return fb_board_handler.read_input(FB_BUTTON_CONFIRM | FB_BUTTON_CANCEL,
					   read_timeout_us)
		== FB_BUTTON_CONFIRM ? 1 : 0;
}

static int unlock_in_fw_set(void)
{
	/* If fastboot override is set, allow lock/unlock unconditionally. */
	if (fb_check_gbb_override())
		return 1;

	/*
	 * Else:
	 * 1. Check fastboot unlock flag in nvstorage or
	 * 2. Callback board handler if it exists.
	 */
	return (vbnv_read(VB2_NV_FASTBOOT_UNLOCK_IN_FW) ||
		(fb_board_handler.allow_unlock &&
		 fb_board_handler.allow_unlock()));
}

static fb_ret_type fb_lock(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	FB_LOG("Locking device\n");
	if (!fb_device_unlocked()) {
		fb_add_string(&cmd->output, "Device already locked", NULL);
		return FB_SUCCESS;
	}

	if (!unlock_in_fw_set()) {
		fb_add_string(&cmd->output, "Enable OEM unlock is not set",
			      NULL);
		return FB_SUCCESS;
	}

	if (!fb_user_confirmation()) {
		FB_LOG("User cancelled\n");
		fb_add_string(&cmd->output, "User cancelled request", NULL);
		return FB_SUCCESS;
	}

	vbnv_write(VB2_NV_DISABLE_DEV_REQUEST, 1);
	cmd->type = FB_OKAY;
	return FB_REBOOT;
}

static fb_ret_type fb_unlock(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	FB_LOG("Unlocking device\n");
	if (fb_device_unlocked()) {
		fb_add_string(&cmd->output, "Device already unlocked", NULL);
		return FB_SUCCESS;
	}

	if (!unlock_in_fw_set()) {
		fb_add_string(&cmd->output, "Enable OEM unlock is not set",
			      NULL);
		return FB_SUCCESS;
	}

	if (!fb_user_confirmation()) {
		FB_LOG("User cancelled\n");
		fb_add_string(&cmd->output, "User cancelled request", NULL);
		return FB_SUCCESS;
	}

	if (VbUnlockDevice() != VBERROR_SUCCESS) {
		fb_add_string(&cmd->output, "Unlock device failed", NULL);
		return FB_SUCCESS;
	}

	cmd->type = FB_OKAY;
	return FB_REBOOT;
}

static fb_ret_type fb_get_unlock_ability(struct fb_cmd *cmd)
{
	fb_add_number(&cmd->output, "%lld", unlock_in_fw_set());

	cmd->type = FB_INFO;
	fb_execute_send(cmd);

	cmd->type = FB_OKAY;
	return FB_SUCCESS;
}

#if CONFIG_FASTBOOT_SLOTS
static fb_ret_type fb_set_active_slot(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	const char *input = fb_buffer_head(&cmd->input);
	size_t input_len = fb_buffer_length(&cmd->input);

	if (input_len != 2) {
		fb_add_string(&cmd->output, "Invalid args", NULL);
		return FB_SUCCESS;
	}

	int index = slot_get_index(input, input_len);
	backend_ret_t ret = backend_set_active_slot(index);

	if (ret != BE_SUCCESS) {
		fb_add_string(&cmd->output, backend_error_string[ret], NULL);
		return FB_SUCCESS;
	}

	cmd->type = FB_OKAY;
	fb_add_string(&cmd->output, "Set active slot done", NULL);
	return FB_SUCCESS;
}
#endif

static fb_ret_type fb_set_off_mode_charge(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	if (fb_board_handler.set_boot_on_ac_detect == NULL) {
		fb_add_string(&cmd->output, "Unsupported cmd", NULL);
		return FB_SUCCESS;
	}

	const char *input = fb_buffer_head(&cmd->input);
	size_t input_len = fb_buffer_length(&cmd->input);

	if (input_len != 1) {
		fb_add_string(&cmd->output, "Invalid args", NULL);
		return FB_SUCCESS;
	}

	/*
	 * If off-mode-charge is set to 1, we want the device to continue
	 * charging in off-mode i.e. device does not boot up if AC is plugged
	 * in. On the other hand, if off-mode-charge is set to 0, then we want
	 * the device to immediately boot up if AC is plugged in. Based on
	 * passed in argument:
	 * 1. Call board handler to set/clear boot_on_ac_detect (This can
	 * default to EC or any component handling charging for the board).
	 * 2. Set flag in VBNV_STORAGE so that it can be used on later boots.
	 */
	int boot_on_ac;
	if (*input == '1')
		boot_on_ac = 0;
	else
		boot_on_ac = 1;

	if (fb_board_handler.set_boot_on_ac_detect(boot_on_ac) != 0) {
		fb_add_string(&cmd->output, "Failed to set off-mode-charge",
			      NULL);
		return FB_SUCCESS;
	}

	vbnv_write(VB2_NV_BOOT_ON_AC_DETECT, boot_on_ac);

	cmd->type = FB_OKAY;
	return FB_SUCCESS;
}

static fb_ret_type fb_clear_gbb(struct fb_cmd *cmd)
{
	if (gbb_clear_flags() == 0) {
		/*
		 * Clearing GBB implies changes in significant components:
		 * 1. Fastboot override
		 * 2. Device forced unlock status
		 *
		 * In order to maintain a clear state, it is safe to reboot the
		 * device into fastboot mode once the GBB flags are cleared.
		 */
		cmd->type = FB_OKAY;
		return FB_REBOOT_BOOTLOADER;
	}

	cmd->type = FB_FAIL;
	return FB_SUCCESS;
}

static fb_ret_type fb_write_protect(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	if (fb_board_handler.write_protect_ro == NULL) {
		FB_LOG("write_protect_ro not implemented by board.\n");
	} else if (fb_board_handler.write_protect_ro() == 0)
		cmd->type = FB_OKAY;

	return FB_SUCCESS;
}

static fb_ret_type fb_double_tap_disable(struct fb_cmd *cmd)
{
	cmd->type = FB_FAIL;

	if (fb_board_handler.double_tap_disable == NULL) {
		FB_LOG("Double tap disable not implemented for this board.\n");
	} else if (fb_board_handler.double_tap_disable() == 0)
		cmd->type = FB_OKAY;

	return FB_SUCCESS;
}

static fb_ret_type fb_usb_recovery(struct fb_cmd *cmd)
{
	cmd->type = FB_OKAY;

	return FB_CONTINUE_RECOVERY;
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
	{ NAME_ARGS("oem off-mode-charge", ' '), FB_ID_OFF_MODE_CHARGE,
	  fb_set_off_mode_charge},
	{ NAME_NO_ARGS("flashing lock"), FB_ID_LOCK, fb_lock},
	{ NAME_NO_ARGS("flashing unlock"), FB_ID_UNLOCK, fb_unlock},
	{ NAME_NO_ARGS("flashing get_unlock_ability"), FB_ID_GET_UNLOCK_ABILITY,
	  fb_get_unlock_ability},
#if CONFIG_FASTBOOT_SLOTS
	{ NAME_ARGS("set_active", ':'), FB_ID_SET_ACTIVE_SLOT,
	  fb_set_active_slot},
#endif
	/* OEM cmd names starting in uppercase imply vendor/device specific. */
	{ NAME_NO_ARGS("oem Powerdown"), FB_ID_POWERDOWN, fb_powerdown},
	{ NAME_NO_ARGS("oem Battery-cutoff"), FB_ID_BATTERY_CUTOFF,
	  fb_battery_cutoff},
	{ NAME_ARGS("oem Setenv", ' '), FB_ID_SETENV, fb_setenv},
	{ NAME_NO_ARGS("oem Clear-gbb"), FB_ID_CLEAR_GBB, fb_clear_gbb},
	{ NAME_NO_ARGS("oem Write-protect"), FB_ID_WRITE_PROTECT,
	  fb_write_protect},
	{ NAME_NO_ARGS("oem Double-tap-disable"), FB_ID_DOUBLE_TAP_DISABLE,
	  fb_double_tap_disable},
	{ NAME_NO_ARGS("oem Usb-recovery"), FB_ID_USB_RECOVERY,
	  fb_usb_recovery},
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

		fb_print_on_screen(PRINT_INFO,
				   "Waiting for fastboot command....\n");
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

		fb_print_on_screen(PRINT_WARN,
				   "Processing fastboot command....\n");

		/* Process the packet as per fastboot protocol */
		ret = fastboot_proto_handler(&cmd);

		fb_execute_send(&cmd);

		fb_buffer_rewind(&cmd.input);
	} while (ret == FB_SUCCESS);

	/* Done with usb gadget driver - Stop it */
	usb_gadget_stop();

	/*
	 * Ret should be either poweroff, reboot, reboot bootloader or continue
	 * usb recovery.
	 */
	if ((ret != FB_POWEROFF) && (ret != FB_REBOOT) &&
	    (ret != FB_REBOOT_BOOTLOADER) && (ret != FB_CONTINUE_RECOVERY))
		ret = FB_REBOOT;

	FB_LOG("********** Exit fastboot mode *****************\n");
	return ret;
}
