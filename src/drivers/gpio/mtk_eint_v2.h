/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef __DRIVERS_GPIO_MTK_EINT_V2_H__
#define __DRIVERS_GPIO_MTK_EINT_V2_H__

#include <stdint.h>

enum {
	EINT_INVALID = 0,
	EINT_E,
	EINT_S,
	EINT_W,
	EINT_N,
	EINT_C,
};

struct eint_info {
	uint8_t instance;
	uint8_t index;
};

extern const struct eint_info eint_data[];
extern const size_t eint_data_len;

#endif /* __DRIVERS_GPIO_MTK_EINT_V2_H__ */
