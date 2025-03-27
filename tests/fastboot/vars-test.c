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

GptEntry *gpt_find_partition(GptData *gpt, const char *partition_name)
{
	assert_ptr_equal(gpt, &test_gpt);
	check_expected(partition_name);

	return mock_ptr_type(GptEntry *);
}

/* Setup for gpt_find_partition mock */
#define WILL_FIND_PARTITION(name, ret) do { \
	expect_string(gpt_find_partition, partition_name, name); \
	will_return(gpt_find_partition, ret); \
} while (0)

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

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	memset(&lib_sysinfo, 0, sizeof(lib_sysinfo));

	setup_test_fb();

	*state = &test_fb;

	return 0;
}

#define TEST_FASTBOOT_GETVAR(var, arg, expected, err) do { \
	struct FastbootOps *fb = *state; \
	char var_buf[FASTBOOT_MSG_MAX]; \
	size_t out_len = sizeof(var_buf); \
	assert_int_equal(fastboot_getvar(fb, var, arg, 0, var_buf, &out_len), err); \
	if (expected != NULL) { \
		assert_string_equal(var_buf, expected); \
		assert_int_equal(out_len, strlen(expected)); \
	} \
} while (0)

#define TEST_FASTBOOT_GETVAR_ERR(var, arg, err) TEST_FASTBOOT_GETVAR(var, arg, NULL, err)

#define TEST_FASTBOOT_GETVAR_OK(var, arg, expected) \
	TEST_FASTBOOT_GETVAR(var, arg, expected, STATE_OK)

static void setup_partition_table(void)
{
	const int num_of_parts = 6;
	GptEntry *part;

	/* vbmeta_a 0x100 at 0 */
	part = (void *)0xcafe;
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(0, part);
	WILL_GET_ENTRY_NAME(part, "vbmeta_a");
	WILL_GET_ENTRY_SIZE(part, 0x100);

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

	/* boot_a 0x300 at 4*/
	part = (void *)0xcaf4;
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(4, part);
	WILL_GET_ENTRY_NAME(part, "boot_a");
	WILL_GET_ENTRY_SIZE(part, 0x300);

	/* super 0x1000 at 5*/
	part = (void *)0xcaf5;
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
	WILL_GET_PARTITION(5, part);
	WILL_GET_ENTRY_NAME(part, "super");
	WILL_GET_ENTRY_SIZE(part, 0x1000);

	/* No more partitions */
	WILL_GET_NUMBER_OF_PARTITIONS(num_of_parts);
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

static void test_fb_getvar_partition_at_index(void **state)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);
	GptEntry *part = (void *)0xcafe;

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(3, part);
	WILL_GET_ENTRY_NAME(part, "part3");
	WILL_GET_ENTRY_SIZE(part, 0x7e);
	assert_int_equal(fastboot_getvar(fb, VAR_PARTITION_SIZE, NULL, 3, var_buf, &out_len),
			 STATE_OK);
	assert_string_equal(var_buf, "part3:0x7e");
	assert_int_equal(out_len, 10);
}

static void test_fb_getvar_partition_at_index_not_exist(void **state)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(0, NULL);
	assert_int_equal(fastboot_getvar(fb, VAR_PARTITION_SIZE, NULL, 0, var_buf, &out_len),
			 STATE_TRY_NEXT);
}

static void test_fb_getvar_partition_at_index_no_name(void **state)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);
	GptEntry *part = (void *)0xcafe;

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	WILL_GET_PARTITION(2, part);
	WILL_GET_ENTRY_NAME(part, NULL);
	assert_int_equal(fastboot_getvar(fb, VAR_PARTITION_SIZE, NULL, 2, var_buf, &out_len),
			 STATE_TRY_NEXT);
}

static void test_fb_getvar_partition_at_index_last(void **state)
{
	struct FastbootOps *fb = *state;
	char var_buf[FASTBOOT_MSG_MAX];
	size_t out_len = sizeof(var_buf);

	WILL_GET_NUMBER_OF_PARTITIONS(5);
	assert_int_equal(fastboot_getvar(fb, VAR_PARTITION_SIZE, NULL, 5, var_buf, &out_len),
			 STATE_LAST);
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

	/* Setup for partition-size */
	setup_partition_table();

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
	check_fb_cmd_getvar_all_contains("INFOproduct:kano");
	check_fb_cmd_getvar_all_contains("INFOsecure:no");
	check_fb_cmd_getvar_all_contains("INFOslot-count:1");
	check_fb_cmd_getvar_all_contains("INFOversion:0.4");
	check_fb_cmd_getvar_all_contains("INFOslot-suffixes:a,b");

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
	setup_partition_table();

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
	check_fb_cmd_getvar_all_contains("INFOproduct:kano");
	check_fb_cmd_getvar_all_contains("INFOsecure:no");
	check_fb_cmd_getvar_all_contains("INFOslot-count:1");
	check_fb_cmd_getvar_all_contains("INFOversion:0.4");
	check_fb_cmd_getvar_all_contains("INFOslot-suffixes:a,b");

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
		TEST(test_fb_getvar_partition_at_index),
		TEST(test_fb_getvar_partition_at_index_not_exist),
		TEST(test_fb_getvar_partition_at_index_no_name),
		TEST(test_fb_getvar_partition_at_index_last),
		TEST(test_fb_getvar_download_size),
		TEST(test_fb_getvar_current_slot),
		TEST(test_fb_getvar_current_slot_fail_gpt_init),
		TEST(test_fb_getvar_current_slot_no_kernel),
		TEST(test_fb_getvar_current_slot_no_name),
		TEST(test_fb_getvar_current_slot_empty_name),
		TEST(test_fb_getvar_slot_suffixes),
		TEST(test_fb_cmd_getvar_current_slot),
		TEST(test_fb_cmd_getvar_download_size),
		TEST(test_fb_cmd_getvar_is_userspace),
		TEST(test_fb_cmd_getvar_partition_size),
		TEST(test_fb_cmd_getvar_product),
		TEST(test_fb_cmd_getvar_secure),
		TEST(test_fb_cmd_getvar_slot_count),
		TEST(test_fb_cmd_getvar_version),
		TEST(test_fb_cmd_getvar_slot_suffixes),
		TEST(test_fb_cmd_getvar_get_fail),
		TEST(test_fb_cmd_getvar_no_args),
		TEST(test_fb_cmd_getvar_prefix_of_var_name),
		TEST(test_fb_cmd_getvar_undefined),
		TEST(test_fb_cmd_getvar_all),
		TEST(test_fb_cmd_getvar_all_fail_get_var),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
