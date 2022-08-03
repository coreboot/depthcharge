// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/storage/info.h"
#include "drivers/storage/ufs.h"
#include "tests/test.h"

#include "drivers/storage/ufs.c"

/* Since UfsCRespUPIU contains a flexible array member, we need to allocate the
   memory by our own. */
static uint8_t resp_upiu_buffer[sizeof(UfsCRespUPIU) + sizeof(UfsSenseData)];
static UfsCRespUPIU *resp_upiu = (UfsCRespUPIU *)resp_upiu_buffer;
static UfsSenseData *sense_data =
	(UfsSenseData *)(resp_upiu_buffer + sizeof(UfsCRespUPIU));

static int setup(void **state)
{
	memset(resp_upiu_buffer, 0, sizeof(resp_upiu_buffer));
	return 0;
}

#define ASSERT_UFS_STORAGE_TEST_LOG(_result, _return_code, _sense_key) do { \
	assert_int_equal_msg((_result).type, STORAGE_INFO_TYPE_UFS, \
			     "storage type UFS"); \
	assert_int_equal_msg((_result).data.ufs_data.return_code, \
			     (_return_code), "selftest return code"); \
	assert_int_equal_msg((_result).data.ufs_data.sense_key, \
			     (_sense_key), "selftest sense key"); \
} while (0)

static inline void mock_ufs_cresp_upiu(uint8_t status, uint8_t sense_key)
{
	resp_upiu->status = status;
	resp_upiu->data_segment_len = htobe16(UFS_SENSE_DATA_SIZE);
	sense_data->len = htobe16(UFS_SENSE_SIZE);
	sense_data->sense = (UfsSense){
		.response_code = SCSI_SENSE_FIXED_FORMAT,
		.sense_key = sense_key,
	};
}

/* SEND DIAGNOSTIC command finished and selftest passed. */
static void test_ufs_selftest_success(void **state)
{
	StorageTestLog result;
	mock_ufs_cresp_upiu(SCSI_STATUS_GOOD, SENSE_KEY_NO_SENSE);
	assert_int_equal(block_ufs_process_diagnostic(0, resp_upiu,
						      &result), 0);
	ASSERT_UFS_STORAGE_TEST_LOG(result, SCSI_STATUS_GOOD,
				    SENSE_KEY_NO_SENSE);
}

/* SEND DIAGNOSTIC command finished but selftest failed (check sense key). */
static void test_ufs_selftest_failed(void **state)
{
	StorageTestLog result;
	mock_ufs_cresp_upiu(SCSI_STATUS_CHK_COND, SENSE_KEY_ILLEGAL_REQUEST);
	assert_int_equal(block_ufs_process_diagnostic(UFS_EIO, resp_upiu,
						      &result), 0);
	ASSERT_UFS_STORAGE_TEST_LOG(result, SCSI_STATUS_CHK_COND,
				    SENSE_KEY_ILLEGAL_REQUEST);
}

/* SEND DIAGNOSTIC command failed (SCSI error). */
static void test_ufs_selftest_scsi_error(void **state)
{
	StorageTestLog result;
	assert_int_not_equal(block_ufs_process_diagnostic(UFS_EPROTO, resp_upiu,
							  &result), 0);
}

#define UFS_SELFTEST_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UFS_SELFTEST_TEST(test_ufs_selftest_success),
		UFS_SELFTEST_TEST(test_ufs_selftest_failed),
		UFS_SELFTEST_TEST(test_ufs_selftest_scsi_error),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
