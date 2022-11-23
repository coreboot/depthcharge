// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <endian.h>
#include <libpayload.h>

#include "diag/diag_internal.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"

////////////////////////////////////////////////////////////////////////////////
// NVMe health status

typedef struct {
	uint64_t lo;
	uint64_t hi;
} u128;

static inline u128 le128_to_u128(const uint8_t val[16])
{
	u128 ret;

	uint64_t *lo_ptr = (uint64_t *)val;
	uint64_t *hi_ptr = (uint64_t *)(val + 8);

	ret.lo = le64toh(*lo_ptr);
	ret.hi = le64toh(*hi_ptr);

	return ret;
}

// Round up to the multiple of base
static inline int round_up(int val, int base)
{
	int diff = base - val % base;
	return val % base ? val + diff : val;
}

// Return the upper 64 non-zero bit and provide the shift.
static uint64_t u128_to_u64(const u128 val, int *shift_ret)
{
	int shift = (val.hi >> 32) ? 64 - clz(val.hi >> 32) : 32 - clz(val.hi);

	// Round up to the multiple of 10
	shift = round_up(shift, 10);

	uint64_t value = 0;

	if (shift > 0)
		value |= val.hi << (64 - shift);
	if (shift < 64)
		value |= val.lo >> shift;

	if (shift_ret)
		*shift_ret = shift;
	return value;
}

// Stringify u128 integer with upper 64-bit precision.
static const char *u128_to_str(const u128 val)
{
	int shift;
	uint64_t value = u128_to_u64(val, &shift);

	// Calculate the padding 0, using dividing 1000 instead of 1024 as
	// approximate.
	shift = shift / 10 * 3;

	static char str[64];
	if (shift > 0)
		snprintf(str, sizeof(str), "~%" PRIu64 "%0*d", value, shift, 0);
	else
		snprintf(str, sizeof(str), "%" PRIu64, value);

	return str;
}

// Stringify u128 integer with Binary prefix (KiB, MiB...).
static const char *u128_to_capacity_str(const u128 unit_val,
					const int bytes_per_unit)
{
	const uint64_t base = 1024; // 2^10

	int shift;
	uint64_t value = u128_to_u64(unit_val, &shift);

	/* clz(x) are the amount of bits "free" in x. The goal is to get enough
	 * free bits in value to fit multiplying bytes_per_unit in without
	 * overflowing. */
	int need_adjust = log2(bytes_per_unit) + 1 - clz(value >> 32);
	if (need_adjust > 0) {
		/* round up to multiple of 10 */
		need_adjust = round_up(need_adjust, 10);
		shift += need_adjust;
		value >>= need_adjust;
	}
	value *= bytes_per_unit;

	// Compute "xxx.yyy binary_prefix".
	while (value >= base * base) {
		value /= base;
		shift += 10;
	}
	int hi, lo;
	if (value >= base) {
		hi = value / base;
		lo = ((value % base) * 1000) / base;
		shift += 10;
	} else {
		hi = value;
		lo = 0;
	}

	assert(shift % 10 == 0);

	// ISO prefixes is only defined for <= 2^80.
	static const char prefixes[9][4] = {
		"B",
		"KiB",
		"MiB",
		"GiB",
		"TiB",
		"PiB",
		"EiB",
		"ZiB",
		"YiB"
	};

	static char str[64];
	if (shift <= 80)
		snprintf(str, sizeof(str), "%d.%03d %s", hi, lo,
			 prefixes[shift / 10]);
	else
		snprintf(str, sizeof(str), "%d.%03d 2^%d %s", hi, lo, shift,
			 prefixes[0]);

	return str;
}

// Kelvin temperature value to Celsius
static inline int kelvin_to_celsius(int k) {
	return k - 273;
}

// NVMe stringify entry point
static char *stringify_nvme_smart(char *buf, const char *end,
				  const NvmeSmartLogData *smart_log)
{
	int show_all = 1;

	buf = APPEND(buf, end, "SMART/Health Information (NVMe Log 0x02)\n");
	buf = APPEND(buf, end, "Critical Warning:                   %#02x\n",
		     smart_log->critical_warning);
	buf = APPEND(buf, end,
		     "Temperature:                        %d Celsius\n",
		     kelvin_to_celsius(smart_log->temperature));
	buf = APPEND(buf, end, "Available Spare:                    %u%%\n",
		     smart_log->avail_spare);
	buf = APPEND(buf, end, "Available Spare Threshold:          %u%%\n",
		     smart_log->spare_thresh);
	buf = APPEND(buf, end, "Percentage Used:                    %u%%\n",
		     smart_log->percent_used);

	{
		// data_units_read/data_units_written is reported in thousands
		// where 1 unit = 512 bytes.
		const int bytes_per_unit = 1000 * 512;

		u128 val;

		val = le128_to_u128(smart_log->data_units_read);
		buf = APPEND(buf, end,
			     "Data Units Read:                    %s [%s]\n",
			     u128_to_str(val),
			     u128_to_capacity_str(val, bytes_per_unit));

		val = le128_to_u128(smart_log->data_units_written);
		buf = APPEND(buf, end,
			     "Data Units Written:                 %s [%s]\n",
			     u128_to_str(val),
			     u128_to_capacity_str(val, bytes_per_unit));
	}

	buf = APPEND(buf, end, "Host Read Commands:                 %s\n",
		     u128_to_str(le128_to_u128(smart_log->host_reads)));
	buf = APPEND(buf, end, "Host Write Commands:                %s\n",
		     u128_to_str(le128_to_u128(smart_log->host_writes)));
	buf = APPEND(buf, end,  "Controller Busy Time:               %s\n",
		     u128_to_str(le128_to_u128(smart_log->ctrl_busy_time)));
	buf = APPEND(buf, end, "Power Cycles:                       %s\n",
		     u128_to_str(le128_to_u128(smart_log->power_cycles)));
	buf = APPEND(buf, end, "Power On Hours:                     %s\n",
		     u128_to_str(le128_to_u128(smart_log->power_on_hours)));
	buf = APPEND(buf, end, "Unsafe Shutdowns:                   %s\n",
		     u128_to_str(le128_to_u128(smart_log->unsafe_shutdowns)));
	buf = APPEND(buf, end, "Media and Data Integrity Errors:    %s\n",
		     u128_to_str(le128_to_u128(smart_log->media_errors)));
	buf = APPEND(
		buf, end, "Error Information Log Entries:      %s\n",
		u128_to_str(le128_to_u128(smart_log->num_err_log_entries)));

	// Temperature thresholds are optional
	if (smart_log->warning_temp_time) {
		buf = APPEND(buf, end,
			     "Warning  Comp. Temperature Time:    %d\n",
			     smart_log->warning_temp_time);
	}
	if (smart_log->critical_comp_time) {
		buf = APPEND(buf, end,
			     "Critical Comp. Temperature Time:    %d\n",
			     smart_log->critical_comp_time);
	}

	// Temperature sensors are optional
	for (int i = 0; i < 8; i++) {
		int k = smart_log->temp_sensor[i];
		if (k) {
			buf = APPEND(buf, end,
				     "Temperature Sensor %d:               "
				     "%d Celsius\n",
				     i + 1, kelvin_to_celsius(k));
		}
	}
	if (show_all || smart_log->thm_temp1_trans_count)
		buf = APPEND(buf, end,
			     "Thermal Temp. 1 Transition Count:   %d\n",
			     smart_log->thm_temp1_trans_count);
	if (show_all || smart_log->thm_temp2_trans_count)
		buf = APPEND(buf, end,
			     "Thermal Temp. 2 Transition Count:   %d\n",
			     smart_log->thm_temp2_trans_count);
	if (show_all || smart_log->thm_temp1_total_time)
		buf = APPEND(buf, end,
			     "Thermal Temp. 1 Total Time:         %d\n",
			     smart_log->thm_temp1_total_time);
	if (show_all || smart_log->thm_temp2_total_time)
		buf = APPEND(buf, end,
			     "Thermal Temp. 2 Total Time:         %d\n",
			     smart_log->thm_temp2_total_time);
	return buf;
}

////////////////////////////////////////////////////////////////////////////////
// Device health report (eMMC >= 5.0 and UFS)

typedef struct {
	// Storage type name
	const char *storage_type_name;
	// Fields
	uint8_t life_time_est_a;
	uint8_t life_time_est_b;
	uint8_t eol_info;
	// Field names
	const char *life_time_est_prefix_name;
	const char *eol_info_name;
} DeviceHealthReport;

static inline char *stringify_device_lifetime(char *buf, size_t len,
					      uint8_t life_used)
{
	const char *end = buf + len;
	switch (life_used) {
	case 0x0:
		buf = APPEND(buf, end, "Not defined");
		break;
	case 0x1 ... 0xa:
		buf = APPEND(buf, end, "%d%% - %d%% device life time used",
			     (life_used - 1) * 10, life_used * 10);
		break;
	case 0xb:
		buf = APPEND(buf, end,
			     "Exceeded its maximum estimated device life time");
		break;
	default:
		buf = APPEND(buf, end, "Unknown");
		break;
	}
	return buf;
}

static inline const char *eol_info_to_str(uint8_t eol_info)
{
	switch (eol_info) {
	case 0x0:
		return "Not defined";
	case 0x1:
		return "Normal";
	case 0x2:
		return "Warning (Consumed 80% of reserved blocks)";
	case 0x3:
		return "Urgent";
	default:
		return "Unknown";
	}
}

static char *stringify_device_health_report(char *buf, const char *end,
					    const DeviceHealthReport *report)
{
	buf = APPEND(buf, end,
		     "%s Life Time Estimation A [%sA]: %#02x\n  i.e. ",
		     report->storage_type_name,
		     report->life_time_est_prefix_name,
		     report->life_time_est_a);
	buf = stringify_device_lifetime(buf, end - buf,
					report->life_time_est_a);
	buf = APPEND(buf, end, "\n");

	buf = APPEND(buf, end,
		     "%s Life Time Estimation B [%sB]: %#02x\n  i.e. ",
		     report->storage_type_name,
		     report->life_time_est_prefix_name,
		     report->life_time_est_b);
	buf = stringify_device_lifetime(buf, end - buf,
					report->life_time_est_b);
	buf = APPEND(buf, end, "\n");

	buf = APPEND(buf, end,
		     "%s Pre EOL information [%s]: %#02x\n  i.e. %s\n",
		     report->storage_type_name,
		     report->eol_info_name,
		     report->eol_info,
		     eol_info_to_str(report->eol_info));
	return buf;
}

// eMMC stringify entry point
static char *stringify_mmc_health(char *buf, const char *end,
				  const MmcHealthData *data)
{
	const char *const ver_str[] = {
		[EXT_CSD_REV_1_0] = "4.0",
		[EXT_CSD_REV_1_1] = "4.1",
		[EXT_CSD_REV_1_2] = "4.2",
		[EXT_CSD_REV_1_3] = "4.3",
		[EXT_CSD_REV_1_4] = "Obsolete",
		[EXT_CSD_REV_1_5] = "4.41",
		[EXT_CSD_REV_1_6] = "4.5",
		[EXT_CSD_REV_1_7] = "5.0",
		[EXT_CSD_REV_1_8] = "5.1",
	};

	if (data->csd_rev >= ARRAY_SIZE(ver_str) ||
	    data->csd_rev == EXT_CSD_REV_1_4) {
		buf = APPEND(buf, end, "Unsupported Extended CSD rev 1.%d\n",
			     data->csd_rev);
		return buf;
	}

	buf = APPEND(buf, end, "Extended CSD rev 1.%d (MMC %s)\n",
		     data->csd_rev, ver_str[data->csd_rev]);

	/* >= eMMC 5.0 */
	if (data->csd_rev >= EXT_CSD_REV_1_7) {
		DeviceHealthReport emmc_report = {
			.storage_type_name = "eMMC",
			.life_time_est_a = data->device_life_time_est_type_a,
			.life_time_est_b = data->device_life_time_est_type_b,
			.eol_info = data->pre_eol_info,
			.life_time_est_prefix_name =
				"EXT_CSD_DEVICE_LIFE_TIME_EST_TYPE_",
			.eol_info_name = "EXT_CSD_PRE_EOL_INFO",
		};
		buf = stringify_device_health_report(buf, end, &emmc_report);
	}

	return buf;
}

////////////////////////////////////////////////////////////////////////////////
// UFS stringify entry point
static char *stringify_ufs_health(char *buf, const char *end,
				  const UfsDescHealth *data)
{
	DeviceHealthReport ufs_report = {
		.storage_type_name = "UFS",
		.life_time_est_a = data->bDeviceLifeTimeEstA,
		.life_time_est_b = data->bDeviceLifeTimeEstB,
		.eol_info = data->bPreEOLInfo,
		.life_time_est_prefix_name = "bDeviceLifeTimeEst",
		.eol_info_name = "bPreEOLInfo",
	};
	buf = stringify_device_health_report(buf, end, &ufs_report);

	buf = APPEND(buf, end,
		     "Vendor proprietary health report "
		     "[VendorPropInfo]:\n");
	for (int i = 0; i < 32; i++) {
		buf = APPEND(buf, end, "  %#.02x", data->rsrvd[i]);
		if (i % 8 == 7)
			buf = APPEND(buf, end, "\n");
	}

	return buf;
}

////////////////////////////////////////////////////////////////////////////////
// APIs

// Append the stringified health_info to string str and return the pointer of
// next available address.
char *stringify_health_info(char *buf, const char *end, const HealthInfo *info)
{
	switch (info->type) {
	case STORAGE_INFO_TYPE_NVME:
		if (!CONFIG(DRIVER_STORAGE_NVME))
			break;
		return stringify_nvme_smart(buf, end, &info->data.nvme_data);
	case STORAGE_INFO_TYPE_MMC:
		if (!CONFIG(DRIVER_STORAGE_MMC))
			break;
		return stringify_mmc_health(buf, end, &info->data.mmc_data);
	case STORAGE_INFO_TYPE_UFS:
		if (!CONFIG(DRIVER_STORAGE_UFS))
			break;
		return stringify_ufs_health(buf, end, &info->data.ufs_data);
	case STORAGE_INFO_TYPE_UNKNOWN:
		break;
	}
	die("unsupported data type: %d\n", info->type);
	return NULL;
}

char *dump_all_health_info(char *buf, const char *end)
{
	ListNode *devs;
	int n = get_all_bdevs(BLOCKDEV_FIXED, &devs);
	if (!n) {
		buf = APPEND(buf, end, "No storage device found\n\n");
		return buf;
	}

	buf = APPEND(buf, end, "Total %d storage device%s\n\n", n,
		     n > 1 ? "s" : "");

	// Fill them from the BlockDev structures.
	BlockDev *bdev;
	int idx = 1;
	list_for_each(bdev, *devs, list_node)
	{
		if (bdev->ops.get_health_info) {
			HealthInfo info = {0};

			int res = bdev->ops.get_health_info(&bdev->ops, &info);
			if (res) {
				buf = APPEND(buf, end,
					     "%s: Get Health info error: %d\n",
					     bdev->name, res);
				continue;
			}

			buf = APPEND(buf, end, "(%d/%d) Block device '%s':\n",
				     idx, n, bdev->name);

			buf = stringify_health_info(buf, end, &info);

			if (idx < n)
				buf = APPEND(buf, end, "\n");

			idx += 1;
		} else {
			buf = APPEND(buf, end,
				     "(%d/%d) Block device '%s' does not "
				     "provide health info.\n",
				     idx, n, bdev->name);
		}
	}
	return buf;
}
