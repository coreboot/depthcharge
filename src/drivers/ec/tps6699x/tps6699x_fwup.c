// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#include <stdio.h>

#include "arch/types.h"
#include "base/init_funcs.h"
#include "drivers/ec/common/pdc_utils.h"
#include "drivers/ec/cros/ec.h"

#include "tps6699x.h"
#include "tps6699x_i2c.h"
#include "tps6699x_fwup.h"

#define TPS_4CC_POLL_DELAY_US 1000	       /* 1ms */
#define TPS_4CC_TIMEOUT_US 5000000	       /* 5sec */
#define TPS_RESET_DELAY_MS 1000		       /* 1s */
#define TPS_TFUC_REBOOT_DELAY_MS 1600	       /* 1600ms */
#define TPS_TFUD_BLOCK_DELAY_MS 150	       /* 150ms */
#define TPS_TFUI_HEADER_DELAY_MS 200	       /* 200ms */
#define TPS_TFUS_ATTEMPT_RETRY_DELAY_MS 100    /* 100ms */
#define TPS_TFUS_BOOTLOADER_ENTRY_DELAY_MS 500 /* 500ms */

#define TPS_TFUS_ATTEMPT_COUNT 3

#define TPS_DATA_METADATA_OFFSET_AT(block)                                     \
	(((TPS6699X_DATA_BLOCK_SIZE + TPS6699X_METADATA_LENGTH) * block) +     \
	 TPS6699X_DATA_REGION_OFFSET)

#define TPS_DATA_AT(block)                                                     \
	(TPS_DATA_METADATA_OFFSET_AT(block) + TPS6699X_METADATA_LENGTH)

/**
 * @brief Read a section of the memory-mapped FW image
 *
 * @param me Chip object that has pointer to image and its length pre-loaded
 * @param buf Output location to write pointer to requested location
 * @param len Number of bytes to read from image (for bounds checking)
 * @return ssize_t Number of bytes to read, or -1 on error.
 */
static ssize_t read_file_offset(Tps6699x *me, int offset, const uint8_t **buf,
				int len)
{
	/* Exceed size of file. */
	if (me->fw_image.image == NULL || offset + len > me->fw_image.size)
		return -1;

	*buf = &me->fw_image.image[offset];

	return len;
}

static int get_appconfig_offsets(Tps6699x *me, uint16_t num_data_blocks,
				 int *metadata_offset, int *data_block_offset)
{
	int bytes_read;
	uint32_t *fw_size;

	bytes_read =
		read_file_offset(me, TPS6699X_FW_SIZE_OFFSET,
				 (const uint8_t **)&fw_size, sizeof(fw_size));

	if (bytes_read < 0) {
		printf("%s: Failed to read firmware size from binary: %d\n",
		       me->chip_name, bytes_read);
		return -1;
	}

	/* The Application Configuration is stored at the following offset
	 * FirmwareImageSize (Which excludes Header and App Config) + 0x800
	 * (Header Block Size)
	 * + (8 (Meta Data for Each Block including Header block) * Number of
	 * Data block + 1)
	 * + 4 (File Identifier)
	 */

	*metadata_offset = *fw_size + TPS6699X_HEADER_BLOCK_LENGTH +
			   (TPS6699X_METADATA_LENGTH * (num_data_blocks + 1)) +
			   TPS6699X_METADATA_OFFSET;

	*data_block_offset = *metadata_offset + TPS6699X_METADATA_LENGTH;

	return 0;
}

/**
 * @brief Convert a 4CC command/task enum to a NUL-terminated printable string
 *
 * @param task The 4CC task enum
 * @param str_out Pointer to a char array capable of holding 5 characters where
 *        the output will be written to.
 */
static void command_task_to_string(enum tps6699x_4cc_tasks task,
				   char str_out[5])
{
	if (task == 0) {
		strncpy(str_out, "0000", strlen("0000") + 1);
		return;
	}

	/* Task enum is a set of 4 ASCII chars (TI 4CC codes) */
	str_out[0] = (((uint32_t)task) >> 0);
	str_out[1] = (((uint32_t)task) >> 8);
	str_out[2] = (((uint32_t)task) >> 16);
	str_out[3] = (((uint32_t)task) >> 24);
	str_out[4] = '\0';
}

/**
 * @brief Get the current PDC FW version and print it to console.
 *
 * @param me Chip object
 * @return int 0 on success
 */
static int get_and_print_device_info(Tps6699x *me)
{
	union reg_version version;
	int rv;

	rv = tps6699x_version_read(me, &version);
	if (rv != 0)
		return rv;

	printf("%s: Current version %u.%u.%u\n", me->chip_name,
	       PDC_FWVER_MAJOR(version.version),
	       PDC_FWVER_MINOR(version.version),
	       PDC_FWVER_PATCH(version.version));

	return 0;
}

/**
 * @brief Run a 4CC task/command synchronously
 *
 * @param me Chip object
 * @param task 4-byte task code string
 * @param cmd_data Command data payload
 * @param user_buf Output buffer for response
 * @return int 0 on success
 */
static int run_task_sync(Tps6699x *me, enum tps6699x_4cc_tasks task,
			 union reg_data *cmd_data, uint8_t *user_buf)
{
	union reg_command cmd;
	int rv;
	char task_str[5];

	command_task_to_string(task, task_str);

	/* Set up self-contained synchronous command call */
	if (cmd_data) {
		rv = tps6699x_data_reg_write(me, cmd_data);
		if (rv) {
			printf("%s: Cannot set command data for '%s' (%d)\n",
			       me->chip_name, task_str, rv);
			return rv;
		}
	}

	cmd.command = task;

	rv = tps6699x_command_reg_write(me, &cmd);
	if (rv) {
		printf("%s: Cannot set command for '%s' (%d)\n", me->chip_name,
		       task_str, rv);
		return rv;
	}

	/* Poll for successful completion */
	uint64_t timeout = timer_us(0);

	while (1) {
		udelay(TPS_4CC_POLL_DELAY_US);

		rv = tps6699x_command_reg_read(me, &cmd);
		if (rv) {
			printf("%s: Cannot poll command status for '%s' (%d)\n",
			       me->chip_name, task_str, rv);
			return rv;
		}

		if (cmd.command == 0) {
			/* Command complete */
			break;
		} else if (cmd.command == COMMAND_TASK_NO_COMMAND) {
			/* Unknown command ("!CMD") */
			printf("%s: Command '%s' is invalid\n", me->chip_name,
			       task_str);
			return -1;
		}

		if (timer_us(timeout) > TPS_4CC_TIMEOUT_US) {
			printf("%s: Command '%s' timed out\n", me->chip_name,
			       task_str);
			return -2;
		}
	}

	printf("%s: Command '%s' finished...\n", me->chip_name, task_str);

	/* Read out success code */
	union reg_data cmd_data_check;

	rv = tps6699x_data_reg_read(me, &cmd_data_check);
	if (rv) {
		printf("%s: Cannot get command result status for '%s' (%d)\n",
		       me->chip_name, task_str, rv);
		return rv;
	}

	/* Data byte offset 0 is the return error code */
	if (cmd_data_check.data[0] != 0) {
		printf("%s: Command '%s' failed. Chip says %02x\n",
		       me->chip_name, task_str, cmd_data_check.data[0]);
		return rv;
	}

	printf("%s: Command '%s' succeeded\n", me->chip_name, task_str);

	/* Provide response data to user if a buffer is provided */
	if (user_buf != NULL)
		memcpy(user_buf, cmd_data_check.data,
		       sizeof(cmd_data_check.data));

	return 0;
}

static int do_reset_pdc(Tps6699x *me)
{
	union reg_data cmd_data;
	union gaid_params_t params;
	int rv;

	/* Default behavior is to switch banks. */
	params.switch_banks = TPS6699X_GAID_MAGIC_VALUE;
	params.copy_banks = 0;

	memcpy(cmd_data.data, &params, sizeof(params));

	rv = run_task_sync(me, COMMAND_TASK_GAID, &cmd_data, NULL);

	if (rv == 0)
		mdelay(TPS_RESET_DELAY_MS);

	return rv;
}

static int tfus_run(Tps6699x *me)
{
	int ret;
	uint64_t start_time;

	union reg_command cmd = {
		.command = COMMAND_TASK_TFUS,
	};

	/* Make multiple attempts to run the TFUs command to start FW update. */
	for (int attempts = 0; attempts < TPS_TFUS_ATTEMPT_COUNT; attempts++) {
		ret = tps6699x_command_reg_write(me, &cmd);
		if (ret == 0)
			break;

		mdelay(TPS_TFUS_ATTEMPT_RETRY_DELAY_MS);
	}

	if (ret) {
		printf("%s: Cannot write TFUs command (%d)\n", me->chip_name,
		       ret);
		return ret;
	}

	/* Wait 500ms for entry to bootloader mode, per datasheet */
	mdelay(TPS_TFUS_BOOTLOADER_ENTRY_DELAY_MS);

	/* Allow up to an additional 200ms */
	start_time = timer_us(0);
	do {
		/* Check mode register for "F211" value */
		union reg_mode mode;

		ret = tps6699x_mode_read(me, &mode);

		if (ret == 0) {
			/* Got a mode result */
			if (memcmp("F211", mode.data, sizeof(mode.data)) == 0) {
				printf("%s: TFUs succeeded\n", me->chip_name);
				return 0;
			}

			/* Wrong mode, continue re-trying */
			printf("%s: TFUs failed! Mode is '%c%c%c%c'\n",
			       me->chip_name, mode.data[0], mode.data[1],
			       mode.data[2], mode.data[3]);
		} else {
			/* I2C error, continue re-trying */
			printf("%s: Cannot read mode reg (%d)\n", me->chip_name,
			       ret);
		}

		mdelay(50);
	} while (timer_us(start_time) < 200000);

	printf("%s: TFUs timed out\n", me->chip_name);
	return -1;
}

static int tfud_block(Tps6699x *me, uint8_t *fbuf, int metadata_offset,
		      int data_block_offset)
{
	struct tfu_download *tfud;
	union reg_data cmd_data;
	int bytes_read;
	uint8_t rbuf[64];
	int ret;

	/* First read the block metadata. */
	bytes_read =
		read_file_offset(me, metadata_offset, (const uint8_t **)&tfud,
				 TPS6699X_METADATA_LENGTH);

	if (bytes_read < 0 || bytes_read != TPS6699X_METADATA_LENGTH) {
		printf("%s: Failed to read block metadata. Wanted %d, got %d\n",
		       me->chip_name, TPS6699X_METADATA_LENGTH, bytes_read);
		return -1;
	}

	printf("%s: TFUd Info: nblks=%u, blksize=%u, timeout=%us, addr=%x\n",
	       me->chip_name, tfud->num_blocks, tfud->data_block_size,
	       tfud->timeout_secs, tfud->broadcast_address);

	if (tfud->data_block_size > TPS6699X_DATA_BLOCK_SIZE) {
		printf("%s: TFUd block size too big: 0x%x (max is 0x%x)\n",
		       me->chip_name, tfud->data_block_size,
		       TPS6699X_DATA_BLOCK_SIZE);
		return -1;
	}

	memcpy(&cmd_data.data, tfud, sizeof(*tfud));
	ret = run_task_sync(me, COMMAND_TASK_TFUD, &cmd_data, rbuf);

	if (ret < 0 || rbuf[0] != 0) {
		printf("%s: Failed to run TFUd. Ret=%d, rbuf[0] = %u\n",
		       me->chip_name, ret, rbuf[0]);
		return -1;
	}

	bytes_read =
		read_file_offset(me, data_block_offset, (const uint8_t **)&fbuf,
				 tfud->data_block_size);

	if (bytes_read < 0 || bytes_read != tfud->data_block_size) {
		printf("%s: Failed to read block. Wanted %d, got %d\n",
		       me->chip_name, tfud->data_block_size, bytes_read);
		return -1;
	}

	/* Stream the data block */
	ret = tps6699x_stream_data(me, tfud->broadcast_address, fbuf,
				   tfud->data_block_size);
	if (ret) {
		printf("%s: Downloading data block failed (%d)\n",
		       me->chip_name, ret);
		return -1;
	}

	/* Wait 150ms after each data block. */
	mdelay(TPS_TFUD_BLOCK_DELAY_MS);

	return 0;
}

/**
 * @brief Run the TFUq (TFU Query) 4CC command to check update progress
 *
 * @param me Chip object
 * @param output Response data output buffer
 * @return int 0 on success
 */
static int tfuq_run(Tps6699x *me, uint8_t *output)
{
	union reg_data cmd_data;
	struct tfu_query *tfuq = (struct tfu_query *)cmd_data.data;

	tfuq->bank = 0;
	tfuq->cmd = 0;

	return run_task_sync(me, COMMAND_TASK_TFUQ, &cmd_data, output);
};

int tps6699x_perform_fw_update(Tps6699x *me)
{
	int appconfig_metadata_offset, appconfig_data_offset;
	struct tfu_initiate *tfui;
	union reg_data cmd_data;
	int bytes_read = 0;
	uint8_t rbuf[64];
	int ret = 0;
	uint8_t *fbuf;

	/*
	 * Flow of operations for firmware update:
	 *   - TFUs: Start TFU process (puts device into bootloader mode)
	 *   - TFUi: Initiate firmware update. This also validates header.
	 *   - TFUd - Loop to download firmware.
	 *   - TFUc - Complete firmware update.
	 *
	 * To cancel or query current status, you can also do the following:
	 *   - TFUq: Query the TFU process
	 *   - TFUe: Cancel back to initial download state.
	 */

	/********************
	 * TFUs stage - enter bootloader code
	 */

	ret = tfus_run(me);
	if (ret) {
		printf("%s: Cannot enter bootloader mode (%d)\n", me->chip_name,
		       ret);
		return ret;
	}

	/********************
	 * TFUi stage
	 */

	/* Read metadata header. */
	bytes_read = read_file_offset(me, TPS6699X_METADATA_OFFSET,
				      (const uint8_t **)&tfui,
				      TPS6699X_METADATA_LENGTH);
	if (bytes_read < 0) {
		printf("%s: Failed to read metadata. Wanted %d, got %d\n",
		       me->chip_name, TPS6699X_METADATA_LENGTH, bytes_read);
		goto cleanup;
	}

	printf("%s: Sending TFUi\n", me->chip_name);

	/* Write TFUi with header. */
	memcpy(cmd_data.data, tfui, sizeof(*tfui));
	ret = run_task_sync(me, COMMAND_TASK_TFUI, &cmd_data, rbuf);

	if (ret < 0 || rbuf[0] != 0) {
		printf("%s: Failed to run TFUi. Ret=%d, rbuf[0]=%u\n",
		       me->chip_name, ret, rbuf[0]);
		goto cleanup;
	}

	/* Read metadata buffer and stream at address given. */
	bytes_read = read_file_offset(me, TPS6699X_HEADER_BLOCK_OFFSET,
				      (const uint8_t **)&fbuf,
				      TPS6699X_HEADER_BLOCK_LENGTH);
	if (bytes_read < 0 || bytes_read != TPS6699X_HEADER_BLOCK_LENGTH) {
		printf("%s: Failed to read header stream. Wanted %d but got "
		       "%d\n",
		       me->chip_name, TPS6699X_HEADER_BLOCK_LENGTH, bytes_read);
		goto cleanup;
	}

	printf("%s: Streaming header to broadcast addr $%x\n", me->chip_name,
	       tfui->broadcast_address);

	ret = tps6699x_stream_data(me, tfui->broadcast_address, fbuf,
				   TPS6699X_HEADER_BLOCK_LENGTH);
	if (ret) {
		printf("%s: Streaming header failed (%d)\n", me->chip_name,
		       ret);
		goto cleanup;
	}

	printf("%s: TFUi complete and header streamed. Number of blocks: %u\n",
	       me->chip_name, tfui->num_blocks);

	/* Wait 200ms after streaming header to do data block. */
	mdelay(TPS_TFUI_HEADER_DELAY_MS);

	/* Iterate through all image blocks. */
	for (int block = 0; block < tfui->num_blocks; ++block) {
		printf("%s: Flashing block %d (%d/%u)\n", me->chip_name, block,
		       block + 1, tfui->num_blocks);
		ret = tfud_block(me, fbuf, TPS_DATA_METADATA_OFFSET_AT(block),
				 TPS_DATA_AT(block));
		if (ret) {
			printf("%s: Error while flashing block (%d)\n",
			       me->chip_name, ret);
			goto cleanup;
		}
	}

	printf("%s: Flashing appconfig to block %d", me->chip_name,
	       tfui->num_blocks);
	if (get_appconfig_offsets(me, tfui->num_blocks,
				  &appconfig_metadata_offset,
				  &appconfig_data_offset) < 0) {
		printf("%s: Failed to get appconfig offsets!\n", me->chip_name);
		goto cleanup;
	}

	ret = tfud_block(me, fbuf, appconfig_metadata_offset,
			 appconfig_data_offset);
	if (ret) {
		printf("%s: Failed to write appconfig block (%d)\n",
		       me->chip_name, ret);
		goto cleanup;
	}

	/* Check the status with TFUq */
	ret = tfuq_run(me, rbuf);
	if (ret) {
		printf("%s: Could not query FW update status (%d)\n",
		       me->chip_name, ret);
		goto cleanup;
	}

	printf("%s: Raw TFUq response:\n", me->chip_name);
	hexdump(rbuf, sizeof(struct tps6699x_tfu_query_output));

	/* Finish update with a TFU copy. */
	struct tfu_complete tfuc;
	tfuc.do_switch = 0;
	tfuc.do_copy = TPS6699X_DO_COPY;

	printf("%s: Running TFUc [Switch: 0x%02x, Copy: 0x%02x]\n",
	       me->chip_name, tfuc.do_switch, tfuc.do_copy);
	memcpy(cmd_data.data, &tfuc, sizeof(tfuc));
	ret = run_task_sync(me, COMMAND_TASK_TFUC, &cmd_data, rbuf);

	if (ret < 0 || rbuf[0] != 0) {
		printf("%s: Failed 4cc task with result %d, rbuf[0] = %d\n",
		       me->chip_name, ret, rbuf[0]);
		goto cleanup;
	}

	printf("%s: TFUq bytes [Success: 0x%02x, State: 0x%02x, Complete: "
	       "0x%02x]\n",
	       me->chip_name, rbuf[1], rbuf[2], rbuf[3]);

	/* Wait for reset to complete. */
	mdelay(TPS_TFUC_REBOOT_DELAY_MS);

	/* Confirm we're on the new firmware now. */
	get_and_print_device_info(me);

	return 0;

cleanup:
	ret = run_task_sync(me, COMMAND_TASK_TFUE, NULL, rbuf);

	printf("%s: Cleaning up resulted in ret=%d and result byte=0x%02x\n",
	       me->chip_name, ret, rbuf[0]);

	/* Reset and confirm we restored original firmware. */
	do_reset_pdc(me);
	get_and_print_device_info(me);

	return -1;
}
