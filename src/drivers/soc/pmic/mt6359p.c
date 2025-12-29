// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <assert.h>
#include <die.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "drivers/soc/pmic/mt6359p.h"
#include "drivers/soc/pmic/mtk_pmif.h"

static MtkPmifOps *pmif_ops;

void mt6359p_set_pmif_ops(MtkPmifOps *ops)
{
	die_if(pmif_ops, "%s: pmif ops already set\n", __func__);
	pmif_ops = ops;
}

int mt6359p_check_init(void)
{
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	return pmif_ops->check_init_done(pmif_ops);
}

u32 mt6359p_read32(u32 reg)
{
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	return pmif_ops->read32(pmif_ops, 0, reg);
}

void mt6359p_write32(u32 reg, u32 data)
{
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	pmif_ops->write32(pmif_ops, 0, reg, data);
}

u32 mt6359p_read_field(u32 reg, u32 mask, u32 shift)
{
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	return pmif_ops->read_field(pmif_ops, 0, reg, mask, shift);
}

void mt6359p_write_field(u32 reg, u32 val, u32 mask, u32 shift)
{
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	pmif_ops->write_field(pmif_ops, 0, reg, val, mask, shift);
}

static void get_buck_params(u32 buck_id, u32 *vol_offset, u32 *vol_step,
			    u32 *vol_set_reg, u32 *vol_get_reg,
			    u32 *vol_get_shift)
{
	switch (buck_id) {
	case MT6359P_GPU11:
		*vol_offset = 400000;
		*vol_step = 6250;
		*vol_set_reg = PMIC_VGPU11_ELR0;
		*vol_get_reg = PMIC_VGPU11_DBG0;
		*vol_get_shift = 0;
		break;
	case MT6359P_SRAM_PROC1:
		*vol_offset = 500000;
		*vol_step = 6250;
		*vol_set_reg = PMIC_VSRAM_PROC1_ELR;
		*vol_get_reg = PMIC_VSRAM_PROC1_VOSEL1;
		*vol_get_shift = 8;
		break;
	case MT6359P_SRAM_PROC2:
		*vol_offset = 500000;
		*vol_step = 6250;
		*vol_set_reg = PMIC_VSRAM_PROC2_ELR;
		*vol_get_reg = PMIC_VSRAM_PROC2_VOSEL1;
		*vol_get_shift = 8;
		break;
	case MT6359P_CORE:
		*vol_offset = 506250;
		*vol_step = 6250;
		*vol_set_reg = PMIC_VCORE_ELR0;
		*vol_get_reg = PMIC_VCORE_DBG0;
		*vol_get_shift = 0;
		break;
	case MT6359P_PA:
		*vol_offset = 500000;
		*vol_step = 50000;
		*vol_set_reg = PMIC_VPA_CON1;
		*vol_get_reg = PMIC_VPA_DBG0;
		*vol_get_shift = 0;
		break;
	case MT6359P_VMODEM:
		*vol_offset = 400000;
		*vol_step = 6250;
		*vol_set_reg = PMIC_VMODEM_ELR0;
		*vol_get_reg = PMIC_VMODEM_DBG0;
		*vol_get_shift = 0;
		break;
	case MT6359P_VSRAM_MD:
		*vol_offset = 500000;
		*vol_step = 6250;
		*vol_set_reg = PMIC_VSRAM_MD_ELR;
		*vol_get_reg = PMIC_VSRAM_MD_VOSEL1;
		*vol_get_shift = 8;
		break;
	default:
		die("Unknown buck_id %u\n", buck_id);
	}
}

void mt6359p_buck_set_voltage(u32 buck_id, u32 buck_uv)
{
	u32 vol_offset, vol_step, vol_set_reg;
	u32 unused;
	u32 vol;

	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	get_buck_params(buck_id, &vol_offset, &vol_step, &vol_set_reg,
			&unused, &unused);

	vol = (buck_uv - vol_offset) / vol_step;
	mt6359p_write_field(vol_set_reg, vol, 0x7F, 0);
}

u32 mt6359p_buck_get_voltage(u32 buck_id)
{
	u32 vol_get_shift, vol_offset, vol_step, vol_get_reg;
	u32 unused;
	u32 vol;

	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	get_buck_params(buck_id, &vol_offset, &vol_step, &unused,
			&vol_get_reg, &vol_get_shift);

	vol = mt6359p_read_field(vol_get_reg, 0x7F, vol_get_shift);
	return vol_offset + vol * vol_step;
}

void mt6359p_set_vm18_voltage(u32 vm18_uv)
{
	u32 reg_vol, reg_cali;

	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	assert(vm18_uv >= 1700000);
	assert(vm18_uv < 2000000);

	reg_vol = (vm18_uv / 1000 - VM18_VOL_OFFSET) / 100;
	reg_cali = ((vm18_uv / 1000) % 100) / 10;
	mt6359p_write32(PMIC_VM18_ANA_CON0, (reg_vol << VM18_VOL_REG_SHIFT) | reg_cali);
}

u32 mt6359p_get_vm18_voltage(void)
{
	u32 reg_vol, reg_cali;

	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	reg_vol = 100 * mt6359p_read_field(PMIC_VM18_ANA_CON0, 0xF, VM18_VOL_REG_SHIFT);
	reg_cali = 10 * mt6359p_read_field(PMIC_VM18_ANA_CON0, 0xF, 0);
	return 1000 * (VM18_VOL_OFFSET + reg_vol + reg_cali);
}

void mt6359p_set_vsim1_voltage(u32 vsim1_uv)
{
	u32 reg_vol, reg_cali, reg_offset;

	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	if (vsim1_uv >= 1700000 && vsim1_uv <= 1900000)
		reg_offset = VSIM1_VOL_OFFSET_1;
	else if (vsim1_uv >= 2700000 && vsim1_uv <= 2800000)
		reg_offset = VSIM1_VOL_OFFSET_2;
	else if (vsim1_uv >= 3000000 && vsim1_uv <= 3200000)
		reg_offset = VSIM1_VOL_OFFSET_2;
	else
		die("Unknown vsim1 voltage %u\n", vsim1_uv);

	reg_vol = (vsim1_uv / 1000 - reg_offset) / 100;
	reg_cali = ((vsim1_uv / 1000) % 100) / 10;
	mt6359p_write32(PMIC_VSIM1_ANA_CON0,
			(reg_vol << VSIM1_VOL_REG_SHIFT) | reg_cali);
}

u32 mt6359p_get_vsim1_voltage(void)
{
	u32 reg_vol, reg_cali, reg_offset;

	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	reg_vol = 100 * mt6359p_read_field(PMIC_VSIM1_ANA_CON0, 0xF,
					   VSIM1_VOL_REG_SHIFT);
	reg_cali = 10 * mt6359p_read_field(PMIC_VSIM1_ANA_CON0, 0xF, 0);

	if (reg_vol == 300 || reg_vol == 400)
		reg_offset = VSIM1_VOL_OFFSET_1;
	else if (reg_vol == 800 || reg_vol == 1100 || reg_vol == 1200)
		reg_offset = VSIM1_VOL_OFFSET_2;
	else
		die("Unknown vsim1 reg_vol %x\n", reg_vol);

	return 1000 * (reg_offset + reg_vol + reg_cali);
}

u32 mt6359p_get_vcn18_voltage(void)
{
	u32 reg_vol, reg_cali;
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	reg_vol = 100 * mt6359p_read_field(PMIC_VCN18_ANA_CON0, 0xF, VCN18_VOL_SHIFT);
	reg_cali = 10 * mt6359p_read_field(PMIC_VCN18_ANA_CON0, 0xF, 0);
	return 1000 * (VCN18_VOL_MV_OFFSET + reg_vol + reg_cali);
}

void mt6359p_set_vcn18_voltage(u32 vcn18_uv)
{
	u32 reg_vol, reg_cali;
	if (!pmif_ops)
		die("pmif_ops not initialized\n");

	if (vcn18_uv < VCN18_VOL_UV_MIN || vcn18_uv > VCN18_VOL_UV_MAX)
		return;

	reg_vol = (vcn18_uv / 1000 - VCN18_VOL_MV_OFFSET) / 100;
	reg_cali = (vcn18_uv / 1000) % 100 / 10;
	mt6359p_write32(PMIC_VCN18_ANA_CON0, (reg_vol << VCN18_VOL_SHIFT) | reg_cali);
}

void mt6359p_enable_vpa(bool enable)
{
	mt6359p_write_field(PMIC_VPA_CON0, enable, 0x1, 0);
}

void mt6359p_enable_vsim1(bool enable)
{
	mt6359p_write_field(PMIC_VSIM1_CON0, enable, 0x1, 0);
}

void mt6359p_enable_vm18(bool enable)
{
	mt6359p_write_field(PMIC_VM18_CON0, enable, 0x1, 0);
}

void mt6359p_enable_vcn18(bool enable)
{
	mt6359p_write_field(PMIC_VCN18_CON0, enable, 0x1, 0);
}
