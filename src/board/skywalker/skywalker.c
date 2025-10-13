// SPDX-License-Identifier: GPL-2.0

#include "board/skywalker/include/variant.h"

const char *get_rts545x_configs(int ec_pd_id)
{
	return ec_pd_id == 0 ? RTS545X_TUSB546 : RTS545X_PS8747;
}
