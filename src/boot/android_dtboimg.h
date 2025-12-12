/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOOT_ANDROID_SEPARATE_DTB_H_
#define _BOOT_ANDROID_SEPARATE_DTB_H_

#include <stddef.h>

/**
 * android_parse_dtbs - Chose a DTB + a set of DTB overlays and construct a final DT
 */
struct device_tree *android_parse_dtbs(const void *dtbo, size_t dtbo_size);

/**
 * android_get_dtbo_indices - Get the matching DTBO indices in a CSV format
 */
const char *android_get_dtbo_indices(void);

#endif /* _BOOT_ANDROID_SEPARATE_DTB_H_ */
