// SPDX-License-Identifier: GPL-2.0

#include "base/android_misc.h"
#include "drivers/storage/ufs.h"
#include "fastboot/fastboot.h"
#include "tests/fastboot/fastboot_common_mocks.h"
#include "tests/test.h"

/* Mock data */
UfsCtlr test_ufs;
char _kernel_start[0x100];
/*
 * Magic number for tests to validate that the same android_misc_oem_cmdline is used for
 * different commands
 */
const int test_cmd_magic = 0x123;

/* Mocked functions */
int android_misc_bcb_write(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb)
{
	assert_ptr_equal(disk, &test_disk);
	assert_ptr_equal(gpt, &test_gpt);
	check_expected(bcb->command);
	check_expected(bcb->recovery);

	return mock();
}

/* Setup for android_misc_bcb_write mock */
#define WILL_WRITE_BCB(cmd, rec, ret) do { \
	expect_string(android_misc_bcb_write, bcb->command, cmd); \
	expect_string(android_misc_bcb_write, bcb->recovery, rec); \
	will_return(android_misc_bcb_write, ret); \
} while (0)

int android_misc_bcb_read(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb)
{
	assert_ptr_equal(disk, &test_disk);
	assert_ptr_equal(gpt, &test_gpt);

	return mock();
}

/* Setup for android_misc_bcb_read mock */
#define WILL_READ_BCB(ret) will_return(android_misc_bcb_read, ret)

int android_misc_oem_cmdline_write(BlockDev *disk, GptData *gpt,
				   struct android_misc_oem_cmdline *cmd)
{
	assert_ptr_equal(disk, &test_disk);
	assert_ptr_equal(gpt, &test_gpt);
	assert_int_equal(cmd->magic, test_cmd_magic);

	return mock();
}

/* Setup for android_misc_oem_cmdline_write mock */
#define WILL_WRITE_CMDLINE(ret) will_return(android_misc_oem_cmdline_write, ret)

int android_misc_oem_cmdline_read(BlockDev *disk, GptData *gpt,
				  struct android_misc_oem_cmdline *cmd)
{
	assert_ptr_equal(disk, &test_disk);
	assert_ptr_equal(gpt, &test_gpt);
	cmd->magic = test_cmd_magic;

	return mock();
}

/* Setup for android_misc_oem_cmdline_read mock */
#define WILL_READ_CMDLINE(ret) will_return(android_misc_oem_cmdline_read, ret)

char *android_misc_get_oem_cmd(struct android_misc_oem_cmdline *cmd, bool bootconfig)
{
	check_expected(bootconfig);
	assert_int_equal(cmd->magic, test_cmd_magic);

	return mock_ptr_type(char *);
}

/* Setup for android_misc_get_oem_cmd mock */
#define WILL_GET_OEM_CMD(cmdline) will_return(android_misc_get_oem_cmd, cmdline)

#define WILL_GET_CMDLINE(cmdline) do { \
	expect_value(android_misc_get_oem_cmd, bootconfig, false); \
	WILL_GET_OEM_CMD(cmdline); \
} while (0)

#define WILL_GET_BOOTCONFIG(cmdline) do { \
	expect_value(android_misc_get_oem_cmd, bootconfig, true); \
	WILL_GET_OEM_CMD(cmdline); \
} while (0)

int android_misc_append_oem_cmd(struct android_misc_oem_cmdline *cmd, const char *buf,
				bool bootconfig)
{
	check_expected(bootconfig);
	assert_int_equal(cmd->magic, test_cmd_magic);
	check_expected(buf);

	return mock();
}

/* Setup for android_misc_append_oem_cmd mock */
#define WILL_APPEND_OEM_CMD(cmd_data, ret) do { \
	expect_string(android_misc_append_oem_cmd, buf, cmd_data); \
	will_return(android_misc_append_oem_cmd, ret); \
} while (0)

#define WILL_APPEND_CMDLINE(cmd, ret) do { \
	expect_value(android_misc_append_oem_cmd, bootconfig, false); \
	WILL_APPEND_OEM_CMD(cmd, ret); \
} while (0)

#define WILL_APPEND_BOOTCONFIG(cmd, ret) do { \
	expect_value(android_misc_append_oem_cmd, bootconfig, true); \
	WILL_APPEND_OEM_CMD(cmd, ret); \
} while (0)

int android_misc_del_oem_cmd(struct android_misc_oem_cmdline *cmd, const size_t idx,
			     const size_t len, bool bootconfig)
{
	char *cmdline;
	int cmd_len;

	check_expected(bootconfig);
	assert_int_equal(cmd->magic, test_cmd_magic);
	check_expected(idx);
	check_expected(len);

	cmdline = mock_ptr_type(char *);
	cmd_len = strlen(cmdline) + 1;
	memmove(cmdline + idx, cmdline + idx + len, cmd_len - idx - len);

	return mock();
}

/* Setup for android_misc_del_oem_cmd mock */
#define WILL_DEL_OEM_CMD(data, i, l, ret) do { \
	expect_value(android_misc_del_oem_cmd, idx, i); \
	expect_value(android_misc_del_oem_cmd, len, l); \
	will_return(android_misc_del_oem_cmd, data); \
	will_return(android_misc_del_oem_cmd, ret); \
} while (0)

#define WILL_DEL_CMDLINE(data, i, l, ret) do { \
	expect_value(android_misc_del_oem_cmd, bootconfig, false); \
	WILL_DEL_OEM_CMD(data, i, l, ret); \
} while (0)

#define WILL_DEL_BOOTCONFIG(data, i, l, ret) do { \
	expect_value(android_misc_del_oem_cmd, bootconfig, true); \
	WILL_DEL_OEM_CMD(data, i, l, ret); \
} while (0)

int android_misc_set_oem_cmd(struct android_misc_oem_cmdline *cmd, const char *buf,
			     bool bootconfig)
{
	check_expected(bootconfig);
	assert_int_equal(cmd->magic, test_cmd_magic);
	check_expected(buf);

	return mock();
}

/* Setup for android_misc_set_oem_cmd mock */
#define WILL_SET_OEM_CMD(cmd_data, ret) do { \
	expect_string(android_misc_set_oem_cmd, buf, cmd_data); \
	will_return(android_misc_set_oem_cmd, ret); \
} while (0)

#define WILL_SET_CMDLINE(cmd, ret) do { \
	expect_value(android_misc_set_oem_cmd, bootconfig, false); \
	WILL_SET_OEM_CMD(cmd, ret); \
} while (0)

#define WILL_SET_BOOTCONFIG(cmd, ret) do { \
	expect_value(android_misc_set_oem_cmd, bootconfig, true); \
	WILL_SET_OEM_CMD(cmd, ret); \
} while (0)

void android_misc_reset_oem_cmd(struct android_misc_oem_cmdline *cmd)
{
	function_called();
	cmd->magic = test_cmd_magic;
}

/* Setup for android_misc_reset_oem_cmd mock */
#define WILL_RESET_CMDLINE expect_function_call(android_misc_reset_oem_cmd)

void fastboot_write(struct FastbootOps *fb, const char *partition_name,
		    const uint64_t blocks_offset, void *data, size_t data_len)
{
	check_expected_ptr(fb);
	check_expected(partition_name);
	check_expected(blocks_offset);
	check_expected_ptr(data);
	check_expected(data_len);
}

/* Setup for fastboot_write mock */
#define WILL_WRITE(fb_ptr, part, offset, length) do { \
	expect_value(fastboot_write, fb, fb_ptr); \
	expect_string(fastboot_write, partition_name, part); \
	expect_value(fastboot_write, blocks_offset, offset); \
	expect_value(fastboot_write, data, &_kernel_start); \
	expect_value(fastboot_write, data_len, length); \
} while (0)

void fastboot_erase(struct FastbootOps *fb, const char *partition_name)
{
	check_expected_ptr(fb);
	check_expected(partition_name);
}

/* Setup for fastboot_erase mock */
#define WILL_ERASE(fb_ptr, part) do { \
	expect_value(fastboot_erase, fb, fb_ptr); \
	expect_string(fastboot_erase, partition_name, part); \
} while (0)

bool gpt_foreach_partition(GptData *gpt, gpt_foreach_callback_t cb, void *ctx)
{
	GptEntry *e;
	int index;
	char *name;
	bool ret = false;

	assert_ptr_equal(gpt, &test_gpt);

	while (!ret && (e = mock_ptr_type(GptEntry *))) {
		index = mock();
		name = mock_ptr_type(char *);
		ret = cb(ctx, index, e, name);
	}

	return ret;
}

/* Setup for gpt_foreach_partition mock */
#define GPT_FOREACH_WILL_RETURN_ENTRY(entry, index, name) do { \
	will_return(gpt_foreach_partition, entry); \
	will_return(gpt_foreach_partition, index); \
	will_return(gpt_foreach_partition, name); \
} while (0)

#define GPT_FOREACH_WILL_END will_return(gpt_foreach_partition, NULL)

char get_slot_for_partition_name(GptEntry *e, char *partition_name)
{
	check_expected_ptr(e);
	check_expected(partition_name);

	return mock();
}

/* Setup for gpt_slot_for_partition_name mock */
#define WILL_GET_SLOT(entry, name, slot) do { \
	expect_value(get_slot_for_partition_name, e, entry); \
	expect_string(get_slot_for_partition_name, partition_name, name); \
	will_return(get_slot_for_partition_name, slot); \
} while (0)

int GetEntryPriority(const GptEntry *e)
{
	check_expected_ptr(e);

	return mock();
}

/* Setup for GetEntryPriority mock */
#define WILL_GET_PRIORITY(entry, priority) do { \
	expect_value(GetEntryPriority, e, entry); \
	will_return(GetEntryPriority, priority); \
} while (0)

GptEntry *fastboot_get_kernel_for_slot(GptData *gpt, char slot)
{
	assert_ptr_equal(gpt, &test_gpt);
	check_expected(slot);

	return mock_ptr_type(GptEntry *);
}

/* Setup for fastboot_get_kernel_for_slot mock */
#define WILL_GET_KERNEL_FOR_SLOT(slot_arg, entry) do { \
	expect_value(fastboot_get_kernel_for_slot, slot, slot_arg); \
	will_return(fastboot_get_kernel_for_slot, entry); \
} while (0)

int GptUpdateKernelWithEntry(GptData *gpt, GptEntry *e, uint32_t update_type)
{
	assert_ptr_equal(gpt, &test_gpt);
	check_expected_ptr(e);
	check_expected(update_type);

	return mock();
}

/* Setup for GptUpdateKernelWithEntry mock */
#define WILL_UPDATE_KERNEL_ENTRY(entry, type, ret) do { \
	expect_value(GptUpdateKernelWithEntry, e, entry); \
	expect_value(GptUpdateKernelWithEntry, update_type, type); \
	will_return(GptUpdateKernelWithEntry, ret); \
} while (0)

void fastboot_slots_disable_all(GptData *gpt)
{
	assert_ptr_equal(gpt, &test_gpt);
	function_called();
}

/* Setup for fastboot_slots_disable_all mock */
#define WILL_DISABLE_ALL_SLOTS expect_function_call(fastboot_slots_disable_all)

int fastboot_save_gpt(struct FastbootOps *fb)
{
	check_expected_ptr(fb);

	return mock();
}

/* Setup for fastboot_save_gpt mock */
#define WILL_SAVE_GPT(fb_ptr, ret) do { \
	expect_value(fastboot_save_gpt, fb, fb_ptr); \
	will_return(fastboot_save_gpt, ret); \
} while (0)


void fastboot_cmd_getvar(struct FastbootOps *fb, const char *args)
{
	check_expected_ptr(fb);
	check_expected(args);
}

/* Setup for fastboot_cmd_getvar mock */
#define WILL_GETVAR(fb_ptr, varname) do { \
	expect_value(fastboot_cmd_getvar, fb, fb_ptr); \
	expect_string(fastboot_cmd_getvar, args, varname); \
} while (0)

UfsCtlr *ufs_get_ctlr(void)
{
	return mock_ptr_type(UfsCtlr *);
}

/* Setup for ufs_get_ctlr mock */
#define WILL_GET_UFS_CTLR(ufs) will_return(ufs_get_ctlr, ufs)

int ufs_read_descriptor(UfsCtlr *ufs, uint8_t idn, uint8_t idx,
			uint8_t *buf, uint64_t len, uint8_t *resp_len)
{
	check_expected_ptr(ufs);
	check_expected(idn);
	check_expected(idx);
	assert_ptr_equal(buf, &_kernel_start);
	assert_int_equal(len, FASTBOOT_MAX_DOWNLOAD_SIZE);

	*resp_len = mock();

	return mock();
}

/* Setup for ufs_read_descriptor mock */
#define WILL_READ_UFS_DESCRIPTOR(ufs_ptr, idn_v, idx_v, len_ret, ret) do { \
	expect_value(ufs_read_descriptor, ufs, ufs_ptr); \
	expect_value(ufs_read_descriptor, idn, idn_v); \
	expect_value(ufs_read_descriptor, idx, idx_v); \
	will_return(ufs_read_descriptor, len_ret); \
	will_return(ufs_read_descriptor, ret); \
} while (0)

int ufs_write_descriptor(UfsCtlr *ufs, uint8_t idn, uint8_t idx,
			 uint8_t *buf, uint64_t len)
{
	check_expected_ptr(ufs);
	check_expected(idn);
	check_expected(idx);
	assert_ptr_equal(buf, &_kernel_start);
	check_expected(len);

	return mock();
}

/* Setup for ufs_write_descriptor mock */
#define WILL_WRITE_UFS_DESCRIPTOR(ufs_ptr, idn_v, idx_v, buf_len, ret) do { \
	expect_value(ufs_write_descriptor, ufs, ufs_ptr); \
	expect_value(ufs_write_descriptor, idn, idn_v); \
	expect_value(ufs_write_descriptor, idx, idx_v); \
	expect_value(ufs_write_descriptor, len, buf_len); \
	will_return(ufs_write_descriptor, ret); \
} while (0)

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	setup_test_fb();

	*state = &test_fb;

	return 0;
}

/* Test functions start here */

static void test_fb_cmd_continue(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "continue";

	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, FINISHED);
}

static void test_fb_cmd_unknown(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "not_exisiting_command";

	WILL_SEND_EXACT(fb, "FAILUnknown command");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_download_bigger_than_max_download_size(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "download:ff000000";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_download_overflow(void **state)
{
	/* TODO(b/435626385): strtol doesn't handle overflow */
	skip();

	struct FastbootOps *fb = *state;
	char cmd[] = "download:0ff00000000000000001000";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_download_nan(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "download:0ff0thisisnotnumber";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_download_ok(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "download:00001230";

	WILL_SEND_EXACT(fb, "DATA00001230");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, DOWNLOAD);
	assert_int_equal(fb->memory_buffer_len, 0x1230);
	assert_int_equal(fb->download_progress, 0);
	assert_false(fb->has_staged_data);
}

static void test_fb_cmd_reboot(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "reboot";

	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, REBOOT);
}

static void test_fb_cmd_reboot_recovery(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "reboot-recovery";

	WILL_READ_BCB(0);
	WILL_WRITE_BCB("boot-recovery", "recovery\n", 0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, REBOOT);
}

static void test_fb_cmd_reboot_recovery_write_fail(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "reboot-recovery";

	WILL_READ_BCB(0);
	WILL_WRITE_BCB("boot-recovery", "recovery\n", 1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_reboot_recovery_read_fail(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "reboot-recovery";

	WILL_READ_BCB(-1);
	WILL_SEND_PREFIX(fb, "INFO");
	WILL_WRITE_BCB("boot-recovery", "recovery\n", 0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, REBOOT);
}

static void test_fb_cmd_reboot_fastbootd(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "reboot-fastboot";

	WILL_READ_BCB(0);
	WILL_WRITE_BCB("boot-recovery", "recovery\n--fastboot\n", 0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, REBOOT);
}

static void test_fb_cmd_reboot_unknown_target(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "reboot-non_existing_target";

	WILL_READ_BCB(0);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_erase(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "erase:partitionname";

	WILL_ERASE(fb, "partitionname");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_flash_no_download(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "flash:partitionname";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_flash(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "flash:partitionname";

	fb->has_staged_data = true;
	fb->memory_buffer_len = 123;

	WILL_WRITE(fb, "partitionname", 0, 123);

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_getvar(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "getvar:varname";

	WILL_GETVAR(fb, "varname");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_active_no_slot(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "set_active:x";

	WILL_GET_KERNEL_FOR_SLOT('x', NULL);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_active(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "set_active:b";
	GptEntry kernel_entry;

	WILL_GET_KERNEL_FOR_SLOT('b', &kernel_entry);
	WILL_DISABLE_ALL_SLOTS;
	WILL_UPDATE_KERNEL_ENTRY(&kernel_entry, GPT_UPDATE_ENTRY_ACTIVE, 0);
	WILL_SAVE_GPT(fb, 0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_cmdline(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline set testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_SET_CMDLINE("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_cmdline_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline set testparam=\"value test\"";

	WILL_READ_CMDLINE(-1);
	WILL_RESET_CMDLINE;
	WILL_SET_CMDLINE("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_cmdline_fail_set(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline set testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_SET_CMDLINE("testparam=\"value test\"", -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_cmdline_fail_write(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline set testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_SET_CMDLINE("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_bootconfig(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig set testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_SET_BOOTCONFIG("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_bootconfig_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig set testparam=\"value test\"";

	WILL_READ_CMDLINE(-1);
	WILL_RESET_CMDLINE;
	WILL_SET_BOOTCONFIG("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_bootconfig_fail_set(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig set testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_SET_BOOTCONFIG("testparam=\"value test\"", -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_set_bootconfig_fail_write(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig set testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_SET_BOOTCONFIG("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_cmdline(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline add testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_APPEND_CMDLINE("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_cmdline_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline add testparam=\"value test\"";

	WILL_READ_CMDLINE(-1);
	WILL_RESET_CMDLINE;
	WILL_APPEND_CMDLINE("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_cmdline_fail_set(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline add testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_APPEND_CMDLINE("testparam=\"value test\"", -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_cmdline_fail_write(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline add testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_APPEND_CMDLINE("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_cmdline_empty_arg(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline add ";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_bootconfig(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig add testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_APPEND_BOOTCONFIG("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_bootconfig_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig add testparam=\"value test\"";

	WILL_READ_CMDLINE(-1);
	WILL_RESET_CMDLINE;
	WILL_APPEND_BOOTCONFIG("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_bootconfig_fail_set(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig add testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_APPEND_BOOTCONFIG("testparam=\"value test\"", -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_bootconfig_fail_write(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig add testparam=\"value test\"";

	WILL_READ_CMDLINE(0);
	WILL_APPEND_BOOTCONFIG("testparam=\"value test\"", 0);
	WILL_WRITE_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_append_bootconfig_empty_arg(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig add ";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_get_cmdline(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline get";
	char cmdline[] = "param param2=test param_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_SEND_EXACT(fb, "INFOparam");
	WILL_SEND_EXACT(fb, "INFOparam2=test");
	WILL_SEND_EXACT(fb, "INFOparam_dq=\"val' ue\"");
	WILL_SEND_EXACT(fb, "INFOparam_q='va");
	WILL_SEND_EXACT(fb, "INFOlue'");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_get_empty_cmdline(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline get";
	char cmdline[] = "";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_SEND_EXACT(fb, "INFO<empty>");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_get_cmdline_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline get";

	WILL_READ_CMDLINE(-1);
	WILL_SEND_EXACT(fb, "INFO<empty>");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_get_bootconfig(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig get";
	char bootconfig[] =
		"param;param2 = test\nparam3=val;param4=text with space;param_dq=\"val';ue\"\n"
		"param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_SEND_EXACT(fb, "INFOparam");
	WILL_SEND_EXACT(fb, "INFOparam2 = test");
	WILL_SEND_EXACT(fb, "INFOparam3=val");
	WILL_SEND_EXACT(fb, "INFOparam4=text with space");
	WILL_SEND_EXACT(fb, "INFOparam_dq=\"val';ue\"");
	WILL_SEND_EXACT(fb, "INFOparam_q='va\"\nlue'");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_get_empty_bootconfig(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig get";
	char bootconfig[] = "";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_SEND_EXACT(fb, "INFO<empty>");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_get_bootconfig_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig get";

	WILL_READ_CMDLINE(-1);
	WILL_SEND_EXACT(fb, "INFO<empty>");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_cmdline(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del par_dq=\"val' ue\"";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_DEL_CMDLINE(cmdline, 18, 17, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "param param2=test param_q='va lue' ");
}

static void test_fb_cmd_del_cmdline_empty_arg(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del ";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_cmdline_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del param";

	WILL_READ_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_last_cmdline(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del lue'";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_DEL_CMDLINE(cmdline, 47, 5, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "param param2=test par_dq=\"val' ue\" param_q='va ");
}

static void test_fb_cmd_del_cmdline_pattern(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del param*";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	/* Delete "param" */
	WILL_DEL_CMDLINE(cmdline, 0, 6, 0);
	/* Delete "param2=test" */
	WILL_DEL_CMDLINE(cmdline, 0, 12, 0);
	/* Delete "param_q='va" */
	WILL_DEL_CMDLINE(cmdline, 17, 12, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "par_dq=\"val' ue\" lue' ");
}

static void test_fb_cmd_del_cmdline_multi_pattern(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del par*=*";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	/* Delete "param2=test" */
	WILL_DEL_CMDLINE(cmdline, 6, 12, 0);
	/* Delete "par_dq=\"val' ue\"" */
	WILL_DEL_CMDLINE(cmdline, 6, 17, 0);
	/* Delete "param_q='va" */
	WILL_DEL_CMDLINE(cmdline, 6, 12, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "param lue' ");
}

static void test_fb_cmd_del_cmdline_escaped_pattern(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del par*=**abc*f";
	char cmdline[] = "param par1=*abcdef par2=...abcdef par3=*abcdefg par4=*abcdefgf last ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	/* Delete "par1=*abcdef" */
	WILL_DEL_CMDLINE(cmdline, 6, 13, 0);
	/* Delete "par4=*abcdefgf" */
	WILL_DEL_CMDLINE(cmdline, 35, 15, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "param par2=...abcdef par3=*abcdefg last ");
}

static void test_fb_cmd_del_cmdline_no_exist(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del param_";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "param param2=test par_dq=\"val' ue\" param_q='va lue' ");
}

static void test_fb_cmd_del_cmdline_fail_del(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del param";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_DEL_CMDLINE(cmdline, 0, 6, -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_cmdline_fail_write(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem cmdline del param2=test";
	char cmdline[] = "param param2=test par_dq=\"val' ue\" param_q='va lue' ";

	WILL_READ_CMDLINE(0);
	WILL_GET_CMDLINE(cmdline);
	WILL_DEL_CMDLINE(cmdline, 6, 12, 0);
	WILL_WRITE_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(cmdline, "param par_dq=\"val' ue\" param_q='va lue' ");
}

static void test_fb_cmd_del_bootconfig(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del par_dq=\"val';ue\"";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_DEL_BOOTCONFIG(bootconfig, 20, 17, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig, "param;param2 = test\nparam_q='va\"\nlue'\n");
}

static void test_fb_cmd_del_bootconfig_empty_arg(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del ";

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_bootconfig_fail_read(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del param";

	WILL_READ_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_last_bootconfig(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del param_q='va\"\nlue'";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_DEL_BOOTCONFIG(bootconfig, 37, 18, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig, "param;param2 = test\npar_dq=\"val';ue\";");
}

static void test_fb_cmd_del_bootconfig_pattern(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del param*";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	/* Delete "param" */
	WILL_DEL_BOOTCONFIG(bootconfig, 0, 6, 0);
	/* Delete "param2 = test" */
	WILL_DEL_BOOTCONFIG(bootconfig, 0, 14, 0);
	/* Delete "param_q='va\"\nlue'" */
	WILL_DEL_BOOTCONFIG(bootconfig, 17, 18, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig, "par_dq=\"val';ue\";");
}

static void test_fb_cmd_del_bootconfig_multi_pattern(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del par*=*";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	/* Delete "param2 = test" */
	WILL_DEL_BOOTCONFIG(bootconfig, 6, 14, 0);
	/* Delete "par_dq=\"val' ue\"" */
	WILL_DEL_BOOTCONFIG(bootconfig, 6, 17, 0);
	/* Delete "param_q='va\"\nlue'" */
	WILL_DEL_BOOTCONFIG(bootconfig, 6, 18, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig, "param;");
}

static void test_fb_cmd_del_bootconfig_escaped_pattern(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del par*=**abc*f";
	char bootconfig[] =
		"param;par1=*abcdef\npar2=...abcdef;par3=*abcdefg\npar4=*abcdefgf;last\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	/* Delete "par1=*abcdef" */
	WILL_DEL_BOOTCONFIG(bootconfig, 6, 13, 0);
	/* Delete "par4=*abcdefgf" */
	WILL_DEL_BOOTCONFIG(bootconfig, 35, 15, 0);
	WILL_WRITE_CMDLINE(0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig, "param;par2=...abcdef;par3=*abcdefg\nlast\n");
}

static void test_fb_cmd_del_bootconfig_no_exist(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del param_";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig,
			    "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n");
}

static void test_fb_cmd_del_bootconfig_fail_del(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del param";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_DEL_BOOTCONFIG(bootconfig, 0, 6, -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_del_bootconfig_fail_write(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem bootconfig del param2 = test";
	char bootconfig[] = "param;param2 = test\npar_dq=\"val';ue\";param_q='va\"\nlue'\n";

	WILL_READ_CMDLINE(0);
	WILL_GET_BOOTCONFIG(bootconfig);
	WILL_DEL_BOOTCONFIG(bootconfig, 6, 14, 0);
	WILL_WRITE_CMDLINE(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_string_equal(bootconfig, "param;par_dq=\"val';ue\";param_q='va\"\nlue'\n");
}

static void test_fb_cmd_oem_get_kernels(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem get-kernels";
	GptEntry entries[4];

	GPT_FOREACH_WILL_RETURN_ENTRY(&entries[0], 0, "boot_a");
	WILL_GET_SLOT(&entries[0], "boot_a", 'a');
	WILL_GET_PRIORITY(&entries[0], 5);
	WILL_SEND_EXACT(fb, "INFOa:boot_a:prio=5");

	GPT_FOREACH_WILL_RETURN_ENTRY(&entries[1], 1, "boot_b");
	WILL_GET_SLOT(&entries[1], "boot_b", 'b');
	WILL_GET_PRIORITY(&entries[1], 7);
	WILL_SEND_EXACT(fb, "INFOb:boot_b:prio=7");

	GPT_FOREACH_WILL_RETURN_ENTRY(&entries[2], 2, "super");
	WILL_GET_SLOT(&entries[2], "super", 0);

	GPT_FOREACH_WILL_RETURN_ENTRY(&entries[3], 4, "metadata_a");
	WILL_GET_SLOT(&entries[3], "metadata_a", 0);

	GPT_FOREACH_WILL_END;
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_upload_no_staged(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "upload";

	fb->has_staged_data = false;
	fb->memory_buffer_len = 123;

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_upload_no_len(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "upload";

	fb->has_staged_data = true;
	fb->memory_buffer_len = 0;

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

/* TODO(b/430379190): Replace this test when packet splitting is implemented */
static void test_fb_cmd_upload_no_splitting(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "upload";

	fb->has_staged_data = true;
	/* This is for sure more than max ethernet frame length */
	fb->memory_buffer_len = 50000;

	WILL_SEND_PREFIX(fb, "INFO");
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_upload(void **state)
{
	struct FastbootOps *fb = *state;
	const size_t data_len = 0x53;
	char cmd[] = "upload";
	char data[data_len];

	for (int i = 0; i < data_len; i++) {
		data[i] = 'a' + i % 26;
		_kernel_start[i] = 'a' + i % 26;
	}
	data[data_len - 1] = '\0';
	_kernel_start[data_len - 1] = '\0';

	fb->has_staged_data = true;
	fb->memory_buffer_len = data_len;

	WILL_SEND_EXACT(fb, "DATA00000053");
	WILL_SEND_EXACT(fb, data);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
	assert_false(fb->has_staged_data);
	assert_int_equal(fb->memory_buffer_len, 0);
}

static void test_fb_cmd_read_ufs_desc(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem read-ufs-descriptor:4,5";

	fb->has_staged_data = false;
	fb->memory_buffer_len = 0;

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_READ_UFS_DESCRIPTOR(&test_ufs, 0x4, 0x5, 0x53, 0);
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);

	assert_int_equal(fb->state, COMMAND);
	assert_true(fb->has_staged_data);
	assert_int_equal(fb->memory_buffer_len, 0x53);
}

static void test_fb_cmd_read_ufs_desc_no_ctlr(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem read-ufs-descriptor:4,5";

	WILL_GET_UFS_CTLR(NULL);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_read_ufs_desc_bad_args(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd1[] = "oem read-ufs-descriptor:,5";

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd1, sizeof(cmd1) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd2[] = "oem read-ufs-descriptor:,";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd2, sizeof(cmd2) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd3[] = "oem read-ufs-descriptor:3,";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd3, sizeof(cmd3) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd4[] = "oem read-ufs-descriptor:";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd4, sizeof(cmd4) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd5[] = "oem read-ufs-descriptor:23,12notnum";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd5, sizeof(cmd5) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_read_ufs_desc_args_too_big(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd1[] = "oem read-ufs-descriptor:100,5";

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd1, sizeof(cmd1) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd2[] = "oem read-ufs-descriptor:ab,1ef";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd2, sizeof(cmd2) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_read_ufs_desc_read_fail(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem read-ufs-descriptor:4a,5b";

	fb->has_staged_data = false;
	fb->memory_buffer_len = 0;

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_READ_UFS_DESCRIPTOR(&test_ufs, 0x4a, 0x5b, 0x53, -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_write_ufs_desc(void **state)
{
	struct FastbootOps *fb = *state;
	const size_t data_len = 0x53;
	char cmd[] = "oem write-ufs-descriptor:4,5";

	fb->has_staged_data = true;
	fb->memory_buffer_len = data_len;

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_WRITE_UFS_DESCRIPTOR(&test_ufs, 0x4, 0x5, data_len, 0);
	WILL_SEND_PREFIX(fb, "INFO");
	WILL_SEND_PREFIX(fb, "OKAY");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);

	assert_int_equal(fb->state, COMMAND);
	assert_false(fb->has_staged_data);
	assert_int_equal(fb->memory_buffer_len, 0);
}

static void test_fb_cmd_write_ufs_desc_no_data(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd[] = "oem write-ufs-descriptor:4,5";

	fb->has_staged_data = false;

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);

	assert_int_equal(fb->state, COMMAND);
	assert_false(fb->has_staged_data);
}

static void test_fb_cmd_write_ufs_desc_too_much_data(void **state)
{
	struct FastbootOps *fb = *state;
	const size_t data_len = UFS_DESCRIPTOR_MAX_SIZE + 1;
	char cmd[] = "oem write-ufs-descriptor:4,5";

	fb->has_staged_data = true;
	fb->memory_buffer_len = data_len;

	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);

	assert_int_equal(fb->state, COMMAND);
	assert_true(fb->has_staged_data);
	assert_int_equal(fb->memory_buffer_len, data_len);
}

static void test_fb_cmd_write_ufs_desc_no_ctlr(void **state)
{
	struct FastbootOps *fb = *state;
	const size_t data_len = 0x53;
	char cmd[] = "oem write-ufs-descriptor:4,5";

	fb->has_staged_data = true;
	fb->memory_buffer_len = data_len;

	WILL_GET_UFS_CTLR(NULL);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);

	assert_int_equal(fb->state, COMMAND);
	assert_true(fb->has_staged_data);
	assert_int_equal(fb->memory_buffer_len, data_len);
}

static void test_fb_cmd_write_ufs_desc_bad_args(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd1[] = "oem write-ufs-descriptor:,5";

	fb->has_staged_data = true;
	fb->memory_buffer_len = 0x10;

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd1, sizeof(cmd1) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd2[] = "oem write-ufs-descriptor:,";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd2, sizeof(cmd2) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd3[] = "oem write-ufs-descriptor:3,";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd3, sizeof(cmd3) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd4[] = "oem write-ufs-descriptor:";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd4, sizeof(cmd4) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd5[] = "oem write-ufs-descriptor:23,12notnum";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd5, sizeof(cmd5) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_write_ufs_desc_args_too_big(void **state)
{
	struct FastbootOps *fb = *state;
	char cmd1[] = "oem read-ufs-descriptor:100,5";

	fb->has_staged_data = true;
	fb->memory_buffer_len = 0x10;

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd1, sizeof(cmd1) - 1);
	assert_int_equal(fb->state, COMMAND);

	char cmd2[] = "oem read-ufs-descriptor:ab,1ef";
	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd2, sizeof(cmd2) - 1);
	assert_int_equal(fb->state, COMMAND);
}

static void test_fb_cmd_write_ufs_desc_write_fail(void **state)
{
	struct FastbootOps *fb = *state;
	const size_t data_len = 0x53;
	char cmd[] = "oem write-ufs-descriptor:4,5";

	fb->has_staged_data = true;
	fb->memory_buffer_len = data_len;

	WILL_GET_UFS_CTLR(&test_ufs);
	WILL_WRITE_UFS_DESCRIPTOR(&test_ufs, 0x4, 0x5, data_len, -1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_handle_packet(fb, cmd, sizeof(cmd) - 1);

	assert_int_equal(fb->state, COMMAND);
	assert_true(fb->has_staged_data);
	assert_int_equal(fb->memory_buffer_len, data_len);
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_fb_cmd_continue),
		TEST(test_fb_cmd_unknown),
		TEST(test_fb_cmd_download_bigger_than_max_download_size),
		TEST(test_fb_cmd_download_overflow),
		TEST(test_fb_cmd_download_nan),
		TEST(test_fb_cmd_download_ok),
		TEST(test_fb_cmd_reboot),
		TEST(test_fb_cmd_reboot_recovery),
		TEST(test_fb_cmd_reboot_recovery_write_fail),
		TEST(test_fb_cmd_reboot_recovery_read_fail),
		TEST(test_fb_cmd_reboot_fastbootd),
		TEST(test_fb_cmd_reboot_unknown_target),
		TEST(test_fb_cmd_erase),
		TEST(test_fb_cmd_flash_no_download),
		TEST(test_fb_cmd_flash),
		TEST(test_fb_cmd_getvar),
		TEST(test_fb_cmd_set_active_no_slot),
		TEST(test_fb_cmd_set_active),
		TEST(test_fb_cmd_set_cmdline),
		TEST(test_fb_cmd_set_cmdline_fail_read),
		TEST(test_fb_cmd_set_cmdline_fail_set),
		TEST(test_fb_cmd_set_cmdline_fail_write),
		TEST(test_fb_cmd_set_bootconfig),
		TEST(test_fb_cmd_set_bootconfig_fail_read),
		TEST(test_fb_cmd_set_bootconfig_fail_set),
		TEST(test_fb_cmd_set_bootconfig_fail_write),
		TEST(test_fb_cmd_append_cmdline),
		TEST(test_fb_cmd_append_cmdline_fail_read),
		TEST(test_fb_cmd_append_cmdline_fail_set),
		TEST(test_fb_cmd_append_cmdline_fail_write),
		TEST(test_fb_cmd_append_cmdline_empty_arg),
		TEST(test_fb_cmd_append_bootconfig),
		TEST(test_fb_cmd_append_bootconfig_fail_read),
		TEST(test_fb_cmd_append_bootconfig_fail_set),
		TEST(test_fb_cmd_append_bootconfig_fail_write),
		TEST(test_fb_cmd_append_bootconfig_empty_arg),
		TEST(test_fb_cmd_get_cmdline),
		TEST(test_fb_cmd_get_empty_cmdline),
		TEST(test_fb_cmd_get_cmdline_fail_read),
		TEST(test_fb_cmd_get_bootconfig),
		TEST(test_fb_cmd_get_empty_bootconfig),
		TEST(test_fb_cmd_get_bootconfig_fail_read),
		TEST(test_fb_cmd_del_cmdline),
		TEST(test_fb_cmd_del_cmdline_empty_arg),
		TEST(test_fb_cmd_del_cmdline_fail_read),
		TEST(test_fb_cmd_del_last_cmdline),
		TEST(test_fb_cmd_del_cmdline_pattern),
		TEST(test_fb_cmd_del_cmdline_multi_pattern),
		TEST(test_fb_cmd_del_cmdline_escaped_pattern),
		TEST(test_fb_cmd_del_cmdline_no_exist),
		TEST(test_fb_cmd_del_cmdline_fail_del),
		TEST(test_fb_cmd_del_cmdline_fail_write),
		TEST(test_fb_cmd_del_bootconfig),
		TEST(test_fb_cmd_del_bootconfig_empty_arg),
		TEST(test_fb_cmd_del_bootconfig_fail_read),
		TEST(test_fb_cmd_del_last_bootconfig),
		TEST(test_fb_cmd_del_bootconfig_pattern),
		TEST(test_fb_cmd_del_bootconfig_multi_pattern),
		TEST(test_fb_cmd_del_bootconfig_escaped_pattern),
		TEST(test_fb_cmd_del_bootconfig_no_exist),
		TEST(test_fb_cmd_del_bootconfig_fail_del),
		TEST(test_fb_cmd_del_bootconfig_fail_write),
		TEST(test_fb_cmd_oem_get_kernels),
		TEST(test_fb_cmd_upload_no_staged),
		TEST(test_fb_cmd_upload_no_len),
		TEST(test_fb_cmd_upload_no_splitting),
		TEST(test_fb_cmd_upload),
		TEST(test_fb_cmd_read_ufs_desc),
		TEST(test_fb_cmd_read_ufs_desc_no_ctlr),
		TEST(test_fb_cmd_read_ufs_desc_bad_args),
		TEST(test_fb_cmd_read_ufs_desc_args_too_big),
		TEST(test_fb_cmd_read_ufs_desc_read_fail),
		TEST(test_fb_cmd_write_ufs_desc),
		TEST(test_fb_cmd_write_ufs_desc_no_data),
		TEST(test_fb_cmd_write_ufs_desc_too_much_data),
		TEST(test_fb_cmd_write_ufs_desc_no_ctlr),
		TEST(test_fb_cmd_write_ufs_desc_bad_args),
		TEST(test_fb_cmd_write_ufs_desc_args_too_big),
		TEST(test_fb_cmd_write_ufs_desc_write_fail),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
