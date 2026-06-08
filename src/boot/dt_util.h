/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _DT_UTIL_H_
#define _DT_UTIL_H_

#include <commonlib/device_tree.h>

#include "base/ranges.h"
#include "vboot/boot.h"

void dt_update_chosen(struct device_tree *tree, struct boot_info *bi);
void dt_update_memory(struct device_tree *tree);

void dt_add_ramdisk(struct device_tree *tree, void *ramdisk_addr,
		    size_t ramdisk_size);
/*
 * Add a node with linux,pkvm-guest-firmware-memory compatible under
 * reserved-memory node reg pointing to the given address and size.
 * This shall be ran after fit_load() has been called.
 */
int dt_add_pvmfw(struct device_tree *tree, void *pvmfw_addr, size_t pvmfw_size);
int dt_add_pkvm_drng(struct device_tree *tree, struct boot_info *bi);

void dt_collect_reserved_memory_ranges(struct device_tree *tree, Ranges *ranges);

#endif /* _DT_UTIL_H_ */
