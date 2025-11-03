/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOOT_ANDROID_MTE_H_
#define _BOOT_ANDROID_MTE_H_

#include <stddef.h>

#include "base/gpt.h"

int android_mte_setup(char *cmdline_buf, size_t size);
void android_mte_get_misc_ctrl(BlockDev *bdev, GptData *gpt);

#endif /* _BOOT_ANDROID_MTE_H_ */
