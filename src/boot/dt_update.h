/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _DT_UPDATE_H_
#define _DT_UPDATE_H_

#include <commonlib/device_tree.h>

void dt_update_chosen(struct device_tree *tree, const char *cmd_line);
void dt_update_memory(struct device_tree *tree);

#endif /* _DT_UPDATE_H_ */
