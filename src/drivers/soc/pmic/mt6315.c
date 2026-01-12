// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <assert.h>
#include <delay.h>
#include <die.h>
#include <libpayload.h>

#include "drivers/soc/pmic/mt6315.h"
#include "drivers/soc/pmic/mtk_pmif.h"

static MtkPmifOps *pmif_ops;

void mt6315_set_pmif_ops(MtkPmifOps *ops)
{
	die_if(pmif_ops, "%s: pmif ops already set\n", __func__);
	pmif_ops = ops;
}

int mt6315_check_init(void)
{
	if (!pmif_ops || !pmif_ops->check_init_done)
		die("pmif_ops->check_init_done not initialized\n");

	return pmif_ops->check_init_done(pmif_ops);
}

u8 mt6315_read8(u32 slvid, u32 reg)
{
	if (!pmif_ops || !pmif_ops->read8)
		die("pmif_ops->read8 not initialized\n");

	return pmif_ops->read8(pmif_ops, slvid, reg);
}

void mt6315_write8(u32 slvid, u32 reg, u8 data)
{
	if (!pmif_ops || !pmif_ops->write8)
		die("pmif_ops->write8 not initialized\n");

	pmif_ops->write8(pmif_ops, slvid, reg, data);
}

void mt6315_write_field(u32 slvid, u32 reg, u32 val, u32 mask, u32 shift)
{
	if (!pmif_ops || !pmif_ops->write_field)
		die("pmif_ops->write_field not initialized\n");

	pmif_ops->write_field(pmif_ops, slvid, reg, val, mask, shift);
}

u32 mt6315_read_field(u32 slvid, u32 reg,u32 mask, u32 shift)
{
	if (!pmif_ops || !pmif_ops->read_field)
		die("pmif_ops->read_field not initialized\n");

	return pmif_ops->read_field(pmif_ops, slvid, reg, mask, shift);
}

void mt6315_buck_set_voltage(u32 slvid, u32 buck_id, u32 buck_uv)
{
	unsigned int vol_reg, vol_val;

	switch (buck_id) {
	case MT6315_BUCK_1:
		vol_reg = MT6315_BUCK_TOP_ELR0;
		break;
	case MT6315_BUCK_3:
		vol_reg = MT6315_BUCK_TOP_ELR3;
		break;
	default:
		die("ERROR: Unknown buck_id %u", buck_id);
		return;
	};

	vol_val = buck_uv / 6250;
	mt6315_write8(slvid, vol_reg, vol_val);
}

u32 mt6315_buck_get_voltage(u32 slvid, u32 buck_id)
{
	u32 vol_reg, vol;

	switch (buck_id) {
	case MT6315_BUCK_1:
		vol_reg = MT6315_BUCK_VBUCK1_DBG0;
		break;
	case MT6315_BUCK_3:
		vol_reg = MT6315_BUCK_VBUCK1_DBG3;
		break;
	default:
		die("ERROR: Unknown buck_id %u", buck_id);
		return 0;
	};

	vol = mt6315_read8(slvid, vol_reg);
	return vol * 6250;
}
