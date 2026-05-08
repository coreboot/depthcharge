/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _DT_UPDATE_H_
#define _DT_UPDATE_H_

#include <commonlib/device_tree.h>

#include "base/ranges.h"
#include "vboot/boot.h"

void dt_update_chosen(struct device_tree *tree, struct boot_info *bi);
void dt_update_memory(struct device_tree *tree);
void dt_collect_reserved_memory_ranges(struct device_tree *tree, Ranges *ranges);
int dt_add_pkvm_drng(struct device_tree *tree, struct boot_info *bi);

#endif /* _DT_UPDATE_H_ */
