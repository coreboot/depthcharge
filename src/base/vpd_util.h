/*
 * Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __VPD_UTIL_H__
#define __VPD_UTIL_H__

/*
 * Find VPD value by key.
 *
 * Searches for a VPD entry in the VPD cache. If found, places the size of the
 * entry into '*size', the offset of the entry in the blob in '*offset' and
 * returns a pointer to the entry in the blob.
 *
 * If blob is NULL then the VPD data that is cached in DRAM will be used to find
 * the key.
 *
 * Returns NULL if key is not found.
 */
const u8 *vpd_find(const char *key, const u8 *blob, u32 *offset, u32 *size);

/*
 * Get VPD value by key.
 *
 * Searches for a VPD entry in the VPD cache. If found, places the size of the
 * entry into '*size' and returns the pointer to the entry data.
 *
 * This function presumes that VPD is cached in DRAM (which is the case in the
 * current implementation) and as such returns the pointer into the cache. The
 * user is not supposed to modify the data, and does not have to free the
 * memory.
 *
 * Returns NULL if key is not found.
 */
char *vpd_gets(const char *key, char *buffer, u32 size);

#endif  /* __VPD_UTIL_H__ */
