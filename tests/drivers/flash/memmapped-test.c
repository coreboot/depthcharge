// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <string.h>

#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "tests/test.h"

struct sysinfo_t lib_sysinfo;

static int setup(void **state)
{
	memset(&lib_sysinfo, 0, sizeof(lib_sysinfo));
	return 0;
}

/* Mocks */

#define HOST_DATA_SIZE 0x10

static const uint8_t *get_host_data(void)
{
	static const uint8_t host_data[HOST_DATA_SIZE] = {
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
	};

	/* TODO: Make sure the address fits in uint32_t */
	if ((uintptr_t)host_data > UINT32_MAX)
		skip();

	return host_data;
}

static int mock_read(struct FlashOps *me, void *buffer, uint32_t offset,
		     uint32_t size)
{
	check_expected(offset);
	check_expected(size);
	return mock();
}

static int mock_write(struct FlashOps *me, const void *buffer, uint32_t offset,
		      uint32_t size)
{
	check_expected(offset);
	check_expected(size);
	return mock();
}

static int mock_erase(struct FlashOps *me, uint32_t offset, uint32_t size)
{
	check_expected(offset);
	check_expected(size);
	return mock();
}

static FlashOps mock_ops = {
	.read = mock_read,
	.write = mock_write,
	.erase = mock_erase,
};

/* Test utilities */

static void add_mmap_window(uint32_t flash_base, uint32_t host_base,
			    uint32_t size)
{
	uint32_t i = lib_sysinfo.spi_flash.mmap_window_count;

	if (i >= SYSINFO_MAX_MMAP_WINDOWS)
		fail_msg("number of mmap windows exceeds %d",
			 SYSINFO_MAX_MMAP_WINDOWS);

	lib_sysinfo.spi_flash.mmap_table[i] = (struct flash_mmap_window) {
		.flash_base = flash_base,
		.host_base = host_base,
		.size = size,
	};
	lib_sysinfo.spi_flash.mmap_window_count++;
}

/* Tests */

static void test_mmap_flash_without_mmap_table(void **state)
{
	lib_sysinfo.spi_flash.mmap_window_count = 0;

	expect_die(new_mmap_flash());
}

static void test_mmap_flash_read_fail(void **state)
{
	add_mmap_window(0x1000, 0x2000, 0x100);

	MmapFlash *flash = new_mmap_flash();
	assert_non_null(flash);

	uint8_t buffer[0x100] = {0};
	assert_int_less_than(flash_read_ops(&flash->ops, buffer, 0x3000, 0x100),
			     0);
}

static void test_mmap_flash_read_ok(void **state)
{

	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_flash();
	assert_non_null(flash);

	uint8_t buffer[HOST_DATA_SIZE] = {0};
	int size = HOST_DATA_SIZE - 0x2;
	assert_int_equal(flash_read_ops(&flash->ops, buffer, 0x1000 + 0x2,
					size),
			 size);
	assert_int_equal(memcmp(buffer, &host_data[0x2], size), 0);
}

static void test_mmap_flash_write_not_supported(void **state)
{
	add_mmap_window(0x1000, 0x2000, 0x100);

	MmapFlash *flash = new_mmap_backed_flash(NULL);
	assert_non_null(flash);

	uint8_t buffer[0x100] = {0};
	assert_int_equal(flash_write_ops(&flash->ops, buffer, 0x1000, 0x100),
			 0);
}

static void test_mmap_backed_flash_read_by_mmap(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint8_t buffer[HOST_DATA_SIZE] = {0};
	int size = HOST_DATA_SIZE - 0x2;
	assert_int_equal(flash_read_ops(&flash->ops, buffer, 0x1000 + 0x2,
					size),
			 size);
	assert_int_equal(memcmp(buffer, &host_data[0x2], size), 0);
}

static void test_mmap_backed_flash_read_by_base_ops(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint8_t buffer[0x100] = {0};
	expect_value(mock_read, offset, 0x90);
	expect_value(mock_read, size, sizeof(buffer));
	will_return(mock_read, sizeof(buffer));
	assert_int_equal(flash_read_ops(&flash->ops, buffer, 0x90,
					sizeof(buffer)),
			 sizeof(buffer));
}

static void test_mmap_backed_flash_read_fail_by_base_ops(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint8_t buffer[0x100] = {0};
	expect_value(mock_read, offset, 0x90);
	expect_value(mock_read, size, sizeof(buffer));
	will_return(mock_read, -2);
	assert_int_equal(flash_read_ops(&flash->ops, buffer, 0x90,
					sizeof(buffer)),
			 -2);
}

static void test_mmap_backed_flash_write_by_base_ops(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint8_t buffer[0x100] = {0};
	expect_value(mock_write, offset, 0x90);
	expect_value(mock_write, size, sizeof(buffer));
	will_return(mock_write, sizeof(buffer));
	assert_int_equal(flash_write_ops(&flash->ops, buffer, 0x90,
					sizeof(buffer)),
			 sizeof(buffer));
}

static void test_mmap_backed_flash_write_fail_by_base_ops(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint8_t buffer[0x100] = {0};
	expect_value(mock_write, offset, 0x90);
	expect_value(mock_write, size, sizeof(buffer));
	will_return(mock_write, -2);
	assert_int_equal(flash_write_ops(&flash->ops, buffer, 0x90,
					 sizeof(buffer)),
			 -2);
}

static void test_mmap_backed_flash_erase_by_base_ops(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint32_t size = 0x100;
	expect_value(mock_erase, offset, 0x90);
	expect_value(mock_erase, size, size);
	will_return(mock_erase, size);
	assert_int_equal(flash_erase_ops(&flash->ops, 0x90, size), size);
}

static void test_mmap_backed_flash_erase_fail_by_base_ops(void **state)
{
	const uint8_t *host_data = get_host_data();
	add_mmap_window(0x1000, (uintptr_t)host_data, HOST_DATA_SIZE);

	MmapFlash *flash = new_mmap_backed_flash(&mock_ops);
	assert_non_null(flash);

	uint32_t size = 0x100;
	expect_value(mock_erase, offset, 0x90);
	expect_value(mock_erase, size, size);
	will_return(mock_erase, -2);
	assert_int_equal(flash_erase_ops(&flash->ops, 0x90, size), -2);
}

#define FLASH_TEST(func) cmocka_unit_test_setup(func, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		FLASH_TEST(test_mmap_flash_without_mmap_table),
		FLASH_TEST(test_mmap_flash_read_fail),
		FLASH_TEST(test_mmap_flash_read_ok),
		FLASH_TEST(test_mmap_flash_write_not_supported),
		FLASH_TEST(test_mmap_backed_flash_read_by_mmap),
		FLASH_TEST(test_mmap_backed_flash_read_by_base_ops),
		FLASH_TEST(test_mmap_backed_flash_read_fail_by_base_ops),
		FLASH_TEST(test_mmap_backed_flash_write_by_base_ops),
		FLASH_TEST(test_mmap_backed_flash_write_fail_by_base_ops),
		FLASH_TEST(test_mmap_backed_flash_erase_by_base_ops),
		FLASH_TEST(test_mmap_backed_flash_erase_fail_by_base_ops),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
