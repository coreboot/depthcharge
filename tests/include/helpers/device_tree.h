/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _HELPERS_DEVICE_TREE_H
#define _HELPERS_DEVICE_TREE_H

#include "base/device_tree.h"

void assert_dt_bin_prop(struct device_tree_node *node, const char *name,
			const void *expected_data, size_t expected_size);

void assert_dt_u32_prop(struct device_tree_node *node,
			const char *name, u32 val);

void assert_dt_u64_prop(struct device_tree_node *node,
			const char *name, u64 val);

void assert_dt_string_prop(struct device_tree_node *node, const char *name,
			   const char *string);

#define assert_dt_compat_strings(node, ...) \
	_assert_dt_compat_strings(node, (const char *[]){ __VA_ARGS__ , NULL })
void _assert_dt_compat_strings(struct device_tree_node *node,
			       const char *const *compat_strings);

#endif /* _HELPERS_DEVICE_TREE_H */
