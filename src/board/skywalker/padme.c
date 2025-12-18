// SPDX-License-Identifier: GPL-2.0

#include <stdbool.h>
#include <string.h>

#include "board/skywalker/include/variant.h"

static bool is_rts545xvb(const struct ec_response_pd_chip_info_v2 *r)
{
	/* "0U" is the config identifier of RTS5452P-VB for Skywalker. */
	return !strncmp(r->fw_name_str, "GOOG0U", 6);
}

const char *get_rts545x_configs(int ec_pd_id,
				const struct ec_response_pd_chip_info_v2 *r)
{
	return is_rts545xvb(r) ? RTS545XVB_PS8747 : RTS545X_PS8747;
}

const char *get_pdc_chip_name(int ec_pd_id,
			      const struct ec_response_pd_chip_info_v2 *r)
{
	return is_rts545xvb(r) ? "rts5453vb" : "rts5453";
}
