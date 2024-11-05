/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2024 Google LLC.  */

/** Type for storing a 3-byte PDC FW version tuple. Bits 23:16 = Major,
 *  15:8 = Minor, 7:0 = Patch versions. Simplifies comparisons of two
 *  version values.
 */
typedef uint32_t pdc_fw_ver_t;

/**
 * @brief Convert a three part PDC FW version into a single integer for
 *        comparison.
 */
#define PDC_FWVER_TO_INT(MAJOR, MINOR, PATCH)                                                  \
	(pdc_fw_ver_t)(((MAJOR) & 0xFF) << 16 | ((MINOR) & 0xFF) << 8 | ((PATCH) & 0xFF))

#define PDC_FWVER_MAJOR(VER) (((VER) >> 16) & 0xFF)
#define PDC_FWVER_MINOR(VER) (((VER) >> 8) & 0xFF)
#define PDC_FWVER_PATCH(VER) ((VER) & 0xFF)

#define PDC_FWVER_TYPE_BRINGUP (0)
#define PDC_FWVER_TYPE_TEST (1)
#define PDC_FWVER_TYPE_BASE (2)
#define PDC_FWVER_TYPE_PROD_MASK (0xF0)
