// SPDX-License-Identifier: GPL-2.0

#include "board/skywalker/include/variant.h"

const char *get_rts545x_configs(int ec_pd_id,
				const struct ec_response_pd_chip_info_v2 *r)
{
	return ec_pd_id == 0 ? RTS545X_TUSB546 : RTS545X_PS8747;
}
