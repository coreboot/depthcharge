/*
 * Copyright 2012 Google Inc.  All rights reserved.
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

uint64_t get_us_since_boot(void)
{
	if(!ts_table)
		return timer_us(0);

	uint64_t tick_freq_mhz = ts_table->tick_freq_mhz;
	if (!tick_freq_mhz)
		tick_freq_mhz = timer_hz() / 1000000;

	if (!tick_freq_mhz)
		tick_freq_mhz = 1;

	return timer_us(0) - ts_table->base_time / tick_freq_mhz;
}

uint64_t get_us_since_pre_cpu_reset(void)
{
	if(!ts_table)
		return timer_us(0);

	uint64_t tick_freq_mhz = ts_table->tick_freq_mhz;
	if (!tick_freq_mhz)
		tick_freq_mhz = timer_hz() / 1000000;

	if (!tick_freq_mhz)
		tick_freq_mhz = 1;

	int64_t earliest_stamp = 0;
	for (uint32_t i = 0; i < ts_table->num_entries; i++) {
		if (ts_table->entries[i].entry_stamp < earliest_stamp)
			earliest_stamp = ts_table->entries[i].entry_stamp;
	}

	if (CONFIG(TIMESTAMP_RAW))
		return (timer_raw_value() - ts_table->base_time - earliest_stamp) /
									tick_freq_mhz;

	return timer_us(0) - (ts_table->base_time - earliest_stamp) / tick_freq_mhz;
}

int timestamp_tick_freq_mhz(void)
{
	if (ts_table)
		return ts_table->tick_freq_mhz;

	return 0;
}
