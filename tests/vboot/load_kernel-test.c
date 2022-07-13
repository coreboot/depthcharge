// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>
#include <vboot_api.h>

#include "mocks/callbacks.h"
#include "mocks/payload.h"
#include "mocks/util/commonparams.h"
#include "tests/test.h"
#include "tests/vboot/common.h"
#include "tests/vboot/ui/common.h"
#include "vboot/load_kernel.h"
#include "vboot/util/commonparams.h"

#define MAX_MOCK_DISKS 10

typedef struct {
	uint64_t bytes_per_lba;
	uint64_t lba_count;
	uint32_t flags;
	const char *name;
} disk_desc_t;

static const char correct_handle[] = "correct choice";
static const char wrong_handle[] = "wrong choice";
static const char invalid_kernel_handle[] = "invalid kernel";
static const char no_kernel_handle[] = "no kernel";
static const char no_kernel2_handle[] = "no kernel 2";

/* Mock data */
static struct vb2_disk_info mock_disks[MAX_MOCK_DISKS];
static struct ui_context test_ui_ctx;
static struct vb2_kernel_params test_kparams;

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

	memset(&mock_disks, 0, sizeof(mock_disks));

	return 0;
}

/* Mocked functions */

/* Do not reference these 3 functions directly in tests. Use the macro below. */
static disk_desc_t *_get_disks(void)
{
	return mock_type(disk_desc_t *);
}

static uint32_t _get_num_disks(void)
{
	return mock_type(uint32_t);
}

vb2_error_t VbExDiskGetInfo(struct vb2_disk_info **infos_ptr, uint32_t *count,
			    uint32_t disk_flags)
{
	int i;

	*infos_ptr = mock_disks;
	*count = _get_num_disks();
	disk_desc_t *disks = _get_disks();
	assert_in_range(*count, 0, MAX_MOCK_DISKS);

	for (i = 0; i < *count; i++) {
		mock_disks[i].bytes_per_lba = disks[i].bytes_per_lba;
		mock_disks[i].lba_count = disks[i].lba_count;
		mock_disks[i].streaming_lba_count = disks[i].lba_count;
		mock_disks[i].flags = disks[i].flags;
		mock_disks[i].handle = (vb2ex_disk_handle_t)disks[i].name;
	}

	return mock_type(vb2_error_t);
}

/* Macros for mocking VbExDiskGetInfo(). */
#define WILL_GET_DISKS(disks, rv) do { \
	will_return_always(_get_disks, disks); \
	will_return_always(_get_num_disks, ARRAY_SIZE(disks)); \
	will_return_always(VbExDiskGetInfo, rv); \
} while (0)

/* Do not reference this function directly in tests. Use the macro below. */
vb2_error_t VbExDiskFreeInfo(struct vb2_disk_info *infos,
			     vb2ex_disk_handle_t preserve_handle)
{
	check_expected(preserve_handle);
	return mock_type(vb2_error_t);
}

/* Macros for mocking VbExDiskGetInfo(). */
#define WILL_FREE_DISKS(rv, ptr) do { \
	will_return(VbExDiskFreeInfo, rv); \
	expect_value(VbExDiskFreeInfo, preserve_handle, ptr); \
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
	check_disk_handle(disk_info->handle);
	check_external_flag(!!(disk_info->flags & VB2_DISK_FLAG_EXTERNAL_GPT));
	return mock_type(vb2_error_t);
}

vb2_error_t vb2api_load_minios_kernel(struct vb2_context *c,
				      struct vb2_kernel_params *params,
				      struct vb2_disk_info *disk_info,
				      uint32_t minios_flags)
{
	check_disk_handle(disk_info->handle);
	check_external_flag(!!(disk_info->flags & VB2_DISK_FLAG_EXTERNAL_GPT));
	return mock_type(vb2_error_t);
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

static void test_lk_first_disk_removable(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE | VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {
		{4096, 100, VB2_DISK_FLAG_REMOVABLE, correct_handle},
		{4096, 100, VB2_DISK_FLAG_FIXED, wrong_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, flags, ui->kparams));
}

static void test_lk_first_disk_fixed(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE | VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {
		{4096, 100, VB2_DISK_FLAG_FIXED, correct_handle},
		{4096, 100, VB2_DISK_FLAG_REMOVABLE, wrong_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, flags, ui->kparams));
}

static void test_lk_skip_invalid_removable_disks(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		/* too small */
		{512, 10, VB2_DISK_FLAG_REMOVABLE, 0},
		/* wrong LBA */
		{511, 100, VB2_DISK_FLAG_REMOVABLE, 0},
		/* not a power of 2 */
		{2047, 100, VB2_DISK_FLAG_REMOVABLE, 0},
		/* wrong type */
		{512, 100, VB2_DISK_FLAG_FIXED, 0},
		/* wrong flags */
		{512, 100, 0, 0},
		/* still wrong flags */
		{512, 100, -1, 0},
		{4096, 100, VB2_DISK_FLAG_REMOVABLE, correct_handle},
		/* already got one */
		{512, 100, VB2_DISK_FLAG_REMOVABLE, wrong_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, flags, ui->kparams));
}

static void test_lk_skip_invalid_fixed_disks(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {
		/* too small */
		{512, 10, VB2_DISK_FLAG_FIXED, 0},
		/* wrong LBA */
		{511, 100, VB2_DISK_FLAG_FIXED, 0},
		/* not a power of 2 */
		{2047, 100, VB2_DISK_FLAG_FIXED, 0},
		/* wrong type */
		{512, 100, VB2_DISK_FLAG_REMOVABLE, 0},
		/* wrong flags */
		{512, 100, 0, 0},
		/* still wrong flags */
		{512, 100, -1, 0},
		/* flags */
		{512, 100, VB2_DISK_FLAG_REMOVABLE | VB2_DISK_FLAG_FIXED, 0},
		{512, 100, VB2_DISK_FLAG_FIXED, correct_handle},
		/* already got one */
		{512, 100, VB2_DISK_FLAG_FIXED, wrong_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, flags, ui->kparams));
}

static void test_lk_allow_externel_gpt(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_REMOVABLE | VB2_DISK_FLAG_EXTERNAL_GPT,
		 correct_handle},
		/* already got one */
		{512, 100, VB2_DISK_FLAG_REMOVABLE, wrong_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 1, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, flags, ui->kparams));
}

static void test_lk_skip_invalid_kernel(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		/* wrong flags */
		{512, 100, 0, 0},
		{512, 100, VB2_DISK_FLAG_REMOVABLE, wrong_handle},
		{512, 100, VB2_DISK_FLAG_REMOVABLE, correct_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0, wrong_handle);
	WILL_LOAD_KERNEL(VB2_SUCCESS, 0, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_kernel(ui->ctx, flags, ui->kparams));
}

static void test_lk_no_disks_at_all(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_NO_DISK);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_NO_DISK_FOUND);
}

static void test_lk_invalid_kernel_before_no_kernel(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {
		/* doesn't load */
		{512, 100, VB2_DISK_FLAG_FIXED, invalid_kernel_handle},
		/* doesn't load */
		{512, 100, VB2_DISK_FLAG_FIXED, no_kernel_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
			 invalid_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_INVALID_OS);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
}

static void test_lk_invalid_kernel_after_no_kernel(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {
		{512, 1000, VB2_DISK_FLAG_FIXED, no_kernel_handle},
		{512, 1000, VB2_DISK_FLAG_FIXED, invalid_kernel_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
			 invalid_kernel_handle);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_INVALID_OS);

	/* INVALID_KERNEL_FOUND overwrites NO_KERNEL_FOUND */
	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
}

static void test_lk_invalid_kernel_removable(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_REMOVABLE, invalid_kernel_handle},
		{512, 100, VB2_DISK_FLAG_REMOVABLE, no_kernel_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
			 invalid_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_INVALID_OS);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
}

static void test_lk_invalid_kernel_removable_rec_mode(void **state)
{
	struct ui_context *ui = *state;
	set_boot_mode(ui->ctx, VB2_BOOT_MODE_MANUAL_RECOVERY);
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_REMOVABLE, invalid_kernel_handle},
		{512, 100, VB2_DISK_FLAG_REMOVABLE, no_kernel_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
			 invalid_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
}

static void test_lk_invalid_kernel_removable_dev_mode(void **state)
{
	struct ui_context *ui = *state;
	set_boot_mode(ui->ctx, VB2_BOOT_MODE_DEVELOPER);
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_REMOVABLE, invalid_kernel_handle},
		{512, 100, VB2_DISK_FLAG_REMOVABLE, no_kernel_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
			 invalid_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_INVALID_KERNEL_FOUND);
}

static void test_lk_no_kernel_fixed(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_FIXED;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_FIXED, no_kernel_handle},
		{512, 1000, VB2_DISK_FLAG_FIXED, no_kernel2_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel2_handle);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_NO_KERNEL);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_NO_KERNEL_FOUND);
}

static void test_lk_no_kernel_removable(void **state)
{
	struct ui_context *ui = *state;
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_REMOVABLE, no_kernel_handle},
		{512, 1000, VB2_DISK_FLAG_REMOVABLE, no_kernel2_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel2_handle);
	expect_value(vb2api_fail, reason, VB2_RECOVERY_RW_NO_KERNEL);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_NO_KERNEL_FOUND);
}

static void test_lk_no_kernel_removable_rec_mode(void **state)
{
	struct ui_context *ui = *state;
	set_boot_mode(ui->ctx, VB2_BOOT_MODE_MANUAL_RECOVERY);
	uint32_t flags = VB2_DISK_FLAG_REMOVABLE;
	disk_desc_t disks[] = {
		{512, 100, VB2_DISK_FLAG_REMOVABLE, no_kernel_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, NULL);
	WILL_LOAD_KERNEL(VB2_ERROR_LK_NO_KERNEL_FOUND, 0, no_kernel_handle);

	assert_int_equal(vboot_load_kernel(ui->ctx, flags, ui->kparams),
			 VB2_ERROR_LK_NO_KERNEL_FOUND);
}

static void test_lmk_pick_first_fixed_disk(void **state)
{
	struct ui_context *ui = *state;
	disk_desc_t disks[] = {
		{4096, 100, VB2_DISK_FLAG_REMOVABLE, 0},
		{4096, 100, VB2_DISK_FLAG_FIXED, correct_handle},
		{4096, 100, VB2_DISK_FLAG_FIXED, wrong_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_MINIOS_KERNEL(VB2_SUCCESS, 0, correct_handle);

	ASSERT_VB2_SUCCESS(vboot_load_minios_kernel(ui->ctx, 0, ui->kparams));
}

static void test_lmk_skip_failed_fixed_disk(void **state)
{
	struct ui_context *ui = *state;
	disk_desc_t disks[] = {
		{4096, 100, VB2_DISK_FLAG_FIXED, wrong_handle},
		{4096, 100, VB2_DISK_FLAG_FIXED, correct_handle},
	};
	WILL_GET_DISKS(disks, VB2_SUCCESS);
	WILL_FREE_DISKS(VB2_SUCCESS, correct_handle);
	WILL_LOAD_MINIOS_KERNEL(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 0,
				wrong_handle);
	WILL_LOAD_MINIOS_KERNEL(VB2_SUCCESS, 0, correct_handle);

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
