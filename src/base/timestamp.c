/*
 * Copyright 2012 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/timestamp.h"

struct timestamp_entry {
	uint32_t	entry_id;
	int64_t		entry_stamp;
} __attribute__((packed));

struct timestamp_table {
	uint64_t	base_time;
	uint16_t	max_entries;
	uint16_t	tick_freq_mhz;
	uint32_t	num_entries;
	struct timestamp_entry entries[0]; /* Variable number of entries */
} __attribute__((packed));

static struct timestamp_table *ts_table;

void timestamp_init(void)
{
	ts_table = phys_to_virt(lib_sysinfo.tstamp_table);
	timestamp_add_now(TS_START);
}

void timestamp_add(enum timestamp_id id, uint64_t ts_time)
{
	struct timestamp_entry *tse;

	if (!ts_table || (ts_table->num_entries == ts_table->max_entries))
		return;

	tse = &ts_table->entries[ts_table->num_entries++];
	tse->entry_id = id;
	tse->entry_stamp = ts_time - ts_table->base_time;
}

void timestamp_add_now(enum timestamp_id id)
{
	if (CONFIG(TIMESTAMP_RAW))
		timestamp_add(id, timer_raw_value());
	else
		timestamp_add(id, timer_us(0));
}

void timestamp_mix_in_randomness(u8 *buffer, size_t size)
{
	assert(ts_table);
	for (size_t i = 0; i < size && i < ts_table->num_entries; i++)
		buffer[i] ^= (u8)ts_table->entries[i].entry_stamp;
}

static uint64_t get_tick_freq_mhz(void)
{
	uint64_t tick_freq_mhz = ts_table ? ts_table->tick_freq_mhz : 0;

	if (!tick_freq_mhz)
		tick_freq_mhz = timer_hz() / 1000000;

	if (!tick_freq_mhz)
		tick_freq_mhz = 1;

	return tick_freq_mhz;
}

uint64_t get_us_since_boot(void)
{
	if (!ts_table)
		return timer_us(0);

	uint64_t tick_freq_mhz = get_tick_freq_mhz();

	if (CONFIG(TIMESTAMP_RAW))
		return (timer_raw_value() - ts_table->base_time) / tick_freq_mhz;
	return timer_us(0) - ts_table->base_time / tick_freq_mhz;
}

/*
 * Retrieves the pre-cpu timestamp recorded in the system's timestamp table.
 *
 * This function iterates through all entries in the `ts_table` and identifies
 * the smallest (most negative or earliest) `entry_stamp` value. This value
 * often serves as a reference point, such as an indicator of a pre-CPU reset
 * event or the earliest recorded activity in the system's lifetime from a
 * specific measurement perspective.
 */
static int64_t get_pre_cpu_reset_timestamp(void)
{
	static int64_t earliest_stamp = INT64_MAX;

	if (earliest_stamp != INT64_MAX)
		return earliest_stamp;

	earliest_stamp = 0;
	for (uint32_t i = 0; i < ts_table->num_entries; i++) {
		if (ts_table->entries[i].entry_stamp < earliest_stamp)
			earliest_stamp = ts_table->entries[i].entry_stamp;
	}

	return earliest_stamp;
}

uint64_t get_us_since_pre_cpu_reset(void)
{
	if (!ts_table)
		return timer_us(0);

	uint64_t tick_freq_mhz = get_tick_freq_mhz();
	uint64_t current_raw_value;

	if (CONFIG(TIMESTAMP_RAW))
		current_raw_value = timer_raw_value();
	else
		current_raw_value = timer_us(0) * tick_freq_mhz;

	return (current_raw_value - ts_table->base_time - get_pre_cpu_reset_timestamp()) /
			 tick_freq_mhz;
}

int timestamp_tick_freq_mhz(void)
{
	if (ts_table)
		return ts_table->tick_freq_mhz;

	return 0;
}

static int64_t timestamp_entry_stamp(enum timestamp_id id)
{
	for (uint32_t i = 0; i < ts_table->num_entries; i++) {
		if (ts_table->entries[i].entry_id == id)
			return ts_table->entries[i].entry_stamp;
	}

	return 0;
}

static int64_t timestamp_entry_stamp_since_pre_cpu_reset(enum timestamp_id id)
{
	return timestamp_entry_stamp(id) - get_pre_cpu_reset_timestamp();
}

/*
 * Calculates the time elapsed since boot to reach a specific timestamp ID.
 *
 * This function retrieves the raw timestamp count for a given `timestamp_id`
 * and converts it into microseconds. The elapsed time is calculated by dividing
 * the raw timestamp count by the system's timestamp tick frequency in MHz.
 *
 */
uint64_t get_us_timestamp_at_index(enum timestamp_id id)
{
	if (!ts_table)
		return 0;

	return timestamp_entry_stamp(id) / timestamp_tick_freq_mhz();
}

/*
 * Calculates the time elapsed since boot (including pre-cpu reset) to reach a
 * specific timestamp ID.
 *
 * This function retrieves the raw timestamp count for a given `timestamp_id`
 * and converts it into microseconds. The elapsed time is calculated by dividing
 * the raw timestamp count by the system's timestamp tick frequency in MHz.
 *
 */
uint64_t get_us_timestamp_since_pre_reset_at_index(enum timestamp_id id)
{
	if (!ts_table)
		return 0;

	return timestamp_entry_stamp_since_pre_cpu_reset(id) / timestamp_tick_freq_mhz();
}
