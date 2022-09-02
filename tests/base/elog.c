// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/elog.h"
#include "mocks/fmap_area.h"
#include "tests/test.h"

#include "base/elog.c"

#define ELOG_SIZE (4 * KiB)
/* Base event size is the size of (event_header + checksum) */
#define BASE_EVENT_SIZE (sizeof(struct event_header) + 1)
#define MAX_DATA_SIZE (ELOG_MAX_EVENT_SIZE - BASE_EVENT_SIZE)

/* Margin for exceeding buffer tests */
#define VALID_MARGIN 2

/* Create a larger buffer to prevent invalid memory accessing */
static uint8_t rw_elog_mirror_buf[ELOG_SIZE * 2];
size_t rw_elog_mirror_offset;
uint8_t mock_year;

static FmapArea area_rw_elog = {
	.offset = 100,
	.size = ELOG_SIZE,
	.name = ELOG_RW_REGION_NAME,
	.flags = 0,  /* Not used */
};

static FmapArea area_small_nv = {
	.offset = 100,
	.size = 1024,
	.name = ELOG_RW_REGION_NAME,
	.flags = 0,  /* Not used */
};

/* Helper functions */
static void init_rw_elog_mirror(void)
{
	memset(rw_elog_mirror_buf, 0xff, sizeof(rw_elog_mirror_buf));
	rw_elog_mirror_offset = 0;
	mock_year = 0;

	/* Write elog header to the mirror buffer */
	struct elog_header header = {
		.magic = ELOG_SIGNATURE,
		.version = ELOG_VERSION,
		.header_size = sizeof(struct elog_header),
	};
	memcpy(rw_elog_mirror_buf, &header, sizeof(struct elog_header));
	rw_elog_mirror_offset += sizeof(struct elog_header);
}

static void push_elog_event(uint8_t type, void *data, size_t data_size)
{
	uint8_t *offset = rw_elog_mirror_buf + rw_elog_mirror_offset;
	struct event_header *event = (struct event_header *)offset;
	size_t event_size = sizeof(struct event_header) + data_size + 1;
	size_t offset_limit = ELOG_SIZE;

	/*
	 * Show warning if the start offset exceeds the elog offset limit.
	 * However, it is valid to push an event with its data exceed the
	 * offset limit for some validation unit test items.
	 */
	if (mock_area)
		offset_limit = MIN(offset_limit, mock_area->size);
	if (rw_elog_mirror_offset >= offset_limit)
		print_message("%s: Push an out-of-bounds elog event; ignored\n",
			      __func__);

	struct event_header header = {
		.type = type,
		.length = event_size,
		.year = mock_year,
	};
	++mock_year;
	memcpy(offset, &header, sizeof(struct event_header));
	offset += sizeof(struct event_header);
	memcpy(offset, data, data_size);
	elog_update_checksum(event, 0);
	elog_update_checksum(event, -(elog_checksum_event(event)));
	rw_elog_mirror_offset += event_size;
}

static int setup(void **state)
{
	init_rw_elog_mirror();
	mock_flash_buf = rw_elog_mirror_buf;
	memset(&elog_state, 0, sizeof(struct elog_state));
	return 0;
}

/* Test functions */

static void test_elog_init_success(void **state)
{
	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	push_elog_event(0xa, NULL, 0);
	push_elog_event(0xa, NULL, 0);
	push_elog_event(0xa, NULL, 0);
	push_elog_event(0xa, NULL, 0);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_SUCCESS);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_INITIALIZED);
	assert_int_equal(elog_state.last_write, rw_elog_mirror_offset);
	assert_int_equal(elog_state.size, ELOG_SIZE);
}

static void test_elog_init_success_small_nv(void **state)
{
	set_mock_fmap_area(&area_small_nv, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_SUCCESS);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_INITIALIZED);
	assert_int_equal(elog_state.size, area_small_nv.size);
}

/* FmapArea initialization error */
static void test_elog_init_bad_fmap(void **state)
{
	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 1);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_EX);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
}

/* flash_read fail */
static void test_elog_init_bad_flash_read(void **state)
{
	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_FAIL);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_EX);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
}

/* The elog header is broken */
static void test_elog_init_bad_elog_header(void **state)
{
	struct elog_header *header = (struct elog_header *)rw_elog_mirror_buf;
	header->magic = 0;
	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	push_elog_event(0xa, NULL, 0);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_CONTENT);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
}

/* The event header is broken */
static void test_elog_init_bad_event_header(void **state)
{
	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	push_elog_event(0xa, NULL, 0);
	++rw_elog_mirror_buf[rw_elog_mirror_offset - 1];  /* checksum */
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_CONTENT);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
}

/* Some redundant data after the last event entry */
static void test_elog_init_data_after_last_event(void **state)
{
	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	push_elog_event(0xa, NULL, 0);
	rw_elog_mirror_buf[50] = 0;
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_CONTENT);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
}

/*
 * Pre-fill many events without data to the elog buffer.
 * The last event should have its header exceeds the offset limit. We need to
 * check that last_write is smaller than elog_state.size to prevent any
 * out-of-bounds memory access.
 */
static void test_elog_init_event_header_exceed_buffer(void **state)
{
	int valid_size, valid_event_count, i;
	uint8_t data_buf[BASE_EVENT_SIZE] = {};

	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	valid_size = ELOG_SIZE - sizeof(struct elog_header) - VALID_MARGIN;
	valid_event_count = valid_size / BASE_EVENT_SIZE;
	/* The data size of the first event should be equal as the remainder. */
	push_elog_event(0x1, data_buf, valid_size % BASE_EVENT_SIZE);
	/* Push one more event which exceeds the boundary */
	for (i = 0; i < valid_event_count; i++)
		push_elog_event(0x1, NULL, 0);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_CONTENT);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
	assert_int_equal(elog_state.size, ELOG_SIZE);
	/* last_write should be at the offset of the last valid event. */
	assert_int_equal(elog_state.last_write,
			 rw_elog_mirror_offset - BASE_EVENT_SIZE);
}

/*
 * Pre-fill many events with maximum data size to the elog buffer.
 * The last event should have its header fit into the buffer while its data
 * exceeds the offset limit. We need to check that last_write is smaller than
 * elog_state.size to prevent any out-of-bounds memory access.
 */
static void test_elog_init_event_data_exceed_buffer(void **state)
{
	int valid_size, valid_event_count, i;
	uint8_t data_buf[MAX_DATA_SIZE] = {};

	set_mock_fmap_area(&area_rw_elog, rw_elog_mirror_buf);
	will_return(fmap_find_area, 0);
	will_return_always(flash_read, MOCK_FLASH_SUCCESS);
	valid_size = ELOG_SIZE - sizeof(struct elog_header) -
		     sizeof(struct event_header) - VALID_MARGIN;
	valid_event_count = valid_size / ELOG_MAX_EVENT_SIZE;
	/* Push one more event which exceeds the boundary */
	for (i = 0; i <= valid_event_count; i++)
		push_elog_event(0x1, data_buf, MAX_DATA_SIZE);
	expect_string(fmap_find_area, name, ELOG_RW_REGION_NAME);
	assert_int_equal(elog_init(), ELOG_ERR_CONTENT);
	assert_int_equal(elog_state.elog_initialized, ELOG_INIT_BROKEN);
	assert_int_equal(elog_state.size, ELOG_SIZE);
	/* last_write should be at the offset of the last valid event. */
	assert_int_equal(elog_state.last_write,
			 rw_elog_mirror_offset - ELOG_MAX_EVENT_SIZE);
}

#define ELOG_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		ELOG_TEST(test_elog_init_success),
		ELOG_TEST(test_elog_init_success_small_nv),
		ELOG_TEST(test_elog_init_bad_fmap),
		ELOG_TEST(test_elog_init_bad_flash_read),
		ELOG_TEST(test_elog_init_bad_elog_header),
		ELOG_TEST(test_elog_init_bad_event_header),
		ELOG_TEST(test_elog_init_data_after_last_event),
		ELOG_TEST(test_elog_init_event_header_exceed_buffer),
		ELOG_TEST(test_elog_init_event_data_exceed_buffer),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
