// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2020 Google Inc.
 */

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>
#include <time.h>
#include "base/list.h"
#include "base/ranges.h"
#include "base/physmem.h"
#include "vboot/util/memory.h"

#include "diag/memory.h"
#include "diag/pattern.h"

#define DEBUG(format, args...)                                                 \
	printf("%s:%d:%s: " format, __FILE__, __LINE__, __func__, ##args)
#define OUTPUT(format, args...)                                                \
	{                                                                      \
		printf(format, ##args);                                        \
		state.buf_cur +=                                               \
			snprintf(state.buf_cur, state.buf_end - state.buf_cur, \
				 format, ##args);                              \
	}

#define DEFAULT_DIAGNOSTIC_OUTPUT_SIZE (64 * KiB)
#define PATTERN_CACHE_SIZE (1 * KiB)
#define CHUNK_SIZE (32 * MiB)

typedef enum {
	TEST_SUCCESS = 0,
	TEST_FAILED,
} TestResult;

typedef struct {
	// Cyclic memory test pattern. To Speedup via memcpy/memcmp and cache.
	uint32_t pattern[PATTERN_CACHE_SIZE];

	// Check Result
	TestResult result;
	uint64_t error_phys_addr_st;
	uint64_t error_phys_addr_ed;
	uint64_t error_phys_offset;
} OpData;

typedef struct {
	int is_running;

	// Output buffer
	char *buf;	     // The owner of output buffer.
	char *buf_cur;	     // The current write position of the output buffer.
	const char *buf_end; // The end of output buffer.

	// Test patterns
	const ListNode *patterns; // The pattern list
	int num_pattern;
	const Pattern *pattern_cur;
	// Operations
	PhysMapFunc *op;
	OpData *single_operation_data;
	// Memory ranges to be tested
	Ranges ranges;
	const RangesEdge *range_cur;
	// Chunks
	uint64_t chunk_st; // The tested chunk in the memory range.

	// Statistics
	uint64_t num_bytes;
	uint64_t num_bytes_processed;
	int percent; // The percentage of memory test progress.

	// Previous state for checking updates
	int prev_percent;
	const char *prev_pattern_name;
} MemoryTestState;

// Copy and fill dst with cyclic src.
static void cyclic(const void *_src, size_t src_size, void *_dst,
		   size_t dst_size)
{
	uint8_t *src = (uint8_t *)_src;
	uint8_t *dst = (uint8_t *)_dst;
	for (uint64_t pos = 0; pos < dst_size; pos++, dst++)
		*dst = src[pos % src_size];
}

/* Functions of PhysMapFunc to operate on memory via arch_phys_map() */
static void op_write(uint64_t phys_addr, void *start, uint64_t size,
		     void *_data)
{
	OpData *data = (OpData *)_data;

	size_t pat_size = sizeof(data->pattern);
	for (uint64_t pos = 0, cur_size; pos < size; pos += cur_size) {
		cur_size = MIN(size - pos, pat_size);
		memcpy(start + pos, data->pattern, cur_size);
	}
}

static void op_read_and_check(uint64_t phys_addr, void *start, uint64_t size,
			      void *_data)
{
	OpData *data = (OpData *)_data;

	size_t pat_size = sizeof(data->pattern);
	for (uint64_t pos = 0, cur_size; pos < size; pos += cur_size) {
		cur_size = MIN(size - pos, pat_size);

		int res = memcmp(start + pos, data->pattern, cur_size);
		if (res != 0) {
			data->result = TEST_FAILED;
			data->error_phys_addr_st = phys_addr;
			data->error_phys_addr_ed = phys_addr + size;

			// Calculate the precised unmatched memory address
			uint64_t offset = 0x0;
			uint8_t *mem = start + pos;
			uint8_t *mem_ed = start + cur_size;
			uint8_t *pat = (uint8_t *)data->pattern;
			for (; mem != mem_ed; mem++, pat++) {
				if (*mem != *pat) {
					offset = mem - (uint8_t *)start;
					break;
				}
			}
			data->error_phys_offset = offset;
			return;
		}
	}
}

PhysMapFunc ops[] = {op_write, op_read_and_check, NULL};

static MemoryTestState state;

static inline uint64_t get_current_chunk_end(void)
{
	return MIN(state.range_cur->next->pos, state.chunk_st + CHUNK_SIZE);
}

static inline void reset_chunk(void)
{
	state.chunk_st = state.range_cur->pos;
}

static inline void reset_range(void)
{
	state.range_cur = state.ranges.head.next;
	reset_chunk();
}

static inline void reset_operation(void)
{
	state.op = ops;
	reset_range();
}

static inline void fill_opdata(void)
{
	const void *src = state.pattern_cur->data;
	size_t src_size =
		sizeof(*state.pattern_cur->data) * state.pattern_cur->len;
	cyclic(src, src_size, state.single_operation_data->pattern,
	       sizeof(state.single_operation_data->pattern));
}

static inline void reset_memory_test(void)
{
	state.buf_cur = state.buf;

	state.pattern_cur = container_of(state.patterns->next,
					 typeof(*state.pattern_cur), list_node);
	fill_opdata();

	reset_operation();

	state.num_bytes_processed = 0;

	state.is_running = 1;
}

static inline void go_next_pattern(void)
{
	ListNode *next = state.pattern_cur->list_node.next;

	// If no next pattern, then we are done.
	if (!next) {
		state.is_running = 0;
		DEBUG("No more patterns. We are done!\n");
		return;
	}

	state.pattern_cur =
		container_of(next, typeof(*state.pattern_cur), list_node);
	fill_opdata();
	reset_operation();
}

static inline void go_next_operation(void)
{
	state.op++;
	if (!*state.op)
		go_next_pattern();
	reset_range();
}

static inline void go_next_range(void)
{
	state.range_cur = state.range_cur->next->next;
	if (!state.range_cur)
		go_next_operation();
	reset_chunk();
}

static inline void go_next_chunk(void)
{
	state.chunk_st = get_current_chunk_end();
	// If we reach the end of current range, go to next one.
	if (state.chunk_st == state.range_cur->next->pos)
		go_next_range();
}

static void update_progress(void)
{
	int operations = ARRAY_SIZE(ops) - 1;
	state.percent = 100LL * state.num_bytes_processed /
			(state.num_pattern * operations * state.num_bytes);
}

vb2_error_t memory_test_init(MemoryTestMode mode)
{
	DEBUG("Initialize for memory test with mode: %d\n", mode);

	// Output buffer initialization
	if (!state.buf) {
		state.buf = malloc(DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
		if (!state.buf) {
			DEBUG("Fail to allocate memory for output buffer.\n");
			return VB2_ERROR_UI_MEMORY_ALLOC;
		}
		state.buf_end = state.buf + DEFAULT_DIAGNOSTIC_OUTPUT_SIZE;
	}

	// Test pattern initialization
	switch (mode) {
	case MEMORY_TEST_MODE_QUICK:
		state.patterns = DiagGetSimpleTestPatterns();
		break;
	case MEMORY_TEST_MODE_FULL:
		state.patterns = DiagGetTestPatterns();
		break;
	default:
		DEBUG("Unknown test mode: %d\n", mode);
		return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
	}
	if (!state.patterns) {
		DEBUG("Fail to get pattern list for memory test.\n");
		return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
	}
	const Pattern *pattern;
	state.num_pattern = 0;
	list_for_each(pattern, *state.patterns, list_node)
	{
		state.num_pattern += 1;
	}

	// Operations initialization
	if (!state.single_operation_data) {
		state.single_operation_data =
			malloc(sizeof(*state.single_operation_data));
		if (!state.single_operation_data) {
			DEBUG("Fail to allocate memory for operation data.\n");
			return VB2_ERROR_UI_MEMORY_ALLOC;
		}
	}
	memset(state.single_operation_data, 0,
	       sizeof(*state.single_operation_data));
	state.single_operation_data->result = TEST_SUCCESS;

	// Memory range initialization
	if (!state.ranges.head.next) {
		if (memory_range_init_and_get_unused(&state.ranges)) {
			DEBUG("Fail to get unused memory range.\n");
			return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
		}
	}

	// Calculate how many memory segments should be tested.
	state.num_bytes = 0;
	for (RangesEdge *cur = state.ranges.head.next; cur != NULL;
	     cur = cur->next->next) {
		if (!cur->next) {
			DEBUG("Odd number of range edges!\n");
			return VB2_ERROR_EX_DIAG_TEST_INIT_FAILED;
		}
		state.num_bytes += cur->next->pos - cur->pos;
	}

	reset_memory_test();

	// Clear previous state.
	state.prev_percent = -1;
	state.prev_pattern_name = NULL;

	OUTPUT("This test may take a few minutes\n\n");

	OUTPUT("Free memory (will be tested): %lld.%03lld GiB\n",
	       state.num_bytes / GiB, (1000 * state.num_bytes / GiB) % 1000);
	OUTPUT("Loaded test patterns:");
	list_for_each(pattern, *state.patterns, list_node)
	{
		OUTPUT(" '%s'", pattern->name);
	}
	OUTPUT("\n\n");

	return VB2_SUCCESS;
}

/*
 * The memory test logic is:
 *   for each pattern,
 *     for each operation,
 *       for each memery range,
 *         for each chunk in memory range,
 *           do the operation on that chunk.
 * However, we would like to process a operation on a chunk for each call, so
 * we will store the state of each loop, and run a chunk each time.
 */
static void memory_test_run_step(void)
{
	if (!state.is_running)
		return;

	uint64_t start = state.chunk_st;
	uint64_t end = get_current_chunk_end();
	uint64_t size = end - start;

	// TODO(menghuan): Add intergration test hook here.

	uint64_t start_time = timer_us(0);
	arch_phys_map(start, size, *state.op, state.single_operation_data);
	uint64_t delta_us = timer_us(start_time);

	state.num_bytes_processed += size;
	update_progress();

	// TODO(menghuan): Remove the test speed estimate here.
	DEBUG("%15s: [%#016llx, %#016llx): %lld ms (%lld bytes/us) ... "
	       "(%d%%)\n",
	       state.pattern_cur->name, start, end, delta_us / 1000,
	       size / delta_us, state.percent);

	if (state.single_operation_data->result != TEST_SUCCESS) {
		OUTPUT("\n"
		       "Memory test failed:\n"
		       "    Pattern '%s' failed at %#016llx\n"
		       "    in memory segment [%#016llx, %#016llx).\n",
		       state.pattern_cur->name,
		       state.single_operation_data->error_phys_addr_st +
			       state.single_operation_data->error_phys_offset,
		       state.single_operation_data->error_phys_addr_st,
		       state.single_operation_data->error_phys_addr_ed);
		state.is_running = 0;
		return;
	}

	go_next_chunk();

	if (!state.is_running)
		OUTPUT("\nAll memory tests passed.\n");
}

static inline vb2_error_t memory_test_run_impl(const char **out)
{
	*out = state.buf;

	memory_test_run_step();

	if (!state.is_running)
		return VB2_SUCCESS;

	// No updates on the out string.
	if (state.prev_percent == state.percent &&
	    state.prev_pattern_name == state.pattern_cur->name)
		return VB2_ERROR_EX_DIAG_TEST_RUNNING;

	// Append the progress bar. Do not change buf_cur, we expect this string
	// will be overwritten in next round.
	snprintf(state.buf_cur, state.buf_end - state.buf_cur,
		 "\n"
		 "%3d%% completed ... Running pattern '%s' ...\n",
		 state.percent, state.pattern_cur->name);

	state.prev_percent = state.percent;
	state.prev_pattern_name = state.pattern_cur->name;

	return VB2_ERROR_EX_DIAG_TEST_UPDATED;
}

vb2_error_t memory_test_run(const char **out)
{
	// Previous chunk timestemp.
	static uint64_t prev_end = 0;
	uint64_t start = timer_us(0);

	vb2_error_t rv = memory_test_run_impl(out);

	uint64_t end = timer_us(0);
	DEBUG("UI Delay: %lld ms, Mem test cost: %lld ms\n",
	      (start - prev_end) / 1000, (end - start) / 1000);

	prev_end = end;
	return rv;
}
