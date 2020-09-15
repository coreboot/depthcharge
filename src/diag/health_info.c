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

#include "drivers/storage/blockdev.h"
#include "drivers/storage/health.h"

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

static char *stringify_nvme_smart(char *buf, const char *end,
				  const NVME_SMART_LOG_DATA *smart_log)
{
	int show_all = 1;

	buf += snprintf(buf, end - buf,
			"SMART/Health Information (NVMe Log 0x02)\n");
	buf += snprintf(buf, end - buf,
			"Critical Warning:                   0x%02x\n",
			smart_log->critical_warning);
	buf += snprintf(buf, end - buf,
			"Temperature:                        %d Celsius\n",
			kelvin_to_celsius(smart_log->temperature));
	buf += snprintf(buf, end - buf,
			"Available Spare:                    %u%%\n",
			smart_log->avail_spare);
	buf += snprintf(buf, end - buf,
			"Available Spare Threshold:          %u%%\n",
			smart_log->spare_thresh);
	buf += snprintf(buf, end - buf,
			"Percentage Used:                    %u%%\n",
			smart_log->percent_used);

	{
		// data_units_read/data_units_written is reported in thousands
		// where 1 unit = 512 bytes.
		const int bytes_per_unit = 1000 * 512;

		u128 val;

		val = le128_to_u128(smart_log->data_units_read);
		buf += snprintf(buf, end - buf,
				"Data Units Read:                    %s [%s]\n",
				u128_to_str(val),
				u128_to_capacity_str(val, bytes_per_unit));

		val = le128_to_u128(smart_log->data_units_written);
		buf += snprintf(buf, end - buf,
				"Data Units Written:                 %s [%s]\n",
				u128_to_str(val),
				u128_to_capacity_str(val, bytes_per_unit));
	}

	buf += snprintf(buf, end - buf,
			"Host Read Commands:                 %s\n",
			u128_to_str(le128_to_u128(smart_log->host_reads)));
	buf += snprintf(buf, end - buf,
			"Host Write Commands:                %s\n",
			u128_to_str(le128_to_u128(smart_log->host_writes)));
	buf += snprintf(buf, end - buf,
			"Controller Busy Time:               %s\n",
			u128_to_str(le128_to_u128(smart_log->ctrl_busy_time)));
	buf += snprintf(buf, end - buf,
			"Power Cycles:                       %s\n",
			u128_to_str(le128_to_u128(smart_log->power_cycles)));
	buf += snprintf(buf, end - buf,
			"Power On Hours:                     %s\n",
			u128_to_str(le128_to_u128(smart_log->power_on_hours)));
	buf += snprintf(
		buf, end - buf, "Unsafe Shutdowns:                   %s\n",
		u128_to_str(le128_to_u128(smart_log->unsafe_shutdowns)));
	buf += snprintf(buf, end - buf,
			"Media and Data Integrity Errors:    %s\n",
			u128_to_str(le128_to_u128(smart_log->media_errors)));
	buf += snprintf(
		buf, end - buf, "Error Information Log Entries:      %s\n",
		u128_to_str(le128_to_u128(smart_log->num_err_log_entries)));

	// Temperature thresholds are optional
	if (smart_log->warning_temp_time) {
		buf += snprintf(buf, end - buf,
				"Warning  Comp. Temperature Time:    %d\n",
				smart_log->warning_temp_time);
	}
	if (smart_log->critical_comp_time) {
		buf += snprintf(buf, end - buf,
				"Critical Comp. Temperature Time:    %d\n",
				smart_log->critical_comp_time);
	}

	// Temperature sensors are optional
	for (int i = 0; i < 8; i++) {
		int k = smart_log->temp_sensor[i];
		if (k) {
			buf += snprintf(buf, end - buf,
					"Temperature Sensor %d:               "
					"%d Celsius\n",
					i + 1, kelvin_to_celsius(k));
		}
	}
	if (show_all || smart_log->thm_temp1_trans_count)
		buf += snprintf(buf, end - buf,
				"Thermal Temp. 1 Transition Count:   %d\n",
				smart_log->thm_temp1_trans_count);
	if (show_all || smart_log->thm_temp2_trans_count)
		buf += snprintf(buf, end - buf,
				"Thermal Temp. 2 Transition Count:   %d\n",
				smart_log->thm_temp2_trans_count);
	if (show_all || smart_log->thm_temp1_total_time)
		buf += snprintf(buf, end - buf,
				"Thermal Temp. 1 Total Time:         %d\n",
				smart_log->thm_temp1_total_time);
	if (show_all || smart_log->thm_temp2_total_time)
		buf += snprintf(buf, end - buf,
				"Thermal Temp. 2 Total Time:         %d\n",
				smart_log->thm_temp2_total_time);
	return buf;
}

// Append the stringified health_info to string str and return the pointer of
// next available address.
char *stringify_health_info(char *buf, const char *end, const HealthInfo *info)
{
	switch (info->type) {
#ifdef CONFIG_DRIVER_STORAGE_NVME
	case HEALTH_NVME:
		return stringify_nvme_smart(buf, end, &info->data.nvme_data);
#endif
	case HEALTH_UNKNOWN:
	default:
		printf("%s: stringify a unknown data.", __func__);
		assert(0);
		return NULL;
	}
}

char *dump_all_health_info(char *buf, const char *end)
{
	ListNode *devs;
	int n = get_all_bdevs(BLOCKDEV_FIXED, &devs);
	if (!n) {
		buf += snprintf(buf, end - buf, "No storage device found.\n\n");
		return buf;
	}

	buf += snprintf(buf, end - buf, "Total %d storage device%s.\n\n", n,
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
				buf += snprintf(
					buf, end - buf,
					"%s: Get Health info error: %d\n",
					bdev->name, res);
				continue;
			}

			buf += snprintf(buf, end - buf,
					"(%d/%d) Block device '%s':\n", idx, n,
					bdev->name);

			buf = stringify_health_info(buf, end, &info);

			if (idx < n)
				buf += snprintf(buf, end - buf, "\n");

			idx += 1;
		} else {
			buf += snprintf(buf, end - buf,
					"(%d/%d) Block device '%s' does not "
					"provide health info.\n",
					idx, n, bdev->name);
		}
	}
	return buf;
}
