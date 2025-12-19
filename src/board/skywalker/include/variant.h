/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOARD_SKYWALKER_VARIANT_H_
#define _BOARD_SKYWALKER_VARIANT_H_

#include <stdint.h>

#include "drivers/ec/cros/ec.h"

#define MAX_PDC_PORT_NUM 2

#define RTS545X_PS8747	"0B00"
#define RTS545X_TUSB546	"0C00"
#define RTS545X_IT5205	"0D00"
#define RTS545XVB_PS8747	"0U00"

const char *get_rts545x_configs(int ec_pd_id,
				const struct ec_response_pd_chip_info_v2 *r);

#endif /* _BOARD_SKYWALKER_VARIANT_H_ */
