// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>
#include <vboot_api.h>

#include "base/list.h"
#include "drivers/storage/blockdev.h"
#include "mocks/callbacks.h"
#include "mocks/payload.h"
#include "mocks/util/commonparams.h"
#include "tests/test.h"
#include "tests/vboot/common.h"
#include "tests/vboot/ui/common.h"
#include "vboot/load_kernel.h"
#include "vboot/util/commonparams.h"

typedef struct {
	uint64_t bytes_per_lba;
	uint64_t lba_count;
	uint32_t flags;
	const char *name;
} disk_desc_t;

/* Mock data */
static struct ui_context test_ui_ctx;
static struct vb2_kernel_params test_kparams;
static BlockDev bdev_head;

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;
	test_ui_ctx.ctx = vboot_get_context();
	set_boot_mode(test_ui_ctx.ctx, VB2_BOOT_MODE_NORMAL);

	memset(&test_kparams, 0, sizeof(test_kparams));
	test_ui_ctx.kparams = &test_kparams;
	*state = &test_ui_ctx;

	memset(&bdev_head, 0, sizeof(bdev_head));

	return 0;
}

/* Mocked functions */

/* Do not reference these 2 functions directly in tests. Use the macro below. */
static void link_disk(BlockDev *dev, uint32_t count)
{
	for (int i = count - 1; i >= 0; i--)
		list_insert_after(&dev[i].list_node, &bdev_head.list_node);
}

int get_all_bdevs(blockdev_type_t type, ListNode **bdevs)
{
	BlockDev *bdev;
	check_expected(type);
	*bdevs = &bdev_head.list_node;
	list_for_each(bdev, bdev_head.list_node, list_node)
		bdev->removable = type == BLOCKDEV_REMOVABLE;
	return mock_type(int);
}

#define WILL_GET_DISKS(disks, _type) do { \
	link_disk(disks, ARRAY_SIZE(disks)); \
	will_return_always(get_all_bdevs, ARRAY_SIZE(disks)); \
	expect_value(get_all_bdevs, type, _type); \
} while (0)

/* Do not reference these 4 functions directly in tests. Use the macro below. */
static void check_disk_handle(const char *preserve_handle)
{
	check_expected_ptr(preserve_handle);
}

static void check_external_flag(int flag)
{
	check_expected(flag);
}

vb2_error_t vb2api_load_kernel(struct vb2_context *c,
			       struct vb2_kernel_params *params,
			       struct vb2_disk_info *disk_info)
{
	vb2_error_t rv = mock_type(vb2_error_t);
	check_disk_handle(disk_info->handle);
	check_external_flag(!!(disk_info->flags & VB2_DISK_FLAG_EXTERNAL_GPT));
	if (rv == VB2_SUCCESS)
		params->disk_handle = disk_info->handle;
	return rv;
}

vb2_error_t vb2api_load_minios_kernel(struct vb2_context *c,
				      struct vb2_kernel_params *params,
				      struct vb2_disk_info *disk_info,
				      uint32_t minios_flags)
{
	vb2_error_t rv = mock_type(vb2_error_t);
	check_disk_handle(disk_info->handle);
	check_external_flag(!!(disk_info->flags & VB2_DISK_FLAG_EXTERNAL_GPT));
	if (rv == VB2_SUCCESS)
		params->disk_handle = disk_info->handle;
	return rv;
}

/* Macros for mocking vb2api_load_kernel(). */
#define WILL_LOAD_KERNEL(rv, expected_flag, handle) do { \
	will_return(vb2api_load_kernel, rv); \
	expect_value(check_external_flag, flag, expected_flag); \
	expect_value(check_disk_handle, preserve_handle, handle); \
} while (0)

#define WILL_LOAD_KERNEL_COUNT(rv, expected_flag, handle, count) do { \
	will_return_count(vb2api_load_kernel, rv, count); \
	expect_value_count(check_external_flag, flag, expected_flag, count); \
	expect_value_count(check_disk_handle, preserve_handle, handle, count); \
} while (0)

/* Macros for mocking vb2api_load_minios_kernel(). */
#define WILL_LOAD_MINIOS_KERNEL(rv, expected_flag, handle) do { \
	will_return(vb2api_load_minios_kernel, rv); \
	expect_value(check_external_flag, flag, expected_flag); \
	expect_value(check_disk_handle, preserve_handle, handle); \
} while (0)

void vb2api_fail(struct vb2_context *ctx, uint8_t reason, uint8_t subcode)
{
	check_expected(reason);
}

/* Test functions */

#define _BDEV(_block_size, _block_count, _external_gpt, ...) { \
	.block_size = _block_size, \
	.block_count = _block_count, \
	.stream_block_count = _block_count, \
	.external_gpt = _external_gpt, \
}
# define BDEV(block_size, block_count, ...) \
	_BDEV(block_size, block_count, ##__VA_ARGS__, 0)

static void test_lk_first_disk_removable(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		BDEV(4096, 100),
		BDEV(4096, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, &disks[0]);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, type, ui->kparams));
}

static void test_lk_first_disk_fixed(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {
		BDEV(4096, 100),
		BDEV(4096, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, &disks[0]);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, type, ui->kparams));
}

static void test_lk_skip_invalid_removable_disks(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		/* too small */
		BDEV(512, 10),
		/* wrong block_size */
		BDEV(511, 100),
		/* not a power of 2 */
		BDEV(2047, 100),
		BDEV(4096, 100),
		/* already got one */
		BDEV(4096, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, &disks[3]);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, type, ui->kparams));
}

static void test_lk_skip_invalid_fixed_disks(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		/* too small */
		BDEV(512, 10),
		/* wrong block_size */
		BDEV(511, 100),
		/* not a power of 2 */
		BDEV(2047, 100),
		BDEV(4096, 100),
		/* already got one */
		BDEV(4096, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, &disks[3]);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, type, ui->kparams));
}

static void test_lk_allow_externel_gpt(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		BDEV(512, 100, 1),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 1, &disks[0]);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, type, ui->kparams));
}

static void test_lk_skip_invalid_kernel(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, &disks[1]);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, type, ui->kparams));
}

static void test_lk_no_disks_at_all(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {};
	WILL_GET_DISKS(disks, type);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_NO_DISK);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_NO_DISK_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_invalid_kernel_before_no_kernel(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[1]);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_INVALID_OS);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_invalid_kernel_after_no_kernel(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, &disks[1]);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_INVALID_OS);

	/* INVALID_KERNEL_FOUND overwrites NO_KERNEL_FOUND */
	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_invalid_kernel_removable(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[1]);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_INVALID_OS);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_invalid_kernel_removable_rec_mode(void **state)
{
	struct ui_context *ui = *state;
	set_boot_mode(ui->ctx, VB2_BOOT_MODE_MANUAL_RECOVERY);
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[1]);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_invalid_kernel_removable_dev_mode(void **state)
{
	struct ui_context *ui = *state;
	set_boot_mode(ui->ctx, VB2_BOOT_MODE_DEVELOPER);
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[1]);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_no_kernel_fixed(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_FIXED;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[1]);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_NO_KERNEL);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_NO_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_no_kernel_removable(void **state)
{
	struct ui_context *ui = *state;
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[0]);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[1]);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_NO_KERNEL);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_NO_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lk_no_kernel_removable_rec_mode(void **state)
{
	struct ui_context *ui = *state;
	set_boot_mode(ui->ctx, VB2_BOOT_MODE_MANUAL_RECOVERY);
	blockdev_type_t type = BLOCKDEV_REMOVABLE;
	BlockDev disks[] = {
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, type);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, &disks[0]);

	assert_int_equal(vboot_load_kernel(ui->ctx, type, ui->kparams),
			 VB2_ERROR_LK_NO_KERNEL_FOUND);
	assert_null(ui->kparams->disk_handle);
}

static void test_lmk_pick_first_fixed_disk(void **state)
{
	struct ui_context *ui = *state;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, BLOCKDEV_FIXED);
	WILL_LOAD_MINIOS_KERNEL(VB2_SUCCESS, 0, &disks[0]);

	ASSERT_VB2_SUCCESS(vboot_load_minios_kernel(ui->ctx, 0, ui->kparams));
}

static void test_lmk_skip_failed_fixed_disk(void **state)
{
	struct ui_context *ui = *state;
	BlockDev disks[] = {
		BDEV(512, 100),
		BDEV(512, 100),
	};
	WILL_GET_DISKS(disks, BLOCKDEV_FIXED);
	WILL_LOAD_MINIOS_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
				&disks[0]);
	WILL_LOAD_MINIOS_KERNEL(VB2_SUCCESS, 0, &disks[1]);

	ASSERT_VB2_SUCCESS(vboot_load_minios_kernel(ui->ctx, 0, ui->kparams));
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_lk_first_disk_removable),
		TEST(test_lk_first_disk_fixed),
		TEST(test_lk_skip_invalid_removable_disks),
		TEST(test_lk_skip_invalid_fixed_disks),
		TEST(test_lk_allow_externel_gpt),
		TEST(test_lk_skip_invalid_kernel),
		TEST(test_lk_no_disks_at_all),
		TEST(test_lk_invalid_kernel_before_no_kernel),
		TEST(test_lk_invalid_kernel_after_no_kernel),
		TEST(test_lk_invalid_kernel_removable),
		TEST(test_lk_invalid_kernel_removable_rec_mode),
		TEST(test_lk_invalid_kernel_removable_dev_mode),
		TEST(test_lk_no_kernel_fixed),
		TEST(test_lk_no_kernel_removable),
		TEST(test_lk_no_kernel_removable_rec_mode),
		TEST(test_lmk_pick_first_fixed_disk),
		TEST(test_lmk_skip_failed_fixed_disk),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
