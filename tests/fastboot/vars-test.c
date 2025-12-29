// SPDX-License-Identifier: GPL-2.0

#include <commonlib/list.h>
#include <libpayload.h>

#include "fastboot/fastboot.h"
#include "fastboot/vars.h"
#include "tests/fastboot/fastboot_common_mocks.h"
#include "tests/test.h"

/* Mock data */
struct sysinfo_t lib_sysinfo;

/* Mocked functions */
int fastboot_get_slot_count(GptData *gpt)
{
	assert_ptr_equal(gpt, &test_gpt);

	return mock();
}

/* Setup for fastboot_get_slot_count mock */
#define WILL_GET_SLOT_COUNT(ret) will_return(fastboot_get_slot_count, ret)

char *gpt_get_entry_name(GptEntry *e)
{
	check_expected(e);

	return mock_ptr_type(char *);
}

/* Setup for gpt_get_entry_name mock */
#define WILL_GET_ENTRY_NAME(part, ret) do { \
	expect_value(gpt_get_entry_name, e, part); \
	will_return(gpt_get_entry_name, ret); \
} while (0)

GptEntry *gpt_get_partition(GptData *gpt, unsigned int index)
{
	assert_ptr_equal(gpt, &test_gpt);
	check_expected(index);

	return mock_ptr_type(GptEntry *);
}

/* Setup for gpt_get_partition mock */
#define WILL_GET_PARTITION(idx, ret) do { \
	expect_value(gpt_get_partition, index, idx); \
	will_return(gpt_get_partition, ret); \
} while (0)

int gpt_get_number_of_partitions(GptData *gpt)
{
	assert_ptr_equal(gpt, &test_gpt);

	return mock();
}

/* Setup for gpt_get_number_of_partitions mock */
#define WILL_GET_NUMBER_OF_PARTITIONS(ret) will_return(gpt_get_number_of_partitions, ret)

uint64_t GptGetEntrySizeBytes(const GptData *gpt, const GptEntry *e)
{
	assert_ptr_equal(gpt, &test_gpt);
	check_expected_ptr(e);

	return mock();
}

/* Setup for GptGetEntrySizeBytes mock */
#define WILL_GET_ENTRY_SIZE(entry, ret) do { \
	expect_value(GptGetEntrySizeBytes, e, entry); \
	will_return(GptGetEntrySizeBytes, ret); \
} while (0)

int GptInit(GptData *gpt)
{
	assert_ptr_equal(gpt, &test_gpt);

	return mock();
}

/* Setup for GptInit mock */
#define WILL_GPT_INIT(ret) will_return(GptInit, ret)

GptEntry *GptNextKernelEntry(GptData *gpt)
{
	assert_ptr_equal(gpt, &test_gpt);

	return mock_ptr_type(GptEntry *);
}

/* Setup for GptNextKernelEntry mock */
#define WILL_GET_NEXT_KERNEL_ENTRY(ret) will_return(GptNextKernelEntry, ret)

int fastboot_get_slot_suffixes(GptData *gpt, char *outbuf, size_t outbuf_len)
{
	assert_ptr_equal(gpt, &test_gpt);
	char *suffixes = mock_ptr_type(char *);
	int suffixes_len = strlen(suffixes);

	memcpy(outbuf, suffixes, MIN(suffixes_len + 1, outbuf_len));

	return suffixes_len;
}

/* Setup for fastboot_get_slot_suffixes mock */
#define WILL_GET_SLOT_SUFFIXES(ret) will_return(fastboot_get_slot_suffixes, ret)

int GetEntrySuccessful(const GptEntry *e)
{
	check_expected_ptr(e);

	return mock();
}

/* Setup for GetEntrySuccessful mock */
#define WILL_GET_ENTRY_SUCCESSFUL(entry, ret) do { \
	expect_value(GetEntrySuccessful, e, entry); \
	will_return(GetEntrySuccessful, ret); \
} while (0)

int GetEntryTries(const GptEntry *e)
{
	check_expected_ptr(e);

	return mock();
}

/* Setup for GetEntryTries mock */
#define WILL_GET_ENTRY_TRIES(entry, ret) do { \
	expect_value(GetEntryTries, e, entry); \
	will_return(GetEntryTries, ret); \
} while (0)

const char *get_active_fw_id(void)
{
	return mock_ptr_type(char *);
}

/* Setup for get_active_fw_id mock */
#define WILL_GET_ACTIVE_FW_ID(ret) will_return(get_active_fw_id, ret)

const u8 *vpd_find(const char *key, const u8 *blob, u32 *offset, u32 *size)
{
	check_expected(key);
	assert_ptr_equal(blob, NULL);
	assert_ptr_equal(offset, NULL);

	*size = mock();

	return mock_ptr_type(u8 *);
}

/* Setup for vpd_find mock */
#define WILL_VPD_FIND(k, len, ret) do { \
	expect_string(vpd_find, key, k); \
	will_return(vpd_find, len); \
	will_return(vpd_find, ret); \
} while (0)

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	memset(&lib_sysinfo, 0, sizeof(lib_sysinfo));
	fastboot_disk_init_could_fail = false;

	setup_test_fb();

	*state = &test_fb;

	return 0;
}

#define TEST_FASTBOOT_GETVAR(var, arg, expected, err) do { \
	struct FastbootOps *fb = *state; \
	char var_buf[FASTBOOT_MSG_MAX]; \
	size_t out_len = sizeof(var_buf); \
	assert_int_equal(fastboot_getvar(fb, var, arg, 0, var_buf, out_len), err); \
	if (expected != NULL) \
		assert_string_equal(var_buf, expected); \
} while (0)

#define TEST_FASTBOOT_GETVAR_ERR(var, arg, err) TEST_FASTBOOT_GETVAR(var, arg, NULL, err)

#define TEST_FASTBOOT_GETVAR_OK(var, arg, expected) \
	TEST_FASTBOOT_GETVAR(var, arg, expected, STATE_OK)

static void setup_partition_table(fastboot_var_t var)
{
	const int num_of_parts = 7;
	GptEntry *part;
	const char *part_name;

	/* vbmeta_a 0x100 at 0 */
	part = (void *)0xcafe;
	part_name = "vbmeta_a";
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(0, part);
	WILL_GET_ENTRY_NAME(part, part_name);
	switch (var) {
	case VAR_PARTITION_SIZE:
		WILL_GET_ENTRY_SIZE(part, 0x100);
		break;
	case VAR_SLOT_SUCCESSFUL:
		WILL_CHECK_ANDROID(part, true);
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'a');
		WILL_GET_ENTRY_SUCCESSFUL(part, 1);
		break;
	case VAR_SLOT_RETRY_COUNT:
		WILL_CHECK_ANDROID(part, true);
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'a');
		WILL_GET_ENTRY_TRIES(part, 12);
		break;
	case VAR_SLOT_UNBOOTABLE:
		WILL_CHECK_ANDROID(part, true);
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'a');
		WILL_CHECK_BOOTABLE_ENTRY(part, true);
		WILL_GET_PRIORITY(part, 5);
		WILL_GET_ENTRY_SUCCESSFUL(part, 1);
		break;
	case VAR_HAS_SLOT:
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'a');
		break;
	default:
		break;
	}

	/* No part at 1 */
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(1, NULL);

	/* No part at 2 */
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(2, NULL);

	/* No part at 3 (unused entry) */
	part = (void *)0xcaf3;
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(3, part);
	WILL_GET_ENTRY_NAME(part, NULL);

	/* boot_a 0x300 at 4 */
	part = (void *)0xcaf4;
	part_name = "boot_a";
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(4, part);
	WILL_GET_ENTRY_NAME(part, part_name);
	switch (var) {
	case VAR_PARTITION_SIZE:
		WILL_GET_ENTRY_SIZE(part, 0x300);
		break;
	case VAR_SLOT_SUCCESSFUL:
		WILL_CHECK_ANDROID(part, false);
		break;
	case VAR_SLOT_RETRY_COUNT:
		WILL_CHECK_ANDROID(part, false);
		break;
	case VAR_SLOT_UNBOOTABLE:
		WILL_CHECK_ANDROID(part, false);
		break;
	case VAR_HAS_SLOT:
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'a');
		break;
	default:
		break;
	}

	/* super 0x1000 at 5 */
	part = (void *)0xcaf5;
	part_name = "super";
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(5, part);
	WILL_GET_ENTRY_NAME(part, part_name);
	switch (var) {
	case VAR_PARTITION_SIZE:
		WILL_GET_ENTRY_SIZE(part, 0x1000);
		break;
	case VAR_SLOT_SUCCESSFUL:
		WILL_CHECK_ANDROID(part, false);
		break;
	case VAR_SLOT_RETRY_COUNT:
		WILL_CHECK_ANDROID(part, false);
		break;
	case VAR_SLOT_UNBOOTABLE:
		WILL_CHECK_ANDROID(part, false);
		break;
	case VAR_HAS_SLOT:
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 0);
		break;
	default:
		break;
	}

	/* vbmeta_b 0x100 at 6 */
	part = (void *)0xcaf6;
	part_name = "vbmeta_b";
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(6, part);
	WILL_GET_ENTRY_NAME(part, part_name);
	switch (var) {
	case VAR_PARTITION_SIZE:
		WILL_GET_ENTRY_SIZE(part, 0x100);
		break;
	case VAR_SLOT_SUCCESSFUL:
		WILL_CHECK_ANDROID(part, true);
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'b');
		WILL_GET_ENTRY_SUCCESSFUL(part, 0);
		break;
	case VAR_SLOT_RETRY_COUNT:
		WILL_CHECK_ANDROID(part, true);
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'b');
		WILL_GET_ENTRY_TRIES(part, 8);
		break;
	case VAR_SLOT_UNBOOTABLE:
		WILL_CHECK_ANDROID(part, true);
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'b');
		WILL_CHECK_BOOTABLE_ENTRY(part, true);
		WILL_GET_PRIORITY(part, 0);
		break;
	case VAR_HAS_SLOT:
		WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 'b');
		break;
	default:
		break;
	}

	/* No more partitions */
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
}

/* Common tests for variables iterating over partitions */

static void test_fb_getvar_partition_at_index(void **state, fastboot_var_t var, GptEntry *part,
					      const char *part_name, const char *exp_out)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(3, part);
	WILL_GET_ENTRY_NAME(part, part_name);
	assert_int_equal(fastboot_getvar(fb, var, NULL, 3, var_buf, out_len), STATE_OK);
	assert_string_equal(var_buf, exp_out);
}

static void test_fb_getvar_kernel_slot_at_index(void **state, fastboot_var_t var,
						GptEntry *part, const char *part_name,
						char slot, const char *exp_out)
{
	WILL_CHECK_ANDROID(part, true);
	WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, slot);
	test_fb_getvar_partition_at_index(state, var, part, part_name, exp_out);
}

static void test_fb_getvar_kernel_slot_at_index_no_slot(void **state, fastboot_var_t var)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);
	GptEntry *part = (void *)0xcafe;
	char part_name[] = "part";

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(3, part);
	WILL_GET_ENTRY_NAME(part, part_name);
	WILL_CHECK_ANDROID(part, true);
	WILL_GET_SLOT_FOR_PARTITION_NAME(part_name, 0);
	assert_int_equal(fastboot_getvar(fb, var, NULL, 3, var_buf, out_len), STATE_TRY_NEXT);
}

static void test_fb_getvar_partition_at_index_not_exist(void **state, fastboot_var_t var)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(0, NULL);
	assert_int_equal(fastboot_getvar(fb, var, NULL, 0, var_buf, out_len), STATE_TRY_NEXT);
}

static void test_fb_getvar_partition_at_index_no_name(void **state, fastboot_var_t var)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);
	GptEntry *part = (void *)0xcafe;

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(2, part);
	WILL_GET_ENTRY_NAME(part, NULL);
	assert_int_equal(fastboot_getvar(fb, var, NULL, 2, var_buf, out_len), STATE_TRY_NEXT);
}

static void test_fb_getvar_partition_at_index_last(void **state, fastboot_var_t var)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	assert_int_equal(fastboot_getvar(fb, var, NULL, 5, var_buf, out_len), STATE_LAST);
}

/* Test functions start here */

static void test_fb_getvar_version(void **state)
{
	TEST_FASTBOOT_GETVAR_OK(VAR_VERSION, "", "0.4");
}

static void test_fb_getvar_is_userspace(void **state)
{
	TEST_FASTBOOT_GETVAR_OK(VAR_IS_USERSPACE, "", "no");
}

static void test_fb_getvar_secure(void **state)
{
	TEST_FASTBOOT_GETVAR_OK(VAR_SECURE, "", "no");
}

static void test_fb_getvar_slot_count(void **state)
{
	WILL_GET_SLOT_COUNT(1);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_COUNT, "", "1");

	WILL_GET_SLOT_COUNT(2);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_COUNT, "", "2");
}

static void test_fb_getvar_product(void **state)
{
	const char product[] = "product";
	struct {
		struct cb_mainboard info;
		char strings[sizeof(product)];
	} mainboard;

	lib_sysinfo.cb_mainboard = virt_to_phys(&mainboard);
	mainboard.info.part_number_idx = 0;
	memcpy(mainboard.info.strings, product, sizeof(product));

	TEST_FASTBOOT_GETVAR_OK(VAR_PRODUCT, "", product);
}

static void test_fb_getvar_partition_size(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_FIND_PARTITION("part", part);
	WILL_GET_ENTRY_SIZE(part, 0x7b);
	TEST_FASTBOOT_GETVAR_OK(VAR_PARTITION_SIZE, "part", "0x7b");
}

static void test_fb_getvar_partition_size_no_entry(void **state)
{
	WILL_FIND_PARTITION("part2", NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_PARTITION_SIZE, "part2", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_partition_size_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_ENTRY_SIZE(part, 0x7e);
	test_fb_getvar_partition_at_index(state, VAR_PARTITION_SIZE, part, "part3",
					  "part3:0x7e");
}

static void test_fb_getvar_partition_size_at_index_not_exist(void **state)
{
	test_fb_getvar_partition_at_index_not_exist(state, VAR_PARTITION_SIZE);
}

static void test_fb_getvar_partition_size_at_index_no_name(void **state)
{
	test_fb_getvar_partition_at_index_no_name(state, VAR_PARTITION_SIZE);
}

static void test_fb_getvar_partition_size_at_index_last(void **state)
{
	test_fb_getvar_partition_at_index_last(state, VAR_PARTITION_SIZE);
}

static void test_fb_getvar_partition_type(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_FIND_PARTITION("part", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_PARTITION_TYPE, "part", "raw");

	WILL_FIND_PARTITION("OEM", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_PARTITION_TYPE, "OEM", "ext4");

	WILL_FIND_PARTITION("EFI-SYSTEM", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_PARTITION_TYPE, "EFI-SYSTEM", "vfat");

	WILL_FIND_PARTITION("metadata", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_PARTITION_TYPE, "metadata", "ext4");

	WILL_FIND_PARTITION("userdata", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_PARTITION_TYPE, "userdata", "ext4");
}

static void test_fb_getvar_partition_type_no_entry(void **state)
{
	WILL_FIND_PARTITION("part2", NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_PARTITION_TYPE, "part2", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_partition_type_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	test_fb_getvar_partition_at_index(state, VAR_PARTITION_TYPE, part, "part3",
					  "part3:raw");
	test_fb_getvar_partition_at_index(state, VAR_PARTITION_TYPE, part, "userdata",
					  "userdata:ext4");
}

static void test_fb_getvar_partition_type_at_index_not_exist(void **state)
{
	test_fb_getvar_partition_at_index_not_exist(state, VAR_PARTITION_TYPE);
}

static void test_fb_getvar_partition_type_at_index_no_name(void **state)
{
	test_fb_getvar_partition_at_index_no_name(state, VAR_PARTITION_TYPE);
}

static void test_fb_getvar_partition_type_at_index_last(void **state)
{
	test_fb_getvar_partition_at_index_last(state, VAR_PARTITION_TYPE);
}

static void test_fb_getvar_download_size(void **state)
{
	char expected[32];
	snprintf(expected, sizeof(expected), "0x%llx", FASTBOOT_MAX_DOWNLOAD_SIZE);

	TEST_FASTBOOT_GETVAR_OK(VAR_DOWNLOAD_SIZE, "", expected);
}

static void test_fb_getvar_current_slot(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GPT_INIT(GPT_SUCCESS);
	WILL_GET_NEXT_KERNEL_ENTRY(part);
	WILL_GET_ENTRY_NAME(part, "vbmeta_a");
	TEST_FASTBOOT_GETVAR_OK(VAR_CURRENT_SLOT, "", "a");
}

static void test_fb_getvar_current_slot_fail_gpt_init(void **state)
{
	WILL_GPT_INIT(-1);
	TEST_FASTBOOT_GETVAR_ERR(VAR_CURRENT_SLOT, "", STATE_DISK_ERROR);
}

static void test_fb_getvar_current_slot_no_kernel(void **state)
{
	WILL_GPT_INIT(GPT_SUCCESS);
	WILL_GET_NEXT_KERNEL_ENTRY(NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_CURRENT_SLOT, "", STATE_DISK_ERROR);
}

static void test_fb_getvar_current_slot_no_name(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GPT_INIT(GPT_SUCCESS);
	WILL_GET_NEXT_KERNEL_ENTRY(part);
	WILL_GET_ENTRY_NAME(part, NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_CURRENT_SLOT, "", STATE_DISK_ERROR);
}

static void test_fb_getvar_current_slot_empty_name(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GPT_INIT(GPT_SUCCESS);
	WILL_GET_NEXT_KERNEL_ENTRY(part);
	WILL_GET_ENTRY_NAME(part, "");
	TEST_FASTBOOT_GETVAR_ERR(VAR_CURRENT_SLOT, "", STATE_DISK_ERROR);
}

static void test_fb_getvar_slot_suffixes(void **state)
{
	WILL_GET_SLOT_SUFFIXES("a,b");
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_SUFFIXES, "", "a,b");
}

static void test_fb_getvar_slot_successful(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_GET_ENTRY_SUCCESSFUL(part, 1);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_SUCCESSFUL, "a", "yes");
}

static void test_fb_getvar_slot_unsuccessful(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_GET_ENTRY_SUCCESSFUL(part, 0);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_SUCCESSFUL, "a", "no");
}

static void test_fb_getvar_slot_successful_no_kernel(void **state)
{
	WILL_GET_KERNEL_FOR_SLOT('a', NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_SLOT_SUCCESSFUL, "a", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_slot_successful_bad_slot(void **state)
{
	TEST_FASTBOOT_GETVAR_ERR(VAR_SLOT_SUCCESSFUL, "ab", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_slot_successful_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_ENTRY_SUCCESSFUL(part, 1);
	test_fb_getvar_kernel_slot_at_index(state, VAR_SLOT_SUCCESSFUL, part, "vbmeta_a", 'a',
					    "a:yes");
}

static void test_fb_getvar_slot_unsuccessful_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_ENTRY_SUCCESSFUL(part, 0);
	test_fb_getvar_kernel_slot_at_index(state, VAR_SLOT_SUCCESSFUL, part, "vbmeta_a", 'a',
					    "a:no");
}

static void test_fb_getvar_slot_successful_at_index_no_slot(void **state)
{
	test_fb_getvar_kernel_slot_at_index_no_slot(state, VAR_SLOT_SUCCESSFUL);
}

static void test_fb_getvar_slot_successful_at_index_not_exist(void **state)
{
	test_fb_getvar_partition_at_index_not_exist(state, VAR_SLOT_SUCCESSFUL);
}

static void test_fb_getvar_slot_successful_at_index_no_name(void **state)
{
	test_fb_getvar_partition_at_index_no_name(state, VAR_SLOT_SUCCESSFUL);
}

static void test_fb_getvar_slot_successful_at_index_last(void **state)
{
	test_fb_getvar_partition_at_index_last(state, VAR_SLOT_SUCCESSFUL);
}

static void test_fb_getvar_slot_retry_count(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_GET_ENTRY_TRIES(part, 1);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_RETRY_COUNT, "a", "1");
}

static void test_fb_getvar_slot_retry_count_no_kernel(void **state)
{
	WILL_GET_KERNEL_FOR_SLOT('a', NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_SLOT_RETRY_COUNT, "a", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_slot_retry_count_bad_slot(void **state)
{
	TEST_FASTBOOT_GETVAR_ERR(VAR_SLOT_RETRY_COUNT, "ab", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_slot_retry_count_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_ENTRY_TRIES(part, 5);
	test_fb_getvar_kernel_slot_at_index(state, VAR_SLOT_RETRY_COUNT, part, "vbmeta_a", 'a',
					    "a:5");
}

static void test_fb_getvar_slot_retry_count_at_index_no_slot(void **state)
{
	test_fb_getvar_kernel_slot_at_index_no_slot(state, VAR_SLOT_RETRY_COUNT);
}

static void test_fb_getvar_slot_retry_count_at_index_not_exist(void **state)
{
	test_fb_getvar_partition_at_index_not_exist(state, VAR_SLOT_RETRY_COUNT);
}

static void test_fb_getvar_slot_retry_count_at_index_no_name(void **state)
{
	test_fb_getvar_partition_at_index_no_name(state, VAR_SLOT_RETRY_COUNT);
}

static void test_fb_getvar_slot_retry_count_at_index_last(void **state)
{
	test_fb_getvar_partition_at_index_last(state, VAR_SLOT_RETRY_COUNT);
}

static void test_fb_getvar_logical_block_size(void **state)
{
	test_disk.block_size = 0x200;

	TEST_FASTBOOT_GETVAR_OK(VAR_LOGICAL_BLOCK_SIZE, "", "0x200");
}

static void test_fb_getvar_slot_unbootable(void **state)
{
	GptEntry *part = (void *)0xcafe;

	/* Unbootable, because partition isn't kernel */
	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_CHECK_BOOTABLE_ENTRY(part, false);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_UNBOOTABLE, "a", "yes");

	/* Unbootable, because partition has 0 priority */
	WILL_GET_KERNEL_FOR_SLOT('b', part);
	WILL_CHECK_BOOTABLE_ENTRY(part, true);
	WILL_GET_PRIORITY(part, 0);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_UNBOOTABLE, "b", "yes");

	/* Unbootable, because partition isn't successful and has zero tries */
	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_CHECK_BOOTABLE_ENTRY(part, true);
	WILL_GET_PRIORITY(part, 3);
	WILL_GET_ENTRY_SUCCESSFUL(part, 0);
	WILL_GET_ENTRY_TRIES(part, 0);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_UNBOOTABLE, "a", "yes");
}

static void test_fb_getvar_slot_bootable(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_CHECK_BOOTABLE_ENTRY(part, true);
	WILL_GET_PRIORITY(part, 5);
	WILL_GET_ENTRY_SUCCESSFUL(part, 0);
	WILL_GET_ENTRY_TRIES(part, 3);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_UNBOOTABLE, "a", "no");

	WILL_GET_KERNEL_FOR_SLOT('b', part);
	WILL_CHECK_BOOTABLE_ENTRY(part, true);
	WILL_GET_PRIORITY(part, 5);
	WILL_GET_ENTRY_SUCCESSFUL(part, 1);
	TEST_FASTBOOT_GETVAR_OK(VAR_SLOT_UNBOOTABLE, "b", "no");
}

static void test_fb_getvar_slot_unbootable_no_kernel(void **state)
{
	WILL_GET_KERNEL_FOR_SLOT('a', NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_SLOT_UNBOOTABLE, "a", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_slot_unbootable_bad_slot(void **state)
{
	TEST_FASTBOOT_GETVAR_ERR(VAR_SLOT_UNBOOTABLE, "ab", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_slot_unbootable_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_CHECK_BOOTABLE_ENTRY(part, false);
	test_fb_getvar_kernel_slot_at_index(state, VAR_SLOT_UNBOOTABLE, part, "vbmeta_a", 'a',
					    "a:yes");
}

static void test_fb_getvar_slot_bootable_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_CHECK_BOOTABLE_ENTRY(part, true);
	WILL_GET_PRIORITY(part, 5);
	WILL_GET_ENTRY_SUCCESSFUL(part, 1);
	test_fb_getvar_kernel_slot_at_index(state, VAR_SLOT_UNBOOTABLE, part, "vbmeta_a", 'a',
					    "a:no");
}

static void test_fb_getvar_slot_unbootable_at_index_no_slot(void **state)
{
	test_fb_getvar_kernel_slot_at_index_no_slot(state, VAR_SLOT_UNBOOTABLE);
}

static void test_fb_getvar_slot_unbootable_at_index_not_exist(void **state)
{
	test_fb_getvar_partition_at_index_not_exist(state, VAR_SLOT_UNBOOTABLE);
}

static void test_fb_getvar_slot_unbootable_at_index_no_name(void **state)
{
	test_fb_getvar_partition_at_index_no_name(state, VAR_SLOT_UNBOOTABLE);
}

static void test_fb_getvar_slot_unbootable_at_index_last(void **state)
{
	test_fb_getvar_partition_at_index_last(state, VAR_SLOT_UNBOOTABLE);
}

static void test_fb_getvar_version_bootloader(void **state)
{
	WILL_GET_ACTIVE_FW_ID("FWID");
	TEST_FASTBOOT_GETVAR_OK(VAR_VERSION_BOOTLOADER, "", "FWID");

	WILL_GET_ACTIVE_FW_ID(NULL);
	TEST_FASTBOOT_GETVAR_OK(VAR_VERSION_BOOTLOADER, "", "unknown");
}

static void test_fb_getvar_serialno(void **state)
{
	WILL_VPD_FIND("serial_number", 4, "123456789");
	TEST_FASTBOOT_GETVAR_OK(VAR_SERIALNO, "", "1234");

	WILL_VPD_FIND("serial_number", 0, "123456789");
	TEST_FASTBOOT_GETVAR_OK(VAR_SERIALNO, "", "unknown");

	WILL_VPD_FIND("serial_number", 2, NULL);
	TEST_FASTBOOT_GETVAR_OK(VAR_SERIALNO, "", "unknown");
}

static void test_fb_getvar_mfg_sku(void **state)
{
	WILL_VPD_FIND("mfg_sku_id", 4, "ABCDEFG");
	TEST_FASTBOOT_GETVAR_OK(VAR_MFG_SKU_ID, "", "ABCD");

	WILL_VPD_FIND("mfg_sku_id", 0, "ABCDEFG");
	TEST_FASTBOOT_GETVAR_OK(VAR_MFG_SKU_ID, "", "unknown");

	WILL_VPD_FIND("mfg_sku_id", 2, NULL);
	TEST_FASTBOOT_GETVAR_OK(VAR_MFG_SKU_ID, "", "unknown");
}

static void test_fb_getvar_has_slot(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_SLOT_FOR_PARTITION_NAME("part", 0);
	WILL_FIND_PARTITION("part", NULL);
	WILL_FIND_PARTITION("part_a", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_HAS_SLOT, "part", "yes");
}

static void test_fb_getvar_has_no_slot(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_SLOT_FOR_PARTITION_NAME("super", 0);
	WILL_FIND_PARTITION("super", part);
	TEST_FASTBOOT_GETVAR_OK(VAR_HAS_SLOT, "super", "no");
}

static void test_fb_getvar_has_slot_no_partition(void **state)
{
	WILL_GET_SLOT_FOR_PARTITION_NAME("unk", 0);
	WILL_FIND_PARTITION("unk", NULL);
	WILL_FIND_PARTITION("unk_a", NULL);
	TEST_FASTBOOT_GETVAR_ERR(VAR_HAS_SLOT, "unk", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_has_slot_in_arg(void **state)
{
	WILL_GET_SLOT_FOR_PARTITION_NAME("part_a", 'a');
	TEST_FASTBOOT_GETVAR_ERR(VAR_HAS_SLOT, "part_a", STATE_UNKNOWN_VAR);
}

static void test_fb_getvar_has_slot_a_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_SLOT_FOR_PARTITION_NAME("part_a", 'a');
	/* Print just the base name without a slot */
	test_fb_getvar_partition_at_index(state, VAR_HAS_SLOT, part, "part_a", "part:yes");
}

static void test_fb_getvar_has_slot_b_at_index(void **state)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);
	GptEntry *part = (void *)0xcafe;

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(3, part);
	WILL_GET_ENTRY_NAME(part, "part_b");
	WILL_GET_SLOT_FOR_PARTITION_NAME("part_b", 'b');
	/* Shouldn't print base name for other slots than "a" */
	assert_int_equal(fastboot_getvar(fb, VAR_HAS_SLOT, NULL, 3, var_buf, out_len),
			 STATE_TRY_NEXT);
}

static void test_fb_getvar_has_no_slot_at_index(void **state)
{
	GptEntry *part = (void *)0xcafe;

	WILL_GET_SLOT_FOR_PARTITION_NAME("part", 0);
	test_fb_getvar_partition_at_index(state, VAR_HAS_SLOT, part, "part", "part:no");
}

static void test_fb_getvar_has_slot_at_index_not_exist(void **state)
{
	test_fb_getvar_partition_at_index_not_exist(state, VAR_HAS_SLOT);
}

static void test_fb_getvar_has_slot_at_index_no_name(void **state)
{
	test_fb_getvar_partition_at_index_no_name(state, VAR_HAS_SLOT);
}

static void test_fb_getvar_has_slot_at_index_last(void **state)
{
	test_fb_getvar_partition_at_index_last(state, VAR_HAS_SLOT);
}

static void test_fb_getvar_total_block_count(void **state)
{
	test_disk.block_count = 0x1000;

	TEST_FASTBOOT_GETVAR_OK(VAR_TOTAL_BLOCK_COUNT, "", "0x1000");
}

/* fastboot_cmd_getvar tests */

static void test_fb_cmd_getvar_current_slot(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_GPT_INIT(GPT_SUCCESS);
	WILL_GET_NEXT_KERNEL_ENTRY(part);
	WILL_GET_ENTRY_NAME(part, "vbmeta_b");

	WILL_SEND_EXACT(fb, "OKAYb");

	fastboot_cmd_getvar(fb, "current-slot");
}

static void test_fb_cmd_getvar_download_size(void **state)
{
	struct FastbootOps *fb = *state;
	char expected[32];
	snprintf(expected, sizeof(expected), "OKAY0x%llx", FASTBOOT_MAX_DOWNLOAD_SIZE);

	WILL_SEND_EXACT(fb, expected);

	fastboot_cmd_getvar(fb, "max-download-size");
}

static void test_fb_cmd_getvar_is_userspace(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_SEND_EXACT(fb, "OKAYno");

	fastboot_cmd_getvar(fb, "is-userspace");
}

static void test_fb_cmd_getvar_partition_size(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_FIND_PARTITION("part", part);
	WILL_GET_ENTRY_SIZE(part, 0x400);

	WILL_SEND_EXACT(fb, "OKAY0x400");

	fastboot_cmd_getvar(fb, "partition-size:part");
}

static void test_fb_cmd_getvar_partition_type(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_FIND_PARTITION("part", part);

	WILL_SEND_EXACT(fb, "OKAYraw");

	fastboot_cmd_getvar(fb, "partition-type:part");
}

static void test_fb_cmd_getvar_product(void **state)
{
	struct FastbootOps *fb = *state;
	const char product[] = "kano";
	struct {
		struct cb_mainboard info;
		char strings[sizeof(product)];
	} mainboard;

	lib_sysinfo.cb_mainboard = virt_to_phys(&mainboard);
	mainboard.info.part_number_idx = 0;
	memcpy(mainboard.info.strings, product, sizeof(product));

	WILL_SEND_EXACT(fb, "OKAYkano");

	fastboot_cmd_getvar(fb, "product");
}

static void test_fb_cmd_getvar_secure(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_SEND_EXACT(fb, "OKAYno");

	fastboot_cmd_getvar(fb, "secure");
}

static void test_fb_cmd_getvar_slot_count(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_GET_SLOT_COUNT(2);

	WILL_SEND_EXACT(fb, "OKAY2");

	fastboot_cmd_getvar(fb, "slot-count");
}

static void test_fb_cmd_getvar_version(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_SEND_EXACT(fb, "OKAY0.4");

	fastboot_cmd_getvar(fb, "version");
}

static void test_fb_cmd_getvar_slot_suffixes(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_GET_SLOT_SUFFIXES("a,b");

	WILL_SEND_EXACT(fb, "OKAYa,b");

	fastboot_cmd_getvar(fb, "slot-suffixes");
}

static void test_fb_cmd_getvar_slot_successful(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_GET_ENTRY_SUCCESSFUL(part, 1);

	WILL_SEND_EXACT(fb, "OKAYyes");

	fastboot_cmd_getvar(fb, "slot-successful:a");
}

static void test_fb_cmd_getvar_slot_retry_count(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_GET_ENTRY_TRIES(part, 10);

	WILL_SEND_EXACT(fb, "OKAY10");

	fastboot_cmd_getvar(fb, "slot-retry-count:a");
}

static void test_fb_cmd_getvar_logical_block_size(void **state)
{
	struct FastbootOps *fb = *state;

	test_disk.block_size = 0x400;

	WILL_SEND_EXACT(fb, "OKAY0x400");

	fastboot_cmd_getvar(fb, "logical-block-size");
}

static void test_fb_cmd_getvar_slot_unbootable(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_GET_KERNEL_FOR_SLOT('a', part);
	WILL_CHECK_BOOTABLE_ENTRY(part, false);

	WILL_SEND_EXACT(fb, "OKAYyes");

	fastboot_cmd_getvar(fb, "slot-unbootable:a");
}

static void test_fb_cmd_getvar_version_bootloader(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_GET_ACTIVE_FW_ID("fwversion");

	WILL_SEND_EXACT(fb, "OKAYfwversion");

	fastboot_cmd_getvar(fb, "version-bootloader");
}

static void test_fb_cmd_getvar_serialno(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_VPD_FIND("serial_number", 4, "123456789");

	WILL_SEND_EXACT(fb, "OKAY1234");

	fastboot_cmd_getvar(fb, "serialno");
}

static void test_fb_cmd_getvar_has_slot(void **state)
{
	struct FastbootOps *fb = *state;
	GptEntry *part = (void *)0xcafe;

	WILL_GET_SLOT_FOR_PARTITION_NAME("part", 0);
	WILL_FIND_PARTITION("part", NULL);
	WILL_FIND_PARTITION("part_a", part);

	WILL_SEND_EXACT(fb, "OKAYyes");

	fastboot_cmd_getvar(fb, "has-slot:part");
}

static void test_fb_cmd_getvar_total_block_count(void **state)
{
	struct FastbootOps *fb = *state;

	test_disk.block_count = 0x1000;

	WILL_SEND_EXACT(fb, "OKAY0x1000");

	fastboot_cmd_getvar(fb, "Total-block-count");
}

/* fastboot_cmd_getvar fail tests */
static void test_fb_cmd_getvar_get_fail(void **state)
{
	struct FastbootOps *fb = *state;

	/* Intentionally fail fastboot_getvar */
	WILL_GPT_INIT(-1);
	WILL_SEND_PREFIX(fb, "FAIL");

	fastboot_cmd_getvar(fb, "current-slot");
}

static void test_fb_cmd_getvar_no_args(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_SEND_EXACT(fb, "FAILUnknown variable");

	/* Get var that should have argument */
	fastboot_cmd_getvar(fb, "partition-size");
}

static void test_fb_cmd_getvar_prefix_of_var_name(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_SEND_EXACT(fb, "FAILUnknown variable");

	/* Try to get var that doesn't exist and is prefix of partition-size */
	fastboot_cmd_getvar(fb, "part");
}

static void test_fb_cmd_getvar_undefined(void **state)
{
	struct FastbootOps *fb = *state;

	WILL_SEND_EXACT(fb, "FAILUnknown variable");

	fastboot_cmd_getvar(fb, "this-var-doesn't-exist");
}

/* fastboot_cmd_getvar all tests */

/* Save packets from "getvar all" to check them later, because order of them isn't important */
struct fb_test_packet {
	char *msg;
	struct list_node list_node;
};

static struct list_node packets_list;

static int fb_mock_send_packet_getvar_all(struct FastbootOps *fb_ptr, void *buf, size_t len)
{
	struct fb_test_packet *packet = xmalloc(sizeof(*packet));

	check_expected_ptr(fb_ptr);

	packet->msg = strndup(buf, len);
	if (packet->msg == NULL)
		fail_msg("Failed to copy packet");

	/* Put packets in reverse order */
	list_insert_after(&packet->list_node, &packets_list);

	return 0;
}

static void check_fb_cmd_getvar_all_contains(const char *msg)
{
	struct fb_test_packet *node;

	list_for_each(node, packets_list, list_node) {
		if (strcmp(node->msg, msg))
			continue;
		list_remove(&node->list_node);
		free(node->msg);
		free(node);
		return;
	}

	fail_msg("\"%s\" wasn't send", msg);
}

static void test_fb_cmd_getvar_all(void **state)
{
	struct fb_test_packet *node;
	struct FastbootOps *fb = *state;
	char expected_max_download_size[64];
	GptEntry *part;

	memset(&packets_list, 0, sizeof(packets_list));
	/* Always should get the same FB pointer */
	expect_value_count(fb_mock_send_packet_getvar_all, fb_ptr, fb, -1);
	fb->send_packet = fb_mock_send_packet_getvar_all;

	/* Setup for current-slot */
	part = (void *)0xcafe;
	WILL_GPT_INIT(GPT_SUCCESS);
	WILL_GET_NEXT_KERNEL_ENTRY(part);
	WILL_GET_ENTRY_NAME(part, "vbmeta_a");

	/* Expected response for max-download-size */
	snprintf(expected_max_download_size, sizeof(expected_max_download_size),
		 "INFOmax-download-size:0x%llx", FASTBOOT_MAX_DOWNLOAD_SIZE);


	/* Setup for slot-successful */
	setup_partition_table(VAR_SLOT_SUCCESSFUL);

	/* Setup for slot-retry-count */
	setup_partition_table(VAR_SLOT_RETRY_COUNT);

	/* Setup for partition-size */
	setup_partition_table(VAR_PARTITION_SIZE);

	/* Setup for partition-type */
	setup_partition_table(VAR_PARTITION_TYPE);

	/* Setup for slot-unbootable */
	setup_partition_table(VAR_SLOT_UNBOOTABLE);

	/* Setup for has-slot */
	setup_partition_table(VAR_HAS_SLOT);

	/* Setup for product */
	const char product[] = "kano";
	struct {
		struct cb_mainboard info;
		char strings[sizeof(product)];
	} mainboard;

	lib_sysinfo.cb_mainboard = virt_to_phys(&mainboard);
	mainboard.info.part_number_idx = 0;
	memcpy(mainboard.info.strings, product, sizeof(product));

	/* Setup for slot-count */
	WILL_GET_SLOT_COUNT(1);

	/* Setup for slot-suffixes */
	WILL_GET_SLOT_SUFFIXES("a,b");

	/* Setup for logical-block-size */
	test_disk.block_size = 0x1000;

	/* Setup for version-bootloader */
	WILL_GET_ACTIVE_FW_ID("fwversion");

	/* Setup for serialno */
	WILL_VPD_FIND("serial_number", 4, "123456789");

	/* Setup for mfg_sku_id */
	WILL_VPD_FIND("mfg_sku_id", 4, "ABCDEFG");

	/* Setup for Total-block-count */
	test_disk.block_count = 0x5000;

	fastboot_cmd_getvar(fb, "all");

	if (packets_list.next == NULL)
		fail_msg("\"OKAY\" isn't last message");

	list_for_each(node, packets_list, list_node) {
		if (!strcmp(node->msg, "OKAY"))
			break;
		fail_msg("\"OKAY\" isn't last message");
	}

	/* Call this to remove last message from the list */
	check_fb_cmd_getvar_all_contains("OKAY");

	check_fb_cmd_getvar_all_contains("INFOcurrent-slot:a");
	check_fb_cmd_getvar_all_contains(expected_max_download_size);
	check_fb_cmd_getvar_all_contains("INFOis-userspace:no");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:vbmeta_a:0x100");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:boot_a:0x300");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:super:0x1000");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:vbmeta_b:0x100");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:vbmeta_a:raw");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:boot_a:raw");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:super:raw");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:vbmeta_b:raw");
	check_fb_cmd_getvar_all_contains("INFOslot-successful:a:yes");
	check_fb_cmd_getvar_all_contains("INFOslot-successful:b:no");
	check_fb_cmd_getvar_all_contains("INFOslot-retry-count:a:12");
	check_fb_cmd_getvar_all_contains("INFOslot-retry-count:b:8");
	check_fb_cmd_getvar_all_contains("INFOslot-unbootable:a:no");
	check_fb_cmd_getvar_all_contains("INFOslot-unbootable:b:yes");
	check_fb_cmd_getvar_all_contains("INFOhas-slot:vbmeta:yes");
	check_fb_cmd_getvar_all_contains("INFOhas-slot:boot:yes");
	check_fb_cmd_getvar_all_contains("INFOhas-slot:super:no");
	check_fb_cmd_getvar_all_contains("INFOproduct:kano");
	check_fb_cmd_getvar_all_contains("INFOsecure:no");
	check_fb_cmd_getvar_all_contains("INFOslot-count:1");
	check_fb_cmd_getvar_all_contains("INFOversion:0.4");
	check_fb_cmd_getvar_all_contains("INFOslot-suffixes:a,b");
	check_fb_cmd_getvar_all_contains("INFOlogical-block-size:0x1000");
	check_fb_cmd_getvar_all_contains("INFOversion-bootloader:fwversion");
	check_fb_cmd_getvar_all_contains("INFOserialno:1234");
	check_fb_cmd_getvar_all_contains("INFOTotal-block-count:0x5000");
	check_fb_cmd_getvar_all_contains("INFOmfg-sku:ABCD");

	list_for_each(node, packets_list, list_node) {
		fail_msg("Unexpected message: \"%s\"", node->msg);
	}
}

/* Failure to get any var shouldn't prevent printing rest of them */
static void test_fb_cmd_getvar_all_fail_get_var(void **state)
{
	struct fb_test_packet *node;
	struct FastbootOps *fb = *state;
	char expected_max_download_size[64];

	memset(&packets_list, 0, sizeof(packets_list));
	/* Always should get the same FB pointer */
	expect_value_count(fb_mock_send_packet_getvar_all, fb_ptr, fb, -1);
	fb->send_packet = fb_mock_send_packet_getvar_all;

	/* Expected response for max-download-size */
	snprintf(expected_max_download_size, sizeof(expected_max_download_size),
		 "INFOmax-download-size:0x%llx", FASTBOOT_MAX_DOWNLOAD_SIZE);

	/* Setup for current-slot - will fail */
	WILL_GPT_INIT(-1);

	/* Setup for partition-size */
	setup_partition_table(VAR_PARTITION_SIZE);

	/* Setup for partition-type */
	setup_partition_table(VAR_PARTITION_TYPE);

	/* Setup for slot-successful */
	setup_partition_table(VAR_SLOT_SUCCESSFUL);

	/* Setup for slot-retry-count */
	setup_partition_table(VAR_SLOT_RETRY_COUNT);

	/* Setup for slot-unbootable */
	setup_partition_table(VAR_SLOT_UNBOOTABLE);

	/* Setup for has-slot */
	setup_partition_table(VAR_HAS_SLOT);

	/* Setup for product */
	const char product[] = "kano";
	struct {
		struct cb_mainboard info;
		char strings[sizeof(product)];
	} mainboard;

	lib_sysinfo.cb_mainboard = virt_to_phys(&mainboard);
	mainboard.info.part_number_idx = 0;
	memcpy(mainboard.info.strings, product, sizeof(product));

	/* Setup for slot-count */
	WILL_GET_SLOT_COUNT(1);

	/* Setup for slot-suffixes */
	WILL_GET_SLOT_SUFFIXES("a,b");

	/* Setup for logical-block-size */
	test_disk.block_size = 0x1000;

	/* Setup for version-bootloader */
	WILL_GET_ACTIVE_FW_ID("fwversion");

	/* Setup for serialno */
	WILL_VPD_FIND("serial_number", 4, "123456789");

	/* Setup for mfg_sku_id */
	WILL_VPD_FIND("mfg_sku_id", 4, "ABCDEFG");

	/* Setup for Total-block-count */
	test_disk.block_count = 0x5000;

	fastboot_cmd_getvar(fb, "all");

	if (packets_list.next == NULL)
		fail_msg("\"OKAY\" isn't last message");

	list_for_each(node, packets_list, list_node) {
		if (!strcmp(node->msg, "OKAY"))
			break;
		fail_msg("\"OKAY\" isn't last message");
	}

	/* Call this to remove last message from the list */
	check_fb_cmd_getvar_all_contains("OKAY");

	check_fb_cmd_getvar_all_contains(expected_max_download_size);
	check_fb_cmd_getvar_all_contains("INFOis-userspace:no");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:vbmeta_a:0x100");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:boot_a:0x300");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:super:0x1000");
	check_fb_cmd_getvar_all_contains("INFOpartition-size:vbmeta_b:0x100");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:vbmeta_a:raw");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:boot_a:raw");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:super:raw");
	check_fb_cmd_getvar_all_contains("INFOpartition-type:vbmeta_b:raw");
	check_fb_cmd_getvar_all_contains("INFOslot-successful:a:yes");
	check_fb_cmd_getvar_all_contains("INFOslot-successful:b:no");
	check_fb_cmd_getvar_all_contains("INFOslot-retry-count:a:12");
	check_fb_cmd_getvar_all_contains("INFOslot-retry-count:b:8");
	check_fb_cmd_getvar_all_contains("INFOslot-unbootable:a:no");
	check_fb_cmd_getvar_all_contains("INFOslot-unbootable:b:yes");
	check_fb_cmd_getvar_all_contains("INFOhas-slot:vbmeta:yes");
	check_fb_cmd_getvar_all_contains("INFOhas-slot:boot:yes");
	check_fb_cmd_getvar_all_contains("INFOhas-slot:super:no");
	check_fb_cmd_getvar_all_contains("INFOproduct:kano");
	check_fb_cmd_getvar_all_contains("INFOsecure:no");
	check_fb_cmd_getvar_all_contains("INFOslot-count:1");
	check_fb_cmd_getvar_all_contains("INFOversion:0.4");
	check_fb_cmd_getvar_all_contains("INFOslot-suffixes:a,b");
	check_fb_cmd_getvar_all_contains("INFOlogical-block-size:0x1000");
	check_fb_cmd_getvar_all_contains("INFOversion-bootloader:fwversion");
	check_fb_cmd_getvar_all_contains("INFOserialno:1234");
	check_fb_cmd_getvar_all_contains("INFOTotal-block-count:0x5000");
	check_fb_cmd_getvar_all_contains("INFOmfg-sku:ABCD");

	list_for_each(node, packets_list, list_node) {
		fail_msg("Unexpected message: \"%s\"", node->msg);
	}
}
#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_fb_getvar_version),
		TEST(test_fb_getvar_is_userspace),
		TEST(test_fb_getvar_secure),
		TEST(test_fb_getvar_slot_count),
		TEST(test_fb_getvar_product),
		TEST(test_fb_getvar_partition_size),
		TEST(test_fb_getvar_partition_size_no_entry),
		TEST(test_fb_getvar_partition_size_at_index),
		TEST(test_fb_getvar_partition_size_at_index_not_exist),
		TEST(test_fb_getvar_partition_size_at_index_no_name),
		TEST(test_fb_getvar_partition_size_at_index_last),
		TEST(test_fb_getvar_partition_type),
		TEST(test_fb_getvar_partition_type_no_entry),
		TEST(test_fb_getvar_partition_type_at_index),
		TEST(test_fb_getvar_partition_type_at_index_not_exist),
		TEST(test_fb_getvar_partition_type_at_index_no_name),
		TEST(test_fb_getvar_partition_type_at_index_last),
		TEST(test_fb_getvar_download_size),
		TEST(test_fb_getvar_current_slot),
		TEST(test_fb_getvar_current_slot_fail_gpt_init),
		TEST(test_fb_getvar_current_slot_no_kernel),
		TEST(test_fb_getvar_current_slot_no_name),
		TEST(test_fb_getvar_current_slot_empty_name),
		TEST(test_fb_getvar_slot_suffixes),
		TEST(test_fb_getvar_slot_successful),
		TEST(test_fb_getvar_slot_unsuccessful),
		TEST(test_fb_getvar_slot_successful_no_kernel),
		TEST(test_fb_getvar_slot_successful_bad_slot),
		TEST(test_fb_getvar_slot_successful_at_index),
		TEST(test_fb_getvar_slot_unsuccessful_at_index),
		TEST(test_fb_getvar_slot_successful_at_index_no_slot),
		TEST(test_fb_getvar_slot_successful_at_index_not_exist),
		TEST(test_fb_getvar_slot_successful_at_index_no_name),
		TEST(test_fb_getvar_slot_successful_at_index_last),
		TEST(test_fb_getvar_slot_retry_count),
		TEST(test_fb_getvar_slot_retry_count_no_kernel),
		TEST(test_fb_getvar_slot_retry_count_bad_slot),
		TEST(test_fb_getvar_slot_retry_count_at_index),
		TEST(test_fb_getvar_slot_retry_count_at_index_no_slot),
		TEST(test_fb_getvar_slot_retry_count_at_index_not_exist),
		TEST(test_fb_getvar_slot_retry_count_at_index_no_name),
		TEST(test_fb_getvar_slot_retry_count_at_index_last),
		TEST(test_fb_getvar_logical_block_size),
		TEST(test_fb_getvar_slot_unbootable),
		TEST(test_fb_getvar_slot_bootable),
		TEST(test_fb_getvar_slot_unbootable_no_kernel),
		TEST(test_fb_getvar_slot_unbootable_bad_slot),
		TEST(test_fb_getvar_slot_unbootable_at_index),
		TEST(test_fb_getvar_slot_bootable_at_index),
		TEST(test_fb_getvar_slot_unbootable_at_index_no_slot),
		TEST(test_fb_getvar_slot_unbootable_at_index_not_exist),
		TEST(test_fb_getvar_slot_unbootable_at_index_no_name),
		TEST(test_fb_getvar_slot_unbootable_at_index_last),
		TEST(test_fb_getvar_version_bootloader),
		TEST(test_fb_getvar_serialno),
		TEST(test_fb_getvar_mfg_sku),
		TEST(test_fb_getvar_has_slot),
		TEST(test_fb_getvar_has_no_slot),
		TEST(test_fb_getvar_has_slot_no_partition),
		TEST(test_fb_getvar_has_slot_in_arg),
		TEST(test_fb_getvar_has_slot_a_at_index),
		TEST(test_fb_getvar_has_slot_b_at_index),
		TEST(test_fb_getvar_has_no_slot_at_index),
		TEST(test_fb_getvar_has_slot_at_index_not_exist),
		TEST(test_fb_getvar_has_slot_at_index_no_name),
		TEST(test_fb_getvar_has_slot_at_index_last),
		TEST(test_fb_getvar_total_block_count),
		TEST(test_fb_cmd_getvar_current_slot),
		TEST(test_fb_cmd_getvar_download_size),
		TEST(test_fb_cmd_getvar_is_userspace),
		TEST(test_fb_cmd_getvar_partition_size),
		TEST(test_fb_cmd_getvar_partition_type),
		TEST(test_fb_cmd_getvar_product),
		TEST(test_fb_cmd_getvar_secure),
		TEST(test_fb_cmd_getvar_slot_count),
		TEST(test_fb_cmd_getvar_version),
		TEST(test_fb_cmd_getvar_slot_suffixes),
		TEST(test_fb_cmd_getvar_slot_successful),
		TEST(test_fb_cmd_getvar_slot_retry_count),
		TEST(test_fb_cmd_getvar_logical_block_size),
		TEST(test_fb_cmd_getvar_slot_unbootable),
		TEST(test_fb_cmd_getvar_version_bootloader),
		TEST(test_fb_cmd_getvar_serialno),
		TEST(test_fb_cmd_getvar_has_slot),
		TEST(test_fb_cmd_getvar_total_block_count),
		TEST(test_fb_cmd_getvar_get_fail),
		TEST(test_fb_cmd_getvar_no_args),
		TEST(test_fb_cmd_getvar_prefix_of_var_name),
		TEST(test_fb_cmd_getvar_undefined),
		TEST(test_fb_cmd_getvar_all),
		TEST(test_fb_cmd_getvar_all_fail_get_var),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
