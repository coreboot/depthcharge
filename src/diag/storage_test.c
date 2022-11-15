// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google Inc.
 */

#include <libpayload.h>

#include "diag/storage_test.h"

#define HALF_BYTE_LOW(x) ((x) & 0xf)
#define HALF_BYTE_HIGH(x) ((x) >> 4)

static BlockDev *get_first_fixed_block_device(void)
{
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
	BlockDev *bdev;
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

static inline const char *test_result_str(uint8_t result)
{
	/* From Nvme 1.4 spec. */
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

static char *print_test_result_detail(char *buf, const char *end,
				      const NvmeTestLogData *data)
{
	uint8_t result = HALF_BYTE_LOW(data->status);
	buf += snprintf(buf, end - buf, "%s.\n", test_result_str(result));
	if (result == 0x7)
		buf += snprintf(buf, end - buf,
				"Segment Number where the first "
				"self-test failure occurred: '%#x'.\n",
				data->segment_number);
	buf += snprintf(buf, end - buf, "\n");
	return buf;
}

#define SC_VALID 8
#define SCT_VALID 4
#define FLBA_VALID 2
#define NSID_VALID 1

static char *print_test_result(char *buf, const char *end,
			       const NvmeTestLogData *data)
{
	uint8_t type = HALF_BYTE_HIGH(data->status);
	buf += snprintf(buf, end - buf, "Test type: %s.\n\n", type_str(type));
	buf = print_test_result_detail(buf, end, data);
	buf += snprintf(buf, end - buf, "Power on hours(POH): %llu\n",
			data->poh);
	if (data->valid_diag_info & NSID_VALID)
		buf += snprintf(buf, end - buf,
				"Namespace Identifier (NSID) of "
				"Failing LBA: '%#x'\n",
				data->nsid);
	if (data->valid_diag_info & FLBA_VALID)
		buf += snprintf(buf, end - buf, "Failing LBA: '%#llx'\n",
				data->failing_lba);
	if (data->valid_diag_info & SCT_VALID)
		buf += snprintf(buf, end - buf,
				"Status code type (SCT): '%#x'\n",
				data->status_code_type);
	if (data->valid_diag_info & SC_VALID)
		buf += snprintf(buf, end - buf, "Status code (SC): '%#x'\n",
				data->status_code);
	return buf;
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
	buf += snprintf(buf, end - buf, "Completion: '%d%%'",
			current_completion);
	int32_t remain_sec =
		get_test_remain_time_seconds(current_completion, 0);
	if (remain_sec > 0) {
		if (remain_sec > 60) {
			int32_t remain_min = remain_sec / 60;
			buf += snprintf(buf, end - buf,
					", %d minutes remaining", remain_min);
		} else
			buf += snprintf(buf, end - buf,
					", %d seconds remaining", remain_sec);
	}
	buf += snprintf(buf, end - buf, "...\n\n");
	return buf;
}

static char *stringify_test_status(char *buf, const char *end,
				   StorageTestLog *log)
{
	const NvmeTestLogData *data = &log->data.nvme_data;
	uint8_t current_op = HALF_BYTE_LOW(data->current_operation);
	switch (current_op) {
	case 0x0:
		buf = print_test_result(buf, end, data);
		break;
	case 0x1:
	case 0x2:
		buf += snprintf(buf, end - buf, "Test type: %s.\n\n",
				type_str(current_op));
		buf = print_test_completion(buf, end, data->current_completion);
		break;
	default:
		buf += snprintf(buf, end - buf,
				"Unknown status, Current Device Self-Test "
				"Operation: '%#x'\n",
				current_op);
		break;
	}
	return buf;
}

static int is_test_running(StorageTestLog *log)
{
	NvmeTestLogData *data = &log->data.nvme_data;
	return HALF_BYTE_LOW(data->current_operation);
}

vb2_error_t diag_dump_storage_test_log(char *buf, const char *end)
{
	BlockDev *dev = get_first_fixed_block_device();
	if (!dev || !(dev->ops.get_test_log)) {
		printf("%s: No supported.\n", __func__);
		return VB2_ERROR_EX_UNIMPLEMENTED;
	}
	StorageTestLog log = {0};

	int res = dev->ops.get_test_log(&dev->ops, &log);
	if (res) {
		buf += snprintf(buf, end - buf,
				"%s: Get Test Result error: %d\n", dev->name,
				res);
		return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
	}

	buf += snprintf(buf, end - buf, "Block device '%s':\n", dev->name);

	buf = stringify_test_status(buf, end, &log);

	if (is_test_running(&log))
		return VB2_ERROR_EX_DIAG_TEST_RUNNING;
	return VB2_SUCCESS;
}

vb2_error_t diag_storage_test_control(enum BlockDevTestOpsType ops)
{
	BlockDev *dev = get_first_fixed_block_device();
	if (!dev || !(dev->ops.test_control)) {
		printf("%s: No supported.\n", __func__);
		return VB2_ERROR_EX_UNIMPLEMENTED;
	}
	if (dev->ops.test_control(&dev->ops, ops))
		return VB2_ERROR_EX;
	get_test_remain_time_seconds(0, 1);
	return VB2_SUCCESS;
}
