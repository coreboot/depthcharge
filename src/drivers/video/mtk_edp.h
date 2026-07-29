/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef __DRIVERS_VIDEO_MTK_EDP_H__
#define __DRIVERS_VIDEO_MTK_EDP_H__

#include <stdbool.h>
#include <stdint.h>

void mtk_edp_tx_disable(uintptr_t edp_base);
bool mtk_edp_is_enabled(uintptr_t edp_base);

#endif /* __DRIVERS_VIDEO_MTK_EDP_H__ */
