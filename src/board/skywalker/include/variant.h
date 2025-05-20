/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOARD_SKYWALKER_VARIANT_H_
#define _BOARD_SKYWALKER_VARIANT_H_

#include <stdint.h>

#define PDC_PORT_NUM 2

#define RTS545X_PS8747	"0B"
#define RTS545X_TUSB546	"0C"
#define RTS545X_IT5205	"0D"

/* Defined in board.c */
extern const char *const rts545x_configs[PDC_PORT_NUM];

#endif /* _BOARD_SKYWALKER_VARIANT_H_ */
