/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOARD_SKYWALKER_VARIANT_H_
#define _BOARD_SKYWALKER_VARIANT_H_

#include <stdint.h>

#define MAX_PDC_PORT_NUM 2

#define RTS545X_PS8747	"0B00"
#define RTS545X_TUSB546	"0C00"
#define RTS545X_IT5205	"0D00"

/* Defined in board.c */
extern const char *const rts545x_configs[MAX_PDC_PORT_NUM];

#endif /* _BOARD_SKYWALKER_VARIANT_H_ */
