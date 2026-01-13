/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef __DRIVERS_VIDEO_MTK_DSI_H__
#define __DRIVERS_VIDEO_MTK_DSI_H__

#include <stdint.h>

/* DSI structure */
struct mtk_dsi {
	uintptr_t reg_base[2];
};

/* DSI register offsets */
#define DSI_START				0x00
#define DSI_INTEN				0x08
#define DSI_INTSTA				0x0C
#define DSI_CON_CTRL				0x10
#define DSI_MODE_CTRL				0x14
#define DSI_TXRX_CTRL				0x18
#define DSI_PSCTRL				0x1C

/* DSI_START */
#define DSI_START_FLD_DSI_START			(0x1 << 0)

/* DSI_INTSTA */
#define DSI_BUSY				(0x1 << 31)

/* Function declarations */
void mtk_dsi_stop(struct mtk_dsi *dsi);
void mtk_dsi_wait_for_idle(struct mtk_dsi *dsi);

#endif /* __DRIVERS_VIDEO_MTK_DSI_H__ */
