/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <libpayload.h>

#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/video/mtk_ddp.h"
#include "drivers/video/mtk_edp.h"
#include "board/skywalker/include/variant.h"

int board_panel_poweroff(MtkDisplay *me)
{
	GpioOps *edp_pp3300 = sysinfo_lookup_gpio("panel_pp3300_edp", 1,
						  new_mtk_gpio_output);
	if (!edp_pp3300) {
		printf("ERROR: panel_pp3300_edp is NULL\n");
		return -1;
	}

	/* eDP T-timing: BL (already off) -> [1ms] -> mute+bandgap -> [>100ms] -> 3.3V off */
	if (mtk_edp_is_enabled(me->edp_base)) {
		mdelay(1);
		mtk_edp_tx_disable(me->edp_base);
		mdelay(120);
		gpio_set(edp_pp3300, 0);
	}

	return 0;
}
