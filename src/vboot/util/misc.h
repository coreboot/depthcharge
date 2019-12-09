/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Miscellaneous support macros and functions.
 */

#ifndef VBOOT_REFERENCE_MISC_H_
#define VBOOT_REFERENCE_MISC_H_

#define PRINT_BYTES(title, value) do { \
		int i; \
		printf(title); \
		printf(":"); \
		for (i = 0; i < sizeof(*(value)); i++) \
			printf(" %02x", *((uint8_t *)(value) + i)); \
		printf("\n"); \
	} while (0)

#endif  /* VBOOT_REFERENCE_MISC_H_ */
