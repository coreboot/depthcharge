// SPDX-License-Identifier: GPL-2.0

#include <cbfs.h>
#include <libpayload.h>
#include <lzma.h>

#include "base/cleanup_funcs.h"
#include "boot/payload.h"
#include "tests/test.h"
#include "vboot/crossystem/crossystem.h"

static void *altfw_list;
static void *altfw_testpayload;
static size_t altfw_testpayload_size;

int crossystem_setup(int firmware_type)
{
	assert_int_equal(firmware_type, FIRMWARE_TYPE_LEGACY);
	return 0;
}

void run_cleanup_funcs(CleanupType type)
{
	assert_int_equal(type, CleanupOnLegacy);
}

void selfboot(void *entry)
{
	check_expected(entry);
}

unsigned long ulzman(const unsigned char *src, unsigned long srcn,
		     unsigned char *dst, unsigned long dstn)
{
	check_expected_ptr(src);
	check_expected(srcn);
	check_expected_ptr(dst);
	check_expected(dstn);
	check_expected(src[0]);

	memset(dst, 'L', dstn);

	return mock();
}

void *_cbfs_unverified_area_load(const char *area, const char *name, void *buf,
				 size_t *size_inout)
{
	assert_string_equal(area, "RW_LEGACY");
	assert_null(buf);
	assert_non_null(size_inout);

	if (!strcmp(name, "altfw/list")) {
		assert_non_null(altfw_list);
		*size_inout = strlen(altfw_list);
		return altfw_list;
	}
	if (!strcmp(name, "altfw/testpayload")) {
		assert_non_null(altfw_testpayload);
		*size_inout = altfw_testpayload_size;
		return altfw_testpayload;
	}
	return mock_ptr_type(void *);
}

static int setup(void **state)
{
	altfw_list = NULL;
	altfw_testpayload = NULL;
	altfw_testpayload_size = 0;
	return 0;
}

#define SEG1_SIZE 0x200
#define SEG2_COMP_SIZE 0x400

static void test_run_payload(void **state)
{
	const size_t seg1_size = SEG1_SIZE;
	const size_t seg2_comp_size = SEG2_COMP_SIZE;
	const size_t seg2_decomp_size = 0x800;
	const size_t bss_size = 0x600;
	const size_t zero_extend_size = 0x100;
	const size_t seg1_dst_size = seg1_size + zero_extend_size;
	const size_t seg2_dst_size = seg2_decomp_size + zero_extend_size;

	void *seg1_dst = test_malloc(seg1_dst_size);
	void *seg2_dst = test_malloc(seg2_dst_size);
	void *bss = test_malloc(bss_size);
	void *entry_point = seg2_dst + 0x123;
	assert_true(seg1_dst && seg2_dst && bss);

	memset(seg1_dst, 'X', seg1_dst_size);
	memset(seg2_dst, 'X', seg2_dst_size);
	memset(bss, 'X', bss_size);

	struct source_buffer {
		struct cbfs_payload_segment segs[4];
		uint8_t seg1[SEG1_SIZE];
		uint8_t seg2[SEG2_COMP_SIZE];
	} src = {
		.segs = {
			{
				.type = htobe32(PAYLOAD_SEGMENT_DATA),
				.compression = htobe32(CBFS_COMPRESS_NONE),
				.offset = htobe32(offsetof(struct source_buffer, seg1)),
				.load_addr = htobe64((uintptr_t)seg1_dst),
				.len = htobe32(seg1_size),
				.mem_len = htobe32(seg1_dst_size),
			},
			{
				.type = htobe32(PAYLOAD_SEGMENT_CODE),
				.compression = htobe32(CBFS_COMPRESS_LZMA),
				.offset = htobe32(offsetof(struct source_buffer, seg2)),
				.load_addr = htobe64((uintptr_t)seg2_dst),
				.len = htobe32(seg2_comp_size),
				.mem_len = htobe32(seg2_dst_size),
			},
			{
				.type = htobe32(PAYLOAD_SEGMENT_BSS),
				.compression = htobe32(CBFS_COMPRESS_NONE),
				.offset = htobe32(0),
				.load_addr = htobe64((uintptr_t)bss),
				.len = htobe32(0),
				.mem_len = htobe32(bss_size),
			},
			{
				.type = htobe32(PAYLOAD_SEGMENT_ENTRY),
				.compression = htobe32(CBFS_COMPRESS_NONE),
				.offset = htobe32(0),
				.load_addr = htobe64((uintptr_t)entry_point),
				.len = htobe32(0),
				.mem_len = htobe32(0),
			},
		},
	};

	memset(src.seg1, '1', seg1_size);
	memset(src.seg2, '2', seg2_comp_size);

	/* Needs to be `char lst[]`, not `char *lst`, to avoid strsep()ing .rodata. */
	char lst[] = "1;altfw/testpayload;Test;Best payload for testing!\n";
	altfw_list = lst;
	altfw_testpayload = &src;
	altfw_testpayload_size = sizeof(src);

	expect_value(ulzman, src, src.seg2);
	expect_value(ulzman, srcn, seg2_comp_size);
	expect_value(ulzman, dst, seg2_dst);
	expect_value(ulzman, dstn, seg2_dst_size);
	expect_value(ulzman, src[0], '2');
	will_return(ulzman, seg2_decomp_size);

	expect_value(selfboot, entry, entry_point);

	payload_run_altfw(1);

	assert_filled_with(seg1_dst, '1', seg1_size);
	assert_filled_with(seg1_dst + seg1_size, 0, zero_extend_size);
	assert_filled_with(seg2_dst, 'L', seg2_decomp_size);
	assert_filled_with(seg2_dst + seg2_decomp_size, 0, zero_extend_size);
	assert_filled_with(bss, 0, bss_size);

	test_free(seg1_dst);
	test_free(seg2_dst);
	test_free(bss);
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_run_payload),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
