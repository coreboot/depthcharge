// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "helpers/device_tree.h"
#include "tests/test.h"

#include "boot/memchipinfo.c"

static struct device_tree tree;
static struct device_tree_node root;
static struct device_tree_fixup fixup;

static int setup(void **state)
{
	memset(&tree, 0, sizeof(tree));
	memset(&root, 0, sizeof(root));
	memset(&fixup, 0, sizeof(root));
	tree.root = &root;
	return 0;
}

static struct {
	struct mem_chip_info info;
	struct mem_chip_entry entries[3];
} test_info = {
	.info = {
		.num_entries = 3,
		.struct_version = MEM_CHIP_STRUCT_VERSION,
	},
	.entries = {
		{
			.channel = 0,
			.rank = 0,
			.type = MEM_CHIP_LPDDR3,
			.channel_io_width = 32,
			.density_mbits = 2048,
			.io_width = 16,
			.manufacturer_id = 0x12,
			.revision_id = { 0x34, 0x56 },
		},
		{
			.channel = 0,
			.rank = 1,
			.type = MEM_CHIP_LPDDR3,
			.channel_io_width = 32,
			.density_mbits = 1024,
			.io_width = 16,
			.manufacturer_id = 0x12,
			.revision_id = { 0x34, 0x56 },
		},
		{
			.channel = 1,
			.rank = 0,
			.type = MEM_CHIP_LPDDR4,
			.channel_io_width = 16,
			.density_mbits = 4096,
			.io_width = 16,
			.manufacturer_id = 0x78,
			.revision_id = { 0x9a, 0xbc },
		},
	},
};
struct sysinfo_t lib_sysinfo = { .mem_chip_base = (uintptr_t)&test_info };

static void test_memchipinfo(void **state)
{
	u32 adc, szc;

	assert_int_equal(install_memchipinfo_data(&fixup, &tree), 0);

	adc = szc = 0xff;
	struct device_tree_node *c0 = dt_find_node_by_path(
		&tree, "/lpddr-channel0", &adc, &szc, 0);
	assert_non_null(c0);
	assert_int_equal(adc, 1);
	assert_int_equal(szc, 0);
	assert_dt_compat_strings(c0, "jedec,lpddr3-channel");
	assert_dt_u32_prop(c0, "io-width", 32);

	adc = szc = 0xff;
	struct device_tree_node *r0 = dt_find_node_by_path(
		&tree, "/lpddr-channel0/rank@0", &adc, &szc, 0);
	assert_non_null(r0);
	assert_int_equal(adc, 1);
	assert_int_equal(szc, 0);
	assert_dt_compat_strings(r0, "lpddr3-12,3456", "jedec,lpddr3");
	assert_dt_u32_prop(r0, "reg", 0);
	assert_dt_u32_prop(r0, "io-width", 16);
	assert_dt_u32_prop(r0, "density", 2048);

	adc = szc = 0xff;
	struct device_tree_node *r1 = dt_find_node_by_path(
		&tree, "/lpddr-channel0/rank@1", &adc, &szc, 0);
	assert_non_null(r1);
	assert_int_equal(adc, 1);
	assert_int_equal(szc, 0);
	assert_dt_compat_strings(r1, "lpddr3-12,3456", "jedec,lpddr3");
	assert_dt_u32_prop(r1, "reg", 1);
	assert_dt_u32_prop(r1, "io-width", 16);
	assert_dt_u32_prop(r1, "density", 1024);

	adc = szc = 0xff;
	struct device_tree_node *c1 = dt_find_node_by_path(
		&tree, "/lpddr-channel1", &adc, &szc, 0);
	assert_non_null(c1);
	assert_int_equal(adc, 1);
	assert_int_equal(szc, 0);
	assert_dt_compat_strings(c1, "jedec,lpddr4-channel");
	assert_dt_u32_prop(c1, "io-width", 16);

	adc = szc = 0xff;
	r0 = dt_find_node_by_path(
		&tree, "/lpddr-channel1/rank@0", &adc, &szc, 0);
	assert_non_null(r0);
	assert_int_equal(adc, 1);
	assert_int_equal(szc, 0);
	assert_dt_compat_strings(r0, "lpddr4-78,9abc", "jedec,lpddr4");
	assert_dt_u32_prop(r0, "reg", 0);
	assert_dt_u32_prop(r0, "io-width", 16);
	assert_dt_u32_prop(r0, "density", 4096);
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_memchipinfo),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
