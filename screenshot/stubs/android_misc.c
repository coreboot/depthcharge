// SPDX-License-Identifier: GPL-2.0

#include "base/android_misc.h"

enum android_misc_bcb_command android_misc_get_bcb_command(BlockDev *disk, GptData *gpt)
{
	return MISC_BCB_NORMAL_BOOT;
}
