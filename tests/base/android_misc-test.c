// SPDX-License-Identifier: GPL-2.0
/* Copyright 2025 Google LLC. */

#include <commonlib/bsd/ipchksum.h>

#include "base/android_misc.h"
#include "tests/test.h"

/* Mock data */
BlockDev test_disk;
GptData *test_gpt = (void *)0xbeff;

/* Mocked functions */
enum gpt_io_ret gpt_read_partition(BlockDev *disk, GptData *gpt, const char *partition_name,
				   const uint64_t blocks_offset, void *data, size_t data_len)
{
	char *to_read;

	assert_ptr_equal(disk, &test_disk);
	assert_ptr_equal(gpt, test_gpt);
	assert_string_equal(partition_name, "misc");
	check_expected(blocks_offset);
	check_expected(data_len);

	to_read = mock_ptr_type(char *);
	if (to_read)
		memcpy(data, to_read, data_len);

	return mock();
}

/* Setup for gpt_read_partition mock */
#define WILL_READ_MISC(offset, len, content, ret) do { \
	expect_value(gpt_read_partition, blocks_offset, offset); \
	expect_value(gpt_read_partition, data_len, len); \
	will_return(gpt_read_partition, content); \
	will_return(gpt_read_partition, ret); \
} while (0)

#define WILL_READ_BCB(content, ret) \
	WILL_READ_MISC(0, sizeof(struct bootloader_message), content, ret)

#define WILL_READ_OEM_CMD(offset, content, ret) \
	WILL_READ_MISC(offset, sizeof(struct android_misc_oem_cmdline), content, ret)

enum gpt_io_ret gpt_write_partition(BlockDev *disk, GptData *gpt, const char *partition_name,
				    const uint64_t blocks_offset, void *data, size_t data_len)
{
	assert_ptr_equal(disk, &test_disk);
	assert_ptr_equal(gpt, test_gpt);
	assert_string_equal(partition_name, "misc");
	check_expected(blocks_offset);
	check_expected(data_len);
	check_expected(data);

	return mock();
}

/* Setup for gpt_write_partition mock */
#define WILL_WRITE_MISC(offset, len, content, ret) do { \
	expect_value(gpt_write_partition, blocks_offset, offset); \
	expect_value(gpt_write_partition, data_len, len); \
	expect_memory(gpt_write_partition, data, content, len); \
	will_return(gpt_write_partition, ret); \
} while (0)

#define WILL_WRITE_BCB(content, ret) \
	WILL_WRITE_MISC(0, sizeof(struct bootloader_message), content, ret)

#define WILL_WRITE_OEM_CMD(offset, content, ret) \
	WILL_WRITE_MISC(offset, sizeof(struct android_misc_oem_cmdline), content, ret)

void setup_test_cmd_chksum(struct android_misc_oem_cmdline *cmd, size_t size)
{
	cmd->chksum = ipchksum(cmd, size);
}

size_t setup_test_cmd(struct android_misc_oem_cmdline *cmd, const char *cmdline,
		      const char *bootconfig)
{
	memset(cmd, 0xfe, sizeof(*cmd));

	cmd->magic = ANDROID_MISC_OEM_CMDLINE_MAGIC;
	cmd->version = 0;
	cmd->cmdline_len = strlen(cmdline) + 1;
	cmd->bootconfig_len = strlen(bootconfig) + 1;
	memcpy(cmd->data, cmdline, cmd->cmdline_len);
	memcpy(cmd->data + cmd->cmdline_len, bootconfig, cmd->bootconfig_len);
	cmd->chksum = 0;
	/* Calculate number of modified bytes */
	return cmd->data + cmd->cmdline_len + cmd->bootconfig_len - (char *)cmd;
}

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	test_disk.block_size = 512;

	return 0;
}

/* Test functions */

static void test_android_misc_bcb_read_fail(void **state)
{
	struct bootloader_message bcb;

	WILL_READ_BCB(NULL, GPT_IO_NO_PARTITION);

	assert_int_equal(android_misc_bcb_read(&test_disk, test_gpt, &bcb), -1);
}

static void test_android_misc_bcb_read_ok(void **state)
{
	struct bootloader_message bcb;
	struct bootloader_message expected_bcb;

	memset(&bcb, 0xac, sizeof(bcb));
	memset(&expected_bcb, 0x3e, sizeof(expected_bcb));
	WILL_READ_BCB(&expected_bcb, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_bcb_read(&test_disk, test_gpt, &bcb), 0);
	assert_memory_equal(&bcb, &expected_bcb, sizeof(expected_bcb));
}

static void test_android_misc_bcb_write_fail(void **state)
{
	struct bootloader_message bcb;
	struct bootloader_message expected_bcb;

	memset(&bcb, 0xac, sizeof(bcb));
	memset(&expected_bcb, 0xac, sizeof(expected_bcb));
	WILL_WRITE_BCB(&expected_bcb, GPT_IO_TRANSFER_ERROR);

	assert_int_equal(android_misc_bcb_write(&test_disk, test_gpt, &bcb), -1);
}

static void test_android_misc_bcb_write_ok(void **state)
{
	struct bootloader_message bcb;
	struct bootloader_message expected_bcb;

	memset(&bcb, 0xac, sizeof(bcb));
	memset(&expected_bcb, 0xac, sizeof(expected_bcb));
	WILL_WRITE_BCB(&expected_bcb, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_bcb_write(&test_disk, test_gpt, &bcb), 0);
}

static void test_android_misc_get_bcb_command_disk_error(void **state)
{
	WILL_READ_BCB(NULL, GPT_IO_NO_PARTITION);

	assert_int_equal(android_misc_get_bcb_command(&test_disk, test_gpt),
			 MISC_BCB_DISK_ERROR);
}

static void test_android_misc_get_bcb_command_recovery(void **state)
{
	struct bootloader_message bcb;
	const char cmd[] = "boot-recovery";

	memset(&bcb, 0xac, sizeof(bcb));
	memcpy(bcb.command, cmd, sizeof(cmd));

	WILL_READ_BCB(&bcb, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_get_bcb_command(&test_disk, test_gpt),
			 MISC_BCB_RECOVERY_BOOT);
}

static void test_android_misc_get_bcb_command_bootloader(void **state)
{
	struct bootloader_message bcb;
	struct bootloader_message expected_bcb;
	const char cmd[] = "bootonce-bootloader";

	memset(&bcb, 0xac, sizeof(bcb));
	memcpy(bcb.command, cmd, sizeof(cmd));

	memset(&expected_bcb, 0xac, sizeof(expected_bcb));
	memset(expected_bcb.command, 0, sizeof(expected_bcb.command));

	WILL_READ_BCB(&bcb, GPT_IO_SUCCESS);
	WILL_WRITE_BCB(&expected_bcb, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_get_bcb_command(&test_disk, test_gpt),
			 MISC_BCB_BOOTLOADER_BOOT);
}

static void test_android_misc_get_bcb_command_normal(void **state)
{
	struct bootloader_message bcb;
	const char cmd[] = "";

	memset(&bcb, 0xac, sizeof(bcb));
	memcpy(bcb.command, cmd, sizeof(cmd));

	WILL_READ_BCB(&bcb, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_get_bcb_command(&test_disk, test_gpt),
			 MISC_BCB_NORMAL_BOOT);
}

static void test_android_misc_get_bcb_command_unknown(void **state)
{
	struct bootloader_message bcb;
	const char cmd[] = "unknown-cmd";

	memset(&bcb, 0xac, sizeof(bcb));
	memcpy(bcb.command, cmd, sizeof(cmd));

	WILL_READ_BCB(&bcb, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_get_bcb_command(&test_disk, test_gpt),
			 MISC_BCB_UNKNOWN);
}

static void test_android_misc_is_cmd_separator(void **state)
{
	assert_true(is_android_misc_cmd_separator(' ', false));
	assert_true(is_android_misc_cmd_separator('\n', false));
	assert_true(is_android_misc_cmd_separator('\t', false));
	assert_false(is_android_misc_cmd_separator(';', false));
	assert_false(is_android_misc_cmd_separator(':', false));
	assert_false(is_android_misc_cmd_separator('\'', false));
	assert_false(is_android_misc_cmd_separator(',', false));
}

static void test_android_misc_is_bootconfig_separator(void **state)
{
	assert_true(is_android_misc_cmd_separator('\n', true));
	assert_true(is_android_misc_cmd_separator(';', true));
	assert_false(is_android_misc_cmd_separator('\t', true));
	assert_false(is_android_misc_cmd_separator(' ', true));
	assert_false(is_android_misc_cmd_separator(':', true));
	assert_false(is_android_misc_cmd_separator('\'', true));
	assert_false(is_android_misc_cmd_separator(',', true));
}

static void test_android_misc_is_oem_cmdline_valid_okay(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_true(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_magic(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	cmd.magic = 0xcafebeff;
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_version(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	cmd.version = 1;
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_len_too_long(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	cmd.cmdline_len = sizeof(cmd);
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_len_too_short_cmd(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "", "ootconfig;");
	cmd_len -= 1;
	cmd.cmdline_len = 0;
	/* Fix bootconfig value */
	cmd.data[0] = 'b';
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_len_too_short_bootconfig(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "");
	cmd_len -= cmd.bootconfig_len;
	cmd.bootconfig_len = 0;
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_no_delimiter_cmd(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_no_delimiter_bootconfig(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig");
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_no_null_char_cmd(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	cmd.data[cmd.cmdline_len - 1] = 'a';
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_no_null_char_bootconfig(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	cmd.data[cmd.cmdline_len + cmd.bootconfig_len - 1] = 'a';
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_is_oem_cmdline_invalid_checksum(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);
	cmd.chksum += 5;

	assert_false(is_android_misc_oem_cmdline_valid(&cmd));
}

static void test_android_misc_oem_cmdline_update_checksum(void **state)
{
	struct android_misc_oem_cmdline cmd;
	uint16_t expected_chksum;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);
	expected_chksum = cmd.chksum;
	cmd.chksum += 5;

	assert_int_equal(android_misc_oem_cmdline_update_checksum(&cmd), 0);
	assert_int_equal(cmd.chksum, expected_chksum);
}

static void test_android_misc_oem_cmdline_update_checksum_bad_len(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);
	cmd.cmdline_len = sizeof(cmd);
	cmd.chksum += 5;

	assert_int_equal(android_misc_oem_cmdline_update_checksum(&cmd), -1);
}

static void test_android_misc_oem_cmdline_write(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;

	size_t cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&expected_cmd, cmd_len);
	memcpy(&cmd, &expected_cmd, sizeof(cmd));
	/* Checksum should be fixed before write */
	cmd.chksum += 5;

	WILL_WRITE_OEM_CMD(8, &expected_cmd, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_oem_cmdline_write(&test_disk, test_gpt, &cmd), 0);

	test_disk.block_size = 4096;
	WILL_WRITE_OEM_CMD(1, &expected_cmd, GPT_IO_TRANSFER_ERROR);

	assert_int_equal(android_misc_oem_cmdline_write(&test_disk, test_gpt, &cmd), -1);
}

static void test_android_misc_oem_cmdline_write_bad_checksum(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);
	/* Provoke failure on updating checksum */
	cmd.cmdline_len = sizeof(cmd);

	assert_int_equal(android_misc_oem_cmdline_write(&test_disk, test_gpt, &cmd), -1);
}

static void test_android_misc_oem_cmdline_read(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;

	size_t cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&expected_cmd, cmd_len);

	WILL_READ_OEM_CMD(8, &expected_cmd, GPT_IO_SUCCESS);

	assert_int_equal(android_misc_oem_cmdline_read(&test_disk, test_gpt, &cmd), 0);

	test_disk.block_size = 4096;
	WILL_READ_OEM_CMD(1, &expected_cmd, GPT_IO_TRANSFER_ERROR);

	assert_int_equal(android_misc_oem_cmdline_read(&test_disk, test_gpt, &cmd), -1);
}

static void test_android_misc_oem_cmdline_read_invalid(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;

	size_t cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "bootconfig;");
	expected_cmd.magic = 0x123;
	setup_test_cmd_chksum(&expected_cmd, cmd_len);

	WILL_READ_OEM_CMD(8, &expected_cmd, GPT_IO_SUCCESS);

	/* Should fail, because the loaded cmd is invalid */
	assert_int_equal(android_misc_oem_cmdline_read(&test_disk, test_gpt, &cmd), -1);

}

static void test_android_misc_get_oem_cmd(void **state)
{
	struct android_misc_oem_cmdline cmd;

	size_t cmd_len = setup_test_cmd(&cmd, "cmdline ", "bootconfig;");
	setup_test_cmd_chksum(&cmd, cmd_len);

	assert_string_equal(android_misc_get_oem_cmd(&cmd, false), "cmdline ");
	assert_string_equal(android_misc_get_oem_cmd(&cmd, true), "bootconfig;");
}

static void test_android_misc_append_oem_cmdline(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	size_t cmd_len;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline param=123 ", "bootconfig;");
	assert_int_equal(android_misc_append_oem_cmd(&cmd, "param=123", false), 0);
	assert_string_equal(cmd.data, expected_cmd.data);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline param=123 test abcd ef ",
				 "bootconfig;");
	assert_int_equal(android_misc_append_oem_cmd(&cmd, "test abcd ef", false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline param=123 test abcd ef arg ",
				 "bootconfig;");
	assert_int_equal(android_misc_append_oem_cmd(&cmd, "arg", false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}

static void test_android_misc_append_oem_cmdline_too_long(void **state)
{
	struct android_misc_oem_cmdline cmd;
	char long_str[sizeof(cmd)];

	memset(long_str, 'A', sizeof(long_str));
	long_str[sizeof(long_str) - 1] = '\0';
	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	assert_int_equal(android_misc_append_oem_cmd(&cmd, long_str, false), -1);
}

static void test_android_misc_append_oem_bootconfig(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	size_t cmd_len;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "bootconfig;param=123\n");
	assert_int_equal(android_misc_append_oem_cmd(&cmd, "param=123", true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ",
				 "bootconfig;param=123\ntest abcd ef\n");
	assert_int_equal(android_misc_append_oem_cmd(&cmd, "test abcd ef", true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ",
				 "bootconfig;param=123\ntest abcd ef\narg\n");
	assert_int_equal(android_misc_append_oem_cmd(&cmd, "arg", true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}

static void test_android_misc_append_oem_bootconfig_too_long(void **state)
{
	struct android_misc_oem_cmdline cmd;
	char long_str[sizeof(cmd)];

	memset(long_str, 'A', sizeof(long_str));
	long_str[sizeof(long_str) - 1] = '\0';
	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	assert_int_equal(android_misc_append_oem_cmd(&cmd, long_str, true), -1);
}

static void test_android_misc_del_oem_cmdline(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	size_t cmd_len;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "cmdle ", "bootconfig;");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 4, 2, false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "dle ", "bootconfig;");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 0, 2, false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "d", "bootconfig;");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 1, 3, false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "", "bootconfig;");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 0, 1, false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}


static void test_android_misc_del_oem_cmdline_wrong_range(void **state)
{
	struct android_misc_oem_cmdline cmd;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	/* Too long */
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 4, 5, false), -1);
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 2, 9, false), -1);

	/* Index outside of cmd */
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 8, 4, false), -1);
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 10, 9, false), -1);
}

static void test_android_misc_del_oem_bootconfig(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	size_t cmd_len;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "bootfig;");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 4, 3, true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "otfig;");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 0, 2, true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "otf");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 3, 3, true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "");
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 0, 3, true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}

static void test_android_misc_del_oem_bootconfig_wrong_range(void **state)
{
	struct android_misc_oem_cmdline cmd;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	/* Too long */
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 4, 8, true), -1);
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 2, 11, true), -1);

	/* Index outside of cmd */
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 11, 4, true), -1);
	assert_int_equal(android_misc_del_oem_cmd(&cmd, 13, 9, true), -1);
}

static void test_android_misc_set_oem_cmdline(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	size_t cmd_len;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "newcmdline ", "bootconfig;");
	assert_int_equal(android_misc_set_oem_cmd(&cmd, "newcmdline", false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}

static void test_android_misc_set_oem_cmdline_too_big(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	char long_str[sizeof(cmd) / 2];
	size_t cmd_len;

	memset(long_str, 'A', sizeof(long_str));
	long_str[sizeof(long_str) - 1] = '\0';
	long_str[sizeof(long_str) - 2] = '\n';
	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", long_str);
	long_str[sizeof(long_str) - 2] = '\0';
	assert_int_equal(android_misc_set_oem_cmd(&cmd, long_str, true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	/* Cannot set cmdline, because bootconfig already occupy more than half of the space */
	assert_int_equal(android_misc_set_oem_cmd(&cmd, long_str, false), -1);
}

static void test_android_misc_set_oem_bootconfig(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	size_t cmd_len;

	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, "cmdline ", "newbootconfig\n");
	assert_int_equal(android_misc_set_oem_cmd(&cmd, "newbootconfig", true), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}

static void test_android_misc_set_oem_bootconfig_too_big(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;
	char long_str[sizeof(cmd) / 2];
	size_t cmd_len;

	memset(long_str, 'A', sizeof(long_str));
	long_str[sizeof(long_str) - 1] = '\0';
	long_str[sizeof(long_str) - 2] = ' ';
	setup_test_cmd(&cmd, "cmdline ", "bootconfig;");

	cmd_len = setup_test_cmd(&expected_cmd, long_str, "bootconfig;");
	long_str[sizeof(long_str) - 2] = '\0';
	assert_int_equal(android_misc_set_oem_cmd(&cmd, long_str, false), 0);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);

	/* Cannot set bootconfig, because cmdline already occupy more than half of the space */
	assert_int_equal(android_misc_set_oem_cmd(&cmd, long_str, true), -1);
}

static void test_android_misc_reset_oem_cmd(void **state)
{
	struct android_misc_oem_cmdline cmd;
	struct android_misc_oem_cmdline expected_cmd;

	size_t cmd_len = setup_test_cmd(&expected_cmd, "", "");
	setup_test_cmd_chksum(&expected_cmd, cmd_len);

	android_misc_reset_oem_cmd(&cmd);
	assert_memory_equal(&cmd, &expected_cmd, cmd_len);
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_android_misc_bcb_read_fail),
		TEST(test_android_misc_bcb_read_ok),
		TEST(test_android_misc_bcb_write_fail),
		TEST(test_android_misc_bcb_write_ok),
		TEST(test_android_misc_get_bcb_command_disk_error),
		TEST(test_android_misc_get_bcb_command_recovery),
		TEST(test_android_misc_get_bcb_command_bootloader),
		TEST(test_android_misc_get_bcb_command_normal),
		TEST(test_android_misc_get_bcb_command_unknown),
		TEST(test_android_misc_is_cmd_separator),
		TEST(test_android_misc_is_bootconfig_separator),
		TEST(test_android_misc_is_oem_cmdline_valid_okay),
		TEST(test_android_misc_is_oem_cmdline_invalid_magic),
		TEST(test_android_misc_is_oem_cmdline_invalid_version),
		TEST(test_android_misc_is_oem_cmdline_invalid_len_too_long),
		TEST(test_android_misc_is_oem_cmdline_invalid_len_too_short_cmd),
		TEST(test_android_misc_is_oem_cmdline_invalid_len_too_short_bootconfig),
		TEST(test_android_misc_is_oem_cmdline_invalid_no_delimiter_cmd),
		TEST(test_android_misc_is_oem_cmdline_invalid_no_delimiter_bootconfig),
		TEST(test_android_misc_is_oem_cmdline_invalid_no_null_char_cmd),
		TEST(test_android_misc_is_oem_cmdline_invalid_no_null_char_bootconfig),
		TEST(test_android_misc_is_oem_cmdline_invalid_checksum),
		TEST(test_android_misc_oem_cmdline_update_checksum),
		TEST(test_android_misc_oem_cmdline_update_checksum_bad_len),
		TEST(test_android_misc_oem_cmdline_write),
		TEST(test_android_misc_oem_cmdline_write_bad_checksum),
		TEST(test_android_misc_oem_cmdline_read),
		TEST(test_android_misc_oem_cmdline_read_invalid),
		TEST(test_android_misc_get_oem_cmd),
		TEST(test_android_misc_append_oem_cmdline),
		TEST(test_android_misc_append_oem_cmdline_too_long),
		TEST(test_android_misc_append_oem_bootconfig),
		TEST(test_android_misc_append_oem_bootconfig_too_long),
		TEST(test_android_misc_del_oem_cmdline),
		TEST(test_android_misc_del_oem_cmdline_wrong_range),
		TEST(test_android_misc_del_oem_bootconfig),
		TEST(test_android_misc_del_oem_bootconfig_wrong_range),
		TEST(test_android_misc_set_oem_cmdline),
		TEST(test_android_misc_set_oem_cmdline_too_big),
		TEST(test_android_misc_set_oem_bootconfig),
		TEST(test_android_misc_set_oem_bootconfig_too_big),
		TEST(test_android_misc_reset_oem_cmd),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
