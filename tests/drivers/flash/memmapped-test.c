// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <string.h>

#include "drivers/flash/flash.h"
#include "tests/test.h"

#include "drivers/flash/memmapped.c"

struct sysinfo_t lib_sysinfo;
FlashOps *test_flash_ops;

static int setup(void **state)
{
	memset(&lib_sysinfo, 0, sizeof(lib_sysinfo));
	test_flash_ops = NULL;
	return 0;
}

/* Mock functions */

void flash_set_ops(FlashOps *ops)
{
	test_flash_ops = ops;
}

/* Tests */

static int test_flash_read(void *buffer, uint32_t offset, uint32_t size)
{
	return flash_read_ops(test_flash_ops, buffer, offset, size);
}

static void test_mmap_flash_without_mmap_table(void **state)
{
	lib_sysinfo.spi_flash.mmap_window_count = 0;

	expect_die(flash_setup());
}

static void test_mmap_flash_read_fail(void **state)
{
	lib_sysinfo.spi_flash.mmap_table[0] = (struct flash_mmap_window) {
		.flash_base = 0x1000,
		.host_base = 0x2000,
		.size = 0x100,
	};
	lib_sysinfo.spi_flash.mmap_window_count = 1;

	assert_int_equal(flash_setup(), 0);

	uint8_t buffer[0x100] = {0};
	assert_int_less_than(test_flash_read(buffer, 0x3000, 0x100), 0);
}

static void test_mmap_flash_read_ok(void **state)
{
	static uint8_t data[0x100] = {0};
	uint8_t expected_buffer[] = {1, 2, 3};
	memcpy(&data[0x80], expected_buffer, sizeof(expected_buffer));

	/* TODO: Make sure the address of "data" fits in uint32_t */
	if ((uintptr_t)data > UINT32_MAX)
		skip();

	lib_sysinfo.spi_flash.mmap_table[0] = (struct flash_mmap_window) {
		.flash_base = 0x1000,
		.host_base = (uintptr_t)data,
		.size = 0x100,
	};
	lib_sysinfo.spi_flash.mmap_window_count = 1;

	assert_int_equal(flash_setup(), 0);

	uint8_t buffer[sizeof(expected_buffer)] = {0};
	assert_int_equal(test_flash_read(buffer, 0x1000 + 0x80, sizeof(buffer)),
			 sizeof(buffer));
	assert_int_equal(memcmp(buffer, expected_buffer, sizeof(buffer)), 0);
}

#define FLASH_TEST(func) cmocka_unit_test_setup(func, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		FLASH_TEST(test_mmap_flash_without_mmap_table),
		FLASH_TEST(test_mmap_flash_read_fail),
		FLASH_TEST(test_mmap_flash_read_ok),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
