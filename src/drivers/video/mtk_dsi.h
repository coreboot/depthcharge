/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef __DRIVERS_VIDEO_MTK_DSI_H__
#define __DRIVERS_VIDEO_MTK_DSI_H__

#include <stdbool.h>
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
#define DSI_CMDQ_SIZE				0x60 /* TODO: Move to MT8189 DSI header. */
#define DSI_CMDQ				0xD00 /* TODO: Move to MT8189 DSI header. */

/* DSI_START */
#define DSI_START_FLD_DSI_START			(0x1 << 0)

/* DSI_INTSTA */
#define DSI_BUSY				(0x1 << 31)

/* DSI_MODE_CTRL */
#define CMD_MODE				0
#define SYNC_PULSE_MODE				1
#define BURST_MODE				3

/* DSI_CMDQ_SIZE */
#define CMDQ_SIZE				0x3f
#define CMDQ_SIZE_SEL				(0x1 << 15)

/* Packet types */
#define SHORT_PACKET				0
#define LONG_PACKET				2
#define BTA					(0x1 << 2)

/* Function declarations */
void mtk_dsi_poweroff(struct mtk_dsi *dsi);

/* DSI detection helpers */
bool mtk_dsi_is_enabled(struct mtk_dsi *dsi);

#endif /* __DRIVERS_VIDEO_MTK_DSI_H__ */
