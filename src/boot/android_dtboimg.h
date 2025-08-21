/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOOT_ANDROID_SEPARATE_DTB_H_
#define _BOOT_ANDROID_SEPARATE_DTB_H_

#include <stddef.h>

/**
 * android_parse_dtbs - Chose a DTB + a set of DTB overlays and construct a final DT
 */
struct device_tree *android_parse_dtbs(void *dtb, size_t dtb_size,
				       void *dtbo, size_t dtbo_size);


#endif /* _BOOT_ANDROID_SEPARATE_DTB_H_ */
