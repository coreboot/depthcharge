// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "diag/health_info.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"
#include "mocks/util/commonparams.h"
#include "tests/test.h"

#define DIAG_BUFFER_SIZE (64 * KiB)

static char info_buf[DIAG_BUFFER_SIZE];
static const char empty_buf[DIAG_BUFFER_SIZE];

static int setup(void **state)
{
	memset(info_buf, 0, sizeof(info_buf));
	reset_mock_workbuf = 1;
	return 0;
}

/* Test functions */

static void test_health_info_small_buffer(void **state)
{
	HealthInfo info = {
		.type = STORAGE_INFO_TYPE_MMC,
		.data.mmc_data = {
			.csd_rev = EXT_CSD_REV_1_1,  /* MMC v4.1 */
			.device_life_time_est_type_a = 0x0,  /* Ignored */
			.device_life_time_est_type_b = 0x0,  /* Ignored */
			.pre_eol_info = 0x0,  /* Ignored */
		},
	};
	char *buf = info_buf;
	char *end = buf + 1;
	assert_ptr_equal(stringify_health_info(buf, end, &info), end);
	assert_memory_equal(info_buf, empty_buf, DIAG_BUFFER_SIZE);
}

static void test_health_info_emmc_old_version(void **state)
{
	HealthInfo info = {
		.type = STORAGE_INFO_TYPE_MMC,
		.data.mmc_data = {
			.csd_rev = EXT_CSD_REV_1_1,  /* MMC v4.1 */
			.device_life_time_est_type_a = 0x3,  /* Ignored */
			.device_life_time_est_type_b = 0x4,  /* Ignored */
			.pre_eol_info = 0x0,  /* Ignored */
		},
	};
	char *buf = info_buf;
	assert_ptr_not_equal(
		stringify_health_info(buf, buf + DIAG_BUFFER_SIZE, &info),
		info_buf);
	assert_string_equal(
		info_buf,
		"Extended CSD rev 1.1 (MMC 4.1)\n");
}

static void test_health_info_emmc(void **state)
{
	HealthInfo info = {
		.type = STORAGE_INFO_TYPE_MMC,
		.data.mmc_data = {
			.csd_rev = EXT_CSD_REV_1_7,  /* MMC v5.0 */
			.device_life_time_est_type_a = 0x0,  /* Not defined */
			.device_life_time_est_type_b = 0x2,  /* 10% - 20% */
			.pre_eol_info = 0x1,  /* Normal */
		},
	};
	char *buf = info_buf;
	assert_ptr_not_equal(
		stringify_health_info(buf, buf + DIAG_BUFFER_SIZE, &info),
		info_buf);
	assert_string_equal(
		info_buf,
		"Extended CSD rev 1.7 (MMC 5.0)\n"
		"eMMC Life Time Estimation A "
		"[EXT_CSD_DEVICE_LIFE_TIME_EST_TYPE_A]: 0x0\n"
		"  i.e. Not defined\n"
		"eMMC Life Time Estimation B "
		"[EXT_CSD_DEVICE_LIFE_TIME_EST_TYPE_B]: 0x2\n"
		"  i.e. 10% - 20% device life time used\n"
		"eMMC Pre EOL information [EXT_CSD_PRE_EOL_INFO]: 0x1\n"
		"  i.e. Normal\n");
}

static void test_health_info_nvme(void **state)
{
	HealthInfo info = {
		.type = STORAGE_INFO_TYPE_NVME,
		.data.nvme_data = {
			.critical_warning = 0x0,
			.temperature = 45 + 273,  /* 45 Celsius */
			.avail_spare = 100,
			.spare_thresh = 10,
			.percent_used = 2,
			.data_units_read = {
				0x94, 0xbb, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},  /* LE128, 0x2cbb94 = 2931604 = 1.364 TiB */
			.data_units_written = {
				0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},  /* LE128, 0x101 = 257 = 125.488 MiB */
			.host_reads = {
				0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.host_writes = {
				0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.ctrl_busy_time = {
				0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.power_cycles = {
				0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.power_on_hours = {
				0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.unsafe_shutdowns = {
				0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.media_errors = {
				0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.num_err_log_entries = {
				0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
			.warning_temp_time = 0x9,
			/* critical_comp_time is an optional field */
			.temp_sensor = {56 + 273, 0, 0, 0, 0, 0, 0, 0},
			.thm_temp1_trans_count = 9,
			.thm_temp2_trans_count = 10,
			.thm_temp1_total_time = 11,
			.thm_temp2_total_time = 12,
		},
	};
	char *buf = info_buf;
	assert_ptr_not_equal(
		stringify_health_info(buf, buf + DIAG_BUFFER_SIZE, &info),
		info_buf);
	assert_string_equal(
		info_buf,
		"SMART/Health Information (NVMe Log 0x02)\n"
		"Critical Warning:                   0x0\n"
		"Temperature:                        45 Celsius\n"
		"Available Spare:                    100%\n"
		"Available Spare Threshold:          10%\n"
		"Percentage Used:                    2%\n"
		"Data Units Read:                    2931604 [1.364 TiB]\n"
		"Data Units Written:                 257 [125.488 MiB]\n"
		"Host Read Commands:                 1\n"
		"Host Write Commands:                2\n"
		"Controller Busy Time:               3\n"
		"Power Cycles:                       4\n"
		"Power On Hours:                     5\n"
		"Unsafe Shutdowns:                   6\n"
		"Media and Data Integrity Errors:    7\n"
		"Error Information Log Entries:      8\n"
		"Warning  Comp. Temperature Time:    9\n"
		"Temperature Sensor 1:               56 Celsius\n"
		"Thermal Temp. 1 Transition Count:   9\n"
		"Thermal Temp. 2 Transition Count:   10\n"
		"Thermal Temp. 1 Total Time:         11\n"
		"Thermal Temp. 2 Total Time:         12\n");
}

static void health_info_ufs(void **state)
{
	HealthInfo info = {
		.type = STORAGE_INFO_TYPE_UFS,
		.data.ufs_data = {
			.bLength = 0x25,
			.bDescriptorIDN = 0x9,
			.bPreEOLInfo = 0x1,
			.bDeviceLifeTimeEstA = 0x2,
			.bDeviceLifeTimeEstB = 0x3,
			.rsrvd = {
				0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
				0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			},
		},
	};
	char *buf = info_buf;
	assert_ptr_not_equal(
		stringify_health_info(buf, buf + DIAG_BUFFER_SIZE, &info),
		info_buf);
	assert_string_equal(
		info_buf,
		"UFS Life Time Estimation A [bDeviceLifeTimeEstA]: 0x2\n"
		"  i.e. 10% - 20% device life time used\n"
		"UFS Life Time Estimation B [bDeviceLifeTimeEstB]: 0x3\n"
		"  i.e. 20% - 30% device life time used\n"
		"UFS Pre EOL information [bPreEOLInfo]: 0x1\n"
		"  i.e. Normal\n"
		"Vendor proprietary health report [VendorPropInfo]:\n"
		"  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07\n"
		"  0x08  0x09  0x0a  0x0b  0x0c  0x0d  0x0e  0x0f\n"
		"  0x10  0x11  0x12  0x13  0x14  0x15  0x16  0x17\n"
		"  0x00  0x00  0x00  0x00  0x00  0x00  0x00  0x00\n");
}

#define HEALTH_INFO_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		HEALTH_INFO_TEST(test_health_info_small_buffer),
		HEALTH_INFO_TEST(test_health_info_emmc_old_version),
		HEALTH_INFO_TEST(test_health_info_emmc),
		HEALTH_INFO_TEST(test_health_info_nvme),
		HEALTH_INFO_TEST(health_info_ufs),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
