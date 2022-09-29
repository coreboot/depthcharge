// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google LLC
 */

#include <libpayload.h>

#include "diag/common.h"
#include "diag/diag_internal.h"
#include "diag/storage_test.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/ufs.h"
#include "drivers/timer/timer.h"

#define HALF_BYTE_LOW(x) ((x) & 0xf)
#define HALF_BYTE_HIGH(x) ((x) >> 4)

/* Global variables. */
struct storage_test_state {
	/* Stopwatch to prevent too frequent dump requests. */
	struct stopwatch sw;
	/* Current running test type. */
	BlockDevTestOpsType type;
	/* Ensure that the first dump will not be skipped */
	bool is_first_dump;
	/*
	 * The storage test result (for diagnostic reports).
	 * If state.type is not BLOCKDEV_TEST_OPS_TYPE_STOP, it means that the
	 * test is still running and the test_result should be
	 * DIAG_TEST_UPDATED; otherwise, the test is finished and the
	 * test_result should be either DIAG_TEST_PASSED or DIAG_TEST_FAILED.
	 */
	DiagTestResult test_result;
};

struct storage_test_ops {
	char *(*stringify)(char *buf, const char *end,
			   const StorageTestLog *log);
	DiagTestResult (*get_result)(const StorageTestLog *log);
};

static struct storage_test_state state = {
	.is_first_dump = true,
	.type = BLOCKDEV_TEST_OPS_TYPE_STOP,
	.test_result = DIAG_TEST_UPDATED,
};

/* Get current test log delay based on the running test type. */
static inline uint32_t get_test_log_delay(void)
{
	switch (state.type) {
	case BLOCKDEV_TEST_OPS_TYPE_SHORT:
		return DIAG_STORAGE_TEST_SHORT_DELAY_MS;
	case BLOCKDEV_TEST_OPS_TYPE_EXTENDED:
		return DIAG_STORAGE_TEST_EXTENDED_DELAY_MS;
	default:
		return DIAG_STORAGE_TEST_DEFAULT_DELAY_MS;
	}
}

static BlockDev *get_first_fixed_block_device(void)
{
	static BlockDev *bdev;

	/* Cache bdev to prevent too frequent get_all_bdevs calls. */
	if (bdev)
		return bdev;

	ListNode *devs;
	int n = get_all_bdevs(BLOCKDEV_FIXED, &devs);
	if (!n) {
		printf("%s: No storage device found.\n", __func__);
		return NULL;
	}
	if (n > 1) {
		printf("%s: More than one device found.\n", __func__);
		return NULL;
	}
	list_for_each(bdev, *devs, list_node) { return bdev; }
	return NULL;
}

static inline const char *type_str(uint8_t type)
{
	switch (type) {
	case 0x1:
		return "Short device self-test";
	case 0x2:
		return "Extended device self-test";
	}
	return "Unknown test type";
}

static int32_t get_test_remain_time_seconds(uint8_t current_completion,
					    int reset)
{
	static uint64_t start_us;
	static uint64_t now_us;
	if (reset) {
		start_us = now_us = timer_us(0);
		return 0;
	}
	if (current_completion == 0)
		return -1;

	/* If the passed time since the last call less than 1 second, don't
	 * update the estimated time to prevent the value changing too quickly.
	 */
	uint64_t now_us_tmp = timer_us(0);
	if (now_us_tmp - now_us > USECS_PER_SEC)
		now_us = now_us_tmp;

	uint64_t passed_us = now_us - start_us;
	uint8_t remain_percent = 100 - current_completion;
	uint64_t remain_us = passed_us / current_completion * remain_percent;
	return remain_us / USECS_PER_SEC;
}

static char *print_test_completion(char *buf, const char *end,
				   uint8_t current_completion)
{
	buf = APPEND(buf, end, "Completion: '%d%%'", current_completion);
	int32_t remain_sec =
		get_test_remain_time_seconds(current_completion, 0);
	if (remain_sec > 0) {
		if (remain_sec > 60)
			buf = APPEND(buf, end, ", %d minutes remaining",
				     remain_sec / 60);
		else
			buf = APPEND(buf, end, ", %d seconds remaining",
				     remain_sec);
	}
	buf = APPEND(buf, end, "...\n\n");
	return buf;
}

/******************************************************************************/
/* stubs */

static char *stringify_stub_test_status(char *buf, const char *end,
					const StorageTestLog *log)
{
	return APPEND(buf, end, "UNSUPPORTED\n");
}

static DiagTestResult get_stub_test_result(const StorageTestLog *log)
{
	return DIAG_TEST_ERROR;
}

static const struct storage_test_ops stub_test_ops = {
	.stringify = stringify_stub_test_status,
	.get_result = get_stub_test_result,
};

/******************************************************************************/
/* NVMe device self-test */

static inline const char *nvme_test_result_str(uint8_t result)
{
	/* From NVMe 1.4 spec. */
	switch (result) {
	case 0x0:
		return "Test completed without error";
	case 0x1:
		return "Operation was aborted by a Device Self-test command";
	case 0x2:
		return "Operation was aborted by a Controller Level Reset";
	case 0x3:
		return "Operation was aborted due to a removal of a namespace "
		       "from the namespace inventory";
	case 0x4:
		return "Operation was aborted due to the processing of a "
		       "Format NVM command";
	case 0x5:
		return "A fatal error or unknown test error occurred "
		       "while the controller was executing the device "
		       "self-test operation and the operation did not "
		       "complete";
	case 0x6:
		return "Operation completed with a segment that failed and the "
		       "segment that failed is not known";
	case 0x7:
		return "Operation completed with one or more failed segments";
	case 0x8:
		return "Operation was aborted for unknown reason";
	case 0x9:
		return "Operation was aborted due to a sanitize operation";
	}
	return "Test result is unknown";
}

static char *nvme_print_test_result_detail(char *buf, const char *end,
					   const NvmeTestLogData *data)
{
	uint8_t result = HALF_BYTE_LOW(data->status);
	buf = APPEND(buf, end, "%s.\n", nvme_test_result_str(result));
	if (result == 0x7)
		buf = APPEND(buf, end,
			     "Segment Number where the first self-test "
			     "failure occurred: '%#x'.\n",
			     data->segment_number);
	buf = APPEND(buf, end, "\n");
	return buf;
}

#define SC_VALID	BIT(3)
#define SCT_VALID	BIT(2)
#define FLBA_VALID	BIT(1)
#define NSID_VALID	BIT(0)

static char *nvme_print_test_result(char *buf, const char *end,
				    const NvmeTestLogData *data)
{
	uint8_t type = HALF_BYTE_HIGH(data->status);
	buf = APPEND(buf, end, "Test type: %s.\n\n", type_str(type));
	buf = nvme_print_test_result_detail(buf, end, data);
	buf = APPEND(buf, end, "Power on hours(POH): %llu\n", data->poh);
	if (data->valid_diag_info & NSID_VALID)
		buf = APPEND(buf, end, "Namespace Identifier (NSID) of "
			     "Failing LBA: '%#x'\n", data->nsid);
	if (data->valid_diag_info & FLBA_VALID)
		buf = APPEND(buf, end, "Failing LBA: '%#llx'\n",
			     data->failing_lba);
	if (data->valid_diag_info & SCT_VALID)
		buf = APPEND(buf, end, "Status code type (SCT): '%#x'\n",
			     data->status_code_type);
	if (data->valid_diag_info & SC_VALID)
		buf = APPEND(buf, end, "Status code (SC): '%#x'\n",
			     data->status_code);
	return buf;
}

static char *stringify_nvme_test_status(char *buf, const char *end,
					const StorageTestLog *log)
{
	const NvmeTestLogData *data = &log->data.nvme_data;
	uint8_t current_op = HALF_BYTE_LOW(data->current_operation);
	switch (current_op) {
	case 0x0:
		buf = nvme_print_test_result(buf, end, data);
		break;
	case 0x1:
	case 0x2:
		buf = APPEND(buf, end, "Test type: %s.\n\n",
			     type_str(current_op));
		buf = print_test_completion(buf, end, data->current_completion);
		break;
	default:
		buf = APPEND(buf, end,
			     "Unknown status, Current Device Self-Test "
			     "Operation: '%#x'\n", current_op);
		break;
	}
	return buf;
}

static DiagTestResult get_nvme_test_result(const StorageTestLog *log)
{
	const NvmeTestLogData *data = &log->data.nvme_data;

	/* Test is still running. */
	if (HALF_BYTE_LOW(data->current_operation))
		return DIAG_TEST_UPDATED;

	/* Check if test result is 0x0. */
	if (HALF_BYTE_LOW(data->status) == 0)
		return DIAG_TEST_PASSED;

	return DIAG_TEST_FAILED;
}

static const struct storage_test_ops nvme_test_ops = {
	.stringify = stringify_nvme_test_status,
	.get_result = get_nvme_test_result,
};

/******************************************************************************/
/* UFS SCSI send diagnostic */

static inline const char *ufs_sense_key_str(uint8_t sense_key)
{
	switch(sense_key) {
	case SENSE_KEY_ILLEGAL_REQUEST:
		return "ILLEGAL REQUEST (range or CDB errors)";
	case SENSE_KEY_MEDIUM_ERROR:
		return "MEDIUM ERROR (medium failure, ECC, etc.)";
	case SENSE_KEY_HARDWARE_ERROR:
		return "HARDWARE ERROR (hardware failure)";
	case SENSE_KEY_UNIT_ATTENTION:
		return "UNIT ATTENTION (reset, power-on, etc.)";
	}
	return "UNKNOWN SENSE ERROR";
}

static char *stringify_ufs_test_status(char *buf, const char *end,
				       const StorageTestLog *log)
{
	const UfsTestLogData *data = &log->data.ufs_data;
	buf = APPEND(buf, end, "Self-test returned ");
	switch(data->return_code) {
	case SCSI_STATUS_GOOD:
		buf = APPEND(buf, end, "GOOD status\n");
		break;
	case SCSI_STATUS_CHK_COND:
		buf = APPEND(buf, end, "FAILED\nSense key: %s\n",
			     ufs_sense_key_str(data->sense_key));
		break;
	default:
		buf = APPEND(buf, end, "UNKNOWN status\n");
	}
	return buf;
}

static DiagTestResult get_ufs_test_result(const StorageTestLog *log)
{
	const UfsTestLogData *data = &log->data.ufs_data;

	/* UFS self-test is a blocking call so it should not be running at this
	   point. Therefore we could directly check its return_code. */
	if (data->return_code == SCSI_STATUS_GOOD)
		return DIAG_TEST_PASSED;

	return DIAG_TEST_FAILED;
}

static const struct storage_test_ops ufs_test_ops = {
	.stringify = stringify_ufs_test_status,
	.get_result = get_ufs_test_result,
};

/******************************************************************************/
/* Helpers */

static const struct storage_test_ops *get_test_ops(const StorageTestLog *log)
{
	switch (log->type) {
	case STORAGE_INFO_TYPE_NVME:
		if (!CONFIG(DRIVER_STORAGE_NVME))
			break;
		return &nvme_test_ops;
	case STORAGE_INFO_TYPE_MMC:
		/* MMC does not support self-test. */
		return &stub_test_ops;
	case STORAGE_INFO_TYPE_UFS:
		if (!CONFIG(DRIVER_STORAGE_UFS))
			break;
		return &ufs_test_ops;
	case STORAGE_INFO_TYPE_UNKNOWN:
		break;
	}
	die("unsupported data type: %d\n", log->type);
	return NULL;
}

/******************************************************************************/
/* APIs */

uint32_t diag_storage_test_supported(void)
{
	BlockDev *dev = get_first_fixed_block_device();
	if (dev && dev->ops.get_test_log && dev->ops.test_control &&
	    dev->ops.test_support)
		return dev->ops.test_support();
	return 0;
}

DiagTestResult diag_dump_storage_test_log(char *buf, const char *end)
{
	BlockDev *dev = get_first_fixed_block_device();
	if (!diag_storage_test_supported()) {
		printf("%s: No supported.\n", __func__);
		return DIAG_TEST_UNIMPLEMENTED;
	}

	/* No test is running. */
	if (state.type == BLOCKDEV_TEST_OPS_TYPE_STOP)
		return state.test_result;

	/* Skip this call if this is not the very first dump call after a
	   command and the stopwatch has not expired yet. */
	if (!state.is_first_dump && !stopwatch_expired(&state.sw))
		return DIAG_TEST_RUNNING;
	stopwatch_init_msecs_expire(&state.sw, get_test_log_delay());
	state.is_first_dump = false;

	StorageTestLog log = {0};
	const struct storage_test_ops *ops;

	int res = dev->ops.get_test_log(&dev->ops, state.type, &log);
	if (res) {
		buf = APPEND(buf, end, "%s: Get Test Result error: %d\n",
			     dev->name, res);
		return DIAG_TEST_ERROR;
	}
	ops = get_test_ops(&log);

	buf = APPEND(buf, end, "Block device '%s':\n", dev->name);
	buf = ops->stringify(buf, end, &log);

	state.test_result = ops->get_result(&log);
	/* The test is stopped. */
	if (state.test_result != DIAG_TEST_UPDATED)
		state.type = BLOCKDEV_TEST_OPS_TYPE_STOP;

	return state.test_result;
}

DiagTestResult diag_storage_test_control(enum BlockDevTestOpsType ops)
{
	BlockDev *dev = get_first_fixed_block_device();
	if (!diag_storage_test_supported()) {
		printf("%s: No supported.\n", __func__);
		return DIAG_TEST_UNIMPLEMENTED;
	}
	if (dev->ops.test_control(&dev->ops, ops))
		return DIAG_TEST_ERROR;
	get_test_remain_time_seconds(0, 1);

	/* Set the global variables for the following diag_dump_storage_test_log
	   calls. */
	state.type = ops;
	state.is_first_dump = true;
	state.test_result = DIAG_TEST_UPDATED;

	return DIAG_TEST_PASSED;
}
