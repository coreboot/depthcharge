/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __SOC_QCOM_SPMI_DEFS_H__
#define __SOC_QCOM_SPMI_DEFS_H__

#if CONFIG(DRIVER_SOC_CALYPSO)
#define PMIC_CORE_REGISTERS_ADDR 0x0C900000
#define PMIC_REG_CHAN0_ADDR 0x0C403000
#define PMIC_REG_LAST_CHAN_ADDR 0x5800
#define PMIC_REG_FIRST_CHAN_ADDR 0x3000
/* ADDR.PPID is 13 bits: SID (20:16) + Periph (15:8) */
#define SPMI_PPID_FROM_ADDR(a) (((a) >> 8) & 0x1fffU)
/* REG.PPID is 13 bits: SID (12:8) + Periph (7:0) */
#define SPMI_PPID_FROM_REG(r)  ((r) & 0x1fffU)
#elif CONFIG(DRIVER_SOC_X1P42100)
#define PMIC_CORE_REGISTERS_ADDR 0x0C500000
#define PMIC_REG_CHAN0_ADDR 0x0C402000
#define PMIC_REG_LAST_CHAN_ADDR 0x3000
#define PMIC_REG_FIRST_CHAN_ADDR 0x2000
/* ADDR.PPID is 12 bits: SID (19:16) + Periph (15:8) */
#define SPMI_PPID_FROM_ADDR(a) (((a) >> 8) & 0xfffU)
/* REG.PPID is 12 bits: SID (19:16) + Periph (15:8) */
#define SPMI_PPID_FROM_REG(r) (((r) >> 8) & 0xfffU)
#else
#define PMIC_CORE_REGISTERS_ADDR 0x0C600000
#define PMIC_REG_CHAN0_ADDR 0x0C440900
#define PMIC_REG_LAST_CHAN_ADDR 0x1100
#define PMIC_REG_FIRST_CHAN_ADDR 0x900
/* ADDR.PPID is 12 bits: SID (19:16) + Periph (15:8) */
#define SPMI_PPID_FROM_ADDR(a) (((a) >> 8) & 0xfffU)
/* REG.PPID is 12 bits: SID (19:16) + Periph (15:8) */
#define SPMI_PPID_FROM_REG(r) (((r) >> 8) & 0xfffU)
#endif

#endif // __SOC_QCOM_SPMI_DEFS_H__