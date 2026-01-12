/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef __DRIVER_SOC_PMIC_MTK_PMIF_H__
#define __DRIVER_SOC_PMIC_MTK_PMIF_H__

#include <libpayload.h>
#include <stddef.h>

enum {
	PMIF_READ_US        = 1000,
	PMIF_WAIT_IDLE_US   = 1000,
};

enum {
	PMIF_CMD_REG = 1,
	PMIF_CMD_EXT_REG,
	PMIF_CMD_EXT_REG_LONG,
};

struct mtk_pmif_regs {
	u32 init_done;
	/* Other registers are not needed. */
};

struct mtk_chan_regs {
	u32 ch_send;
	u32 wdata;
	u32 reserved12[3];
	u32 rdata;
	u32 reserved13[3];
	u32 ch_rdy;
	u32 ch_sta;
};

typedef struct MtkPmifOps {
	int (*check_init_done)(struct MtkPmifOps *me);
	u8 (*read8)(struct MtkPmifOps *me, u32 slvid, u32 reg);
	void (*write8)(struct MtkPmifOps *me, u32 slvid, u32 reg, u8 data);
	u16 (*read16)(struct MtkPmifOps *me, u32 slvid, u32 reg);
	void (*write16)(struct MtkPmifOps *me, u32 slvid, u32 reg, u16 data);
	u32 (*read_field)(struct MtkPmifOps *me, u32 slvid, u32 reg,
			  u32 mask, u32 shift);
	void (*write_field)(struct MtkPmifOps *me, u32 slvid, u32 reg, u32 val,
			    u32 mask, u32 shift);
} MtkPmifOps;

typedef struct {
	MtkPmifOps ops;
	struct mtk_pmif_regs *pmif_regs;
	struct mtk_chan_regs *chan_regs;
} MtkPmif;

MtkPmif *new_mtk_pmif_spi(uintptr_t pmif_base, size_t channel_offset);
MtkPmif *new_mtk_pmif_spmi(uintptr_t pmif_base, size_t channel_offset);

#endif /*__DRIVER_SOC_PMIC_MTK_PMIF_H__*/
