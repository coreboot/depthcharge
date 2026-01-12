/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef __DRIVER_SOC_PMIC_MT6315_H__
#define __DRIVER_SOC_PMIC_MT6315_H__

#include <libpayload.h>

#include "drivers/soc/pmic/mtk_pmif.h"

enum {
	MT6315_BUCK_1 = 0,
	MT6315_BUCK_2,
	MT6315_BUCK_3,
	MT6315_BUCK_4,
};

enum {
	MT6315_BUCK_TOP_ELR0	= 0x1449,
	MT6315_BUCK_TOP_ELR3	= 0x144d,
	MT6315_BUCK_VBUCK1_DBG0	= 0x1499,
	MT6315_BUCK_VBUCK1_DBG3	= 0x1599,
};

enum {
	SPMI_SLAVE_6 = 6,
	SPMI_SLAVE_7 = 7,
	SPMI_SLAVE_8 = 8,
};

void mt6315_set_pmif_ops(MtkPmifOps *ops);

int mt6315_check_init(void);
u8 mt6315_read8(u32 slvid, u32 reg);
void mt6315_write8(u32 slvid, u32 reg, u8 data);
void mt6315_buck_set_voltage(u32 slvid, u32 buck_id, u32 buck_uv);
u32 mt6315_buck_get_voltage(u32 slvid, u32 buck_id);
void mt6315_write_field(u32 slvid, u32 reg, u32 val, u32 mask, u32 shift);
u32 mt6315_read_field(u32 slvid, u32 reg,u32 mask, u32 shift);

#endif /* __DRIVER_SOC_PMIC_MT6315_H__ */
