// SPDX-License-Identifier: GPL-2.0

#include <string.h>

#include "mocks/fmap_area.h"
#include "tests/test.h"

FmapArea *mock_area;
void *mock_flash_buf;

void set_mock_fmap_area(FmapArea *area, void *mirror_buf)
{
	mock_area = area;
	mock_flash_buf = mirror_buf;
}

#define CHECK_FMAP_AREA_SET() do { \
	assert_non_null(mock_area); \
	assert_non_null(mock_flash_buf); \
} while (0)

/* Mock functions from image/fmap.h */

const int fmap_find_area(const char *name, FmapArea *area)
{
	CHECK_FMAP_AREA_SET();
	check_expected(name);
	assert_string_equal(name, mock_area->name);
	*area = *mock_area;
	return mock();
}

/* Mock functions from drivers/flash/flash.h
   These mocks will fail if the requested region is not in mock FmapArea */

#define CHECK_REGION(offset, size) do { \
	assert_true(offset >= mock_area->offset); \
	assert_true(offset + size <= mock_area->offset + mock_area->size); \
} while (0)

int __must_check flash_read(void *buffer, uint32_t offset, uint32_t size)
{
	CHECK_FMAP_AREA_SET();
	assert_non_null(buffer);
	CHECK_REGION(offset, size);
	memcpy(buffer, mock_flash_buf + offset - mock_area->offset, size);

	if (mock() == MOCK_FLASH_FAIL)
		return 0;
	return size;
}
