// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/device_tree.h"
#include "helpers/device_tree.h"
#include "tests/test.h"

void assert_dt_bin_prop(DeviceTreeNode *node, const char *name,
			const void *expected_data, size_t expected_size)
{
	void *found_data;
	size_t found_size;

	dt_find_bin_prop(node, name, &found_data, &found_size);
	assert_non_null(found_data);
	assert_int_equal(expected_size, found_size);
	assert_memory_equal(expected_data, found_data, expected_size);
}

void assert_dt_u32_prop(DeviceTreeNode *node, const char *name, u32 val)
{
	u32 be = htobe32(val);
	assert_dt_bin_prop(node, name, &be, sizeof(be));
}

void assert_dt_u64_prop(DeviceTreeNode *node, const char *name, u64 val)
{
	u64 be = htobe64(val);
	assert_dt_bin_prop(node, name, &be, sizeof(be));
}

void assert_dt_string_prop(DeviceTreeNode *node, const char *name,
			   const char *string)
{
	assert_dt_bin_prop(node, name, string, strlen(string) + 1);
}

void _assert_dt_compat_strings(DeviceTreeNode *node,
			       const char *const *compat_strings)
{
	char *data;
	size_t size;

	dt_find_bin_prop(node, "compatible", (void **)&data, &size);
	assert_non_null(&data);
	assert_int_not_equal(size, 0);
	assert_int_equal(data[size - 1], '\0');

	do {
		size_t len = strlen(*compat_strings) + 1;

		assert_true(len <= size);
		assert_string_equal(*compat_strings, data);

		data += len;
		size -= len;
	} while (*++compat_strings);

	assert_int_equal(size, 0);
}
