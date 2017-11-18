/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_GPIO_ICELAKE_H__
#define __DRIVERS_GPIO_ICELAKE_H__

#include <stdint.h>
#include "base/cleanup_funcs.h"
#include "drivers/gpio/gpio.h"

/*
 * There are 9 GPIO groups. GPP_A -> GPP_H and GPD. GPD is the special case
 * where that group is not so generic. So most of the fixed numbers and macros
 * are based on the GPP groups. The GPIO groups are accessed through register
 * blocks called communities.
 */
#define GPP_G			0
#define GPP_B			1
#define GPP_A			2
#define GPP_H			3
#define GPP_D			4
#define GPP_F			5
#define GPP_C			6
#define GPP_E			7
#define GPD			8
#define GPIO_NUM_GROUPS		9
#define GPIO_MAX_NUM_PER_GROUP	24

#define GPIO_DWx_WIDTH(x)		(4 * (x +1))
/*
 * GPIOs are ordered monotonically increasing to match ACPI/OS driver.
 */

/* Group G */
#define GPP_G0		0
#define GPP_G1		1
#define GPP_G2		2
#define GPP_G3		3
#define GPP_G4		4
#define GPP_G5		5
#define GPP_G6		6
#define GPP_G7		7

/* Group B */
#define GPP_B0		8
#define GPP_B1		9
#define GPP_B2		10
#define GPP_B3		11
#define GPP_B4		12
#define GPP_B5		13
#define GPP_B6		14
#define GPP_B7		15
#define GPP_B8		16
#define GPP_B9		17
#define GPP_B10		18
#define GPP_B11		19
#define GPP_B12		20
#define GPP_B13		21
#define GPP_B14		22
#define GPP_B15		23
#define GPP_B16		24
#define GPP_B17		25
#define GPP_B18		26
#define GPP_B19		27
#define GPP_B20		28
#define GPP_B21		29
#define GPP_B22		30
#define GPP_B23		31
#define GPIO_RSVD_0	32
#define GPIO_RSVD_1	33

/* Group A */
#define GPP_A0		34
#define GPP_A1		35
#define GPP_A2		36
#define GPP_A3		37
#define GPP_A4		38
#define GPP_A5		39
#define GPP_A6		40
#define GPP_A7		41
#define GPP_A8		42
#define GPP_A9		43
#define GPP_A10		44
#define GPP_A11		45
#define GPP_A12		46
#define GPP_A13		47
#define GPP_A14		48
#define GPP_A15		49
#define GPP_A16		50
#define GPP_A17		51
#define GPP_A18		52
#define GPP_A19		53
#define GPP_A20		54
#define GPP_A21		55
#define GPP_A22		56
#define GPP_A23		57

/* Group H */
#define GPP_H0		58
#define GPP_H1		59
#define GPP_H2		60
#define GPP_H3		61
#define GPP_H4		62
#define GPP_H5		63
#define GPP_H6		64
#define GPP_H7		65
#define GPP_H8		66
#define GPP_H9		67
#define GPP_H10		68
#define GPP_H11		69
#define GPP_H12		70
#define GPP_H13		71
#define GPP_H14		72
#define GPP_H15		73
#define GPP_H16		74
#define GPP_H17		75
#define GPP_H18		76
#define GPP_H19		77
#define GPP_H20		78
#define GPP_H21		79
#define GPP_H22		80
#define GPP_H23		81

/* Group D */
#define GPP_D0		82
#define GPP_D1		83
#define GPP_D2		84
#define GPP_D3		85
#define GPP_D4		86
#define GPP_D5		87
#define GPP_D6		88
#define GPP_D7		89
#define GPP_D8		90
#define GPP_D9		91
#define GPP_D10		92
#define GPP_D11		93
#define GPP_D12		94
#define GPP_D13		95
#define GPP_D14		96
#define GPP_D15		97
#define GPP_D16		98
#define GPP_D17		99
#define GPP_D18		100
#define GPP_D19		101
#define GPIO_RSVD_2	102

/* Group F */
#define GPP_F0		103
#define GPP_F1		104
#define GPP_F2		105
#define GPP_F3		106
#define GPP_F4		107
#define GPP_F5		108
#define GPP_F6		109
#define GPP_F7		110
#define GPP_F8		111
#define GPP_F9		112
#define GPP_F10		113
#define GPP_F11		114
#define GPP_F12		115
#define GPP_F13		116
#define GPP_F14		117
#define GPP_F15		118
#define GPP_F16		119
#define GPP_F17		120
#define GPP_F18		121
#define GPP_F19		122

/* Group GPD  */
#define GPD0		123
#define GPD1		124
#define GPD2		125
#define GPD3		126
#define GPD4		127
#define GPD5		128
#define GPD6		129
#define GPD7		130
#define GPD8		131
#define GPD9		132
#define GPD10		133
#define GPD11		134

/* Group C */
#define GPP_C0		135
#define GPP_C1		136
#define GPP_C2		137
#define GPP_C3		138
#define GPP_C4		139
#define GPP_C5		140
#define GPP_C6		141
#define GPP_C7		142
#define GPP_C8		143
#define GPP_C9		144
#define GPP_C10		145
#define GPP_C11		146
#define GPP_C12		147
#define GPP_C13		148
#define GPP_C14		149
#define GPP_C15		150
#define GPP_C16		151
#define GPP_C17		152
#define GPP_C18		153
#define GPP_C19		154
#define GPP_C20		155
#define GPP_C21		156
#define GPP_C22		157
#define GPP_C23		158
#define GPIO_RSVD_3	159
#define GPIO_RSVD_4	160
#define GPIO_RSVD_5	161
#define GPIO_RSVD_6	162
#define GPIO_RSVD_7	163
#define GPIO_RSVD_8	164

/* Group E */
#define GPP_E0		165
#define GPP_E1		166
#define GPP_E2		167
#define GPP_E3		168
#define GPP_E4		169
#define GPP_E5		170
#define GPP_E6		171
#define GPP_E7		172
#define GPP_E8		173
#define GPP_E9		174
#define GPP_E10		175
#define GPP_E11		176
#define GPP_E12		177
#define GPP_E13		178
#define GPP_E14		179
#define GPP_E15		180
#define GPP_E16		181
#define GPP_E17		182
#define GPP_E18		183
#define GPP_E19		184
#define GPP_E20		185
#define GPP_E21		186
#define GPP_E22		187
#define GPP_E23		188

/* Group R*/
#define GPP_R0		189
#define GPP_R1		190
#define GPP_R2		191
#define GPP_R3		192
#define GPP_R4		193
#define GPP_R5		194
#define GPP_R6		195
#define GPP_R7		196

/* Group S */
#define GPP_S0		197
#define GPP_S1		198
#define GPP_S2		199
#define GPP_S3		200
#define GPP_S4		201
#define GPP_S5		202
#define GPP_S6		203
#define GPP_S7		204

/* Register defines. */
#define MISCCFG_OFFSET		0x10
#define  GPIO_DRIVER_IRQ_ROUTE_MASK	8
#define  GPIO_DRIVER_IRQ_ROUTE_IRQ14	0
#define  GPIO_DRIVER_IRQ_ROUTE_IRQ15	8
#define  GPE_DW_SHIFT		8
#define  GPE_DW_MASK		0xfff00
#define PAD_OWN_REG_OFFSET	0x20
#define  PAD_OWN_PADS_PER	8
#define  PAD_OWN_WIDTH_PER	4
#define  PAD_OWN_MASK		0x03
#define  PAD_OWN_HOST		0x00
#define  PAD_OWN_ME		0x01
#define  PAD_OWN_ISH		0x02
#define HOSTSW_OWN_REG_OFFSET	0xb0
#define  HOSTSW_OWN_PADS_PER	24
#define  HOSTSW_OWN_ACPI	0
#define  HOSTSW_OWN_GPIO	1
#define PAD_CFG_DW_OFFSET	0x600
/* DW0 */
	/* PADRSTCFG - when to reset the pad config */
#define  PADRSTCFG_SHIFT	30
#define  PADRSTCFG_MASK		0x3
#define  PADRSTCFG_DSW_PWROK	0
#define  PADRSTCFG_DEEP		1
#define  PADRSTCFG_PLTRST	2
#define  PADRSTCFG_RSMRST	3
	/* RXPADSTSEL - raw signal or internal state */
#define  RXPADSTSEL_SHIFT	29
#define  RXPADSTSEL_MASK	0x1
#define  RXPADSTSEL_RAW		0
#define  RXPADSTSEL_INTERNAL	1
	/* RXRAW1 - drive 1 instead instead of pad value */
#define  RXRAW1_SHIFT		28
#define  RXRAW1_MASK		0x1
#define  RXRAW1_NO		0
#define  RXRAW1_YES		1
	/* RXEVCFG - Interrupt and wake types */
#define  RXEVCFG_SHIFT		25
#define  RXEVCFG_MASK		0x3
#define  RXEVCFG_LEVEL		0
#define  RXEVCFG_EDGE		1 /* RXINV =0 for rising edge; 1 for failing edge */
#define  RXEVCFG_DISABLE		2
#define  RXEVCFG_EITHEREDGE		3
	/* PREGFRXSEL - use filtering on Rx pad */
#define  PREGFRXSEL_SHIFT	24
#define  PREGFRXSEL_MASK	0x1
#define  PREGFRXSEL_NO		0
#define  PREGFRXSEL_YES		1
	/* RXINV - invert signal to SMI, SCI, NMI, or IRQ routing. */
#define  RXINV_SHIFT		23
#define  RXINV_MASK		0x1
#define  RXINV_NO		0
#define  RXINV_YES		1
	/* GPIROUTIOXAPIC - route to io-xapic or not */
#define  GPIROUTIOXAPIC_SHIFT	20
#define  GPIROUTIOXAPIC_MASK	0x1
#define  GPIROUTIOXAPIC_NO	0
#define  GPIROUTIOXAPIC_YES	1
	/* GPIROUTSCI - route to SCI */
#define  GPIROUTSCI_SHIFT	19
#define  GPIROUTSCI_MASK	0x1
#define  GPIROUTSCI_NO		0
#define  GPIROUTSCI_YES		1
	/* GPIROUTSMI - route to SMI */
#define  GPIROUTSMI_SHIFT	18
#define  GPIROUTSMI_MASK	0x1
#define  GPIROUTSMI_NO		0
#define  GPIROUTSMI_YES		1
	/* GPIROUTNMI - route to NMI */
#define  GPIROUTNMI_SHIFT	17
#define  GPIROUTNMI_MASK	0x1
#define  GPIROUTNMI_NO		0
#define  GPIROUTNMI_YES		1
	/* PMODE - mode of pad */
#define  PMODE_SHIFT		10
#define  PMODE_MASK		0x3
#define  PMODE_GPIO		0
#define  PMODE_NF1		1
#define  PMODE_NF2		2
#define  PMODE_NF3		3
	/* GPIORXDIS - Disable Rx */
#define  GPIORXDIS_SHIFT	9
#define  GPIORXDIS_MASK		0x1
#define  GPIORXDIS_NO		0
#define  GPIORXDIS_YES		1
	/* GPIOTXDIS - Disable Tx */
#define  GPIOTXDIS_SHIFT	8
#define  GPIOTXDIS_MASK		0x1
#define  GPIOTXDIS_NO		0
#define  GPIOTXDIS_YES		1
	/* GPIORXSTATE - Internal state after glitch filter */
#define  GPIORXSTATE_SHIFT	1
#define  GPIORXSTATE_MASK	0x1
	/* GPIOTXSTATE - Drive value onto pad */
#define  GPIOTXSTATE_SHIFT	0
#define  GPIOTXSTATE_MASK	0x1
/* DW1 */
	/* TERM - termination control */
#define  PAD_TERM_SHIFT		10
#define  PAD_TERM_MASK		0xf
#define  PAD_TERM_NONE		0
#define  PAD_TERM_5K_PD		2
#define  PAD_TERM_20K_PD	4
#define  PAD_TERM_1K_PU		9
#define  PAD_TERM_2K_PU		11
#define  PAD_TERM_5K_PU		10
#define  PAD_TERM_20K_PU	12
#define  PAD_TERM_667_PU	13
#define  PAD_TERM_NATIVE	15
/* DW2 */
	/* DEBONDUR - Debounce duration */
#define  DEBONDUR_SHIFT		1
#define  DEBONDUR_MASK		0xf
#define  DEBONDUR_NONE		0
#define  DEBONDUR_DEBOUNCE_3		3
#define  DEBONDUR_DEBOUNCE_4		4
#define  DEBONDUR_DEBOUNCE_5		5
#define  DEBONDUR_DEBOUNCE_6		6
#define  DEBONDUR_DEBOUNCE_7		7
#define  DEBONDUR_DEBOUNCE_8		8
#define  DEBONDUR_DEBOUNCE_9		9
#define  DEBONDUR_DEBOUNCE_10		10
#define  DEBONDUR_DEBOUNCE_11		11
#define  DEBONDUR_DEBOUNCE_12		12
#define  DEBONDUR_DEBOUNCE_13		13
#define  DEBONDUR_DEBOUNCE_14		14
#define  DEBONDUR_DEBOUNCE_15		15
	/* DEBONEN - Debounce enable */
#define  DEBON_SHIFT		0
#define  DEBON_MASK		1
#define  DEBON_DISABLE		0
#define  DEBON_ENABLE		1

#define PAD_FIELD_VAL(field_, val_) \
	(((val_) & field_ ## _MASK) << field_ ## _SHIFT)

#define PAD_FIELD(field_, setting_) \
	PAD_FIELD_VAL(field_, field_ ## _ ## setting_)

/*
 * This encodes all the fields found within the dw0 register for each
 * pad. It directly follows the register specification:
 *   rst - reset type when pad configuration is reset
 *   rxst - native function routing: raw buffer or internal buffer
 *   rxraw1 - drive fixed '1' for Rx buffer
 *   rxev - event filtering for pad value: level, edge, drive '0'
 *   rxgf - glitch filter enable
 *   rxinv - invert the internal pad state
 *   gpiioapic - route to IOxAPIC
 *   gpisci -  route for SCI
 *   gpismi - route for SMI
 *   gpinmi - route for NMI
 *   mode -  GPIO vs native function
 *   rxdis - disable Rx buffer
 *   txdis - disable Tx buffer
 */
#define _DW0_VALS(rst, rxst, rxraw1, rxev, rxgf, rxinv, gpiioapic, gpisci, \
			gpismi, gpinmi, mode, rxdis, txdis) \
	(PAD_FIELD(PADRSTCFG, rst) | \
	 PAD_FIELD(RXPADSTSEL, rxst) | \
	 PAD_FIELD(RXRAW1, rxraw1) | \
	 PAD_FIELD(RXEVCFG, rxev) | \
	 PAD_FIELD(PREGFRXSEL, rxgf) | \
	 PAD_FIELD(RXINV, rxinv) | \
	 PAD_FIELD(GPIROUTIOXAPIC, gpiioapic) | \
	 PAD_FIELD(GPIROUTSCI, gpisci) | \
	 PAD_FIELD(GPIROUTSMI, gpismi) | \
	 PAD_FIELD(GPIROUTNMI, gpinmi) | \
	 PAD_FIELD(PMODE, mode) | \
	 PAD_FIELD(GPIORXDIS, rxdis) | \
	 PAD_FIELD(GPIOTXDIS, txdis))

/*
 * This encodes all the fields found within the dw1 register for each
 * pad. It directly follows the register specification:
 *   debounce duration -This value shouldn't be less than 3, Time should be
 *                                   calculated as (2 ^ debounce) * (glitch filter clock)
 *   debounce enable - This bit enables or disables decouncer with the pad
 */
#define _DW2_VALS(deduration, debounen) \
	(PAD_FIELD(DEBONDUR, deduration) | \
	 PAD_FIELD(DEBON, debounen))

#define _PAD_CFG_ATTRS(pad_, term_, dw0_, dw2_, attrs_)			\
	{ 								\
		.pad = pad_,						\
		.attrs = PAD_FIELD(PAD_TERM,  term_) | attrs_,		\
		.dw0 = dw0_,						\
		.dw2 = dw2_,						\
	}

/* Default to ACPI owned. Ownership only matters for GPI pads. */
#define _PAD_CFG(pad_, term_, dw0_, dw2_) \
	_PAD_CFG_ATTRS(pad_, term_, dw0_, dw2_, PAD_FIELD(HOSTSW, ACPI))

/* Native Function - No Rx buffer manipulation */
#define PAD_CFG_NF(pad_, term_, rst_, func_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, func_, NO, NO), \
	_DW2_VALS(NONE, DISABLE))

/* General purpose output. By default no termination. */
#define PAD_CFG_GPO(pad_, val_, rst_, debondu_, debon_) \
	_PAD_CFG(pad_, NONE, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, YES, NO) \
		| PAD_FIELD_VAL(GPIOTXSTATE, val_), \
	_DW2_VALS(debondu_, debon_))

/* General purpose input with no special IRQ routing. */
#define PAD_CFG_GPI(pad_, term_, rst_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, NO, YES),\
	_DW2_VALS(NONE, DISABLE))

/* General purpose input passed through to IOxAPIC. Assume APIC logic can
 * handle polarity/edge/level constraints. */
#define PAD_CFG_GPI_APIC(pad_, term_, rst_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, YES, NO, NO, NO, GPIO, NO, YES), \
	_DW2_VALS(NONE, DISABLE))

/* General purpose input routed to SCI. This assumes edge triggered events. */
#define PAD_CFG_GPI_ACPI_SCI(pad_, term_, rst_, inv_) \
	_PAD_CFG_ATTRS(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, EDGE, NO, inv_, \
		NO, YES, NO, NO, GPIO, NO, YES), PAD_FIELD(HOSTSW, ACPI), \
	_DW2_VALS(NONE, DISABLE))

/* General purpose input routed to SMI. This assumes edge triggered events. */
#define PAD_CFG_GPI_ACPI_SMI(pad_, term_, rst_, inv_) \
	_PAD_CFG_ATTRS(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, EDGE, NO, inv_, \
		NO, NO, YES, NO, GPIO, NO, YES), PAD_FIELD(HOSTSW, ACPI), \
	_DW2_VALS(NONE, DISABLE))

/*
 * The 'attrs' field carries the termination in bits 13:10 to match up with
 * thd DW1 pad configuration register. Additionally, other attributes can
 * be applied such as the ones below. Bit allocation matters.
 */
#define HOSTSW_SHIFT		0
#define HOSTSW_MASK		1
#define HOSTSW_ACPI		HOSTSW_OWN_ACPI
#define HOSTSW_GPIO		HOSTSW_OWN_GPIO

struct pad_config {
	uint16_t pad;
	uint16_t attrs;
	uint32_t dw0;
	uint32_t dw2;
};

/*
 * Depthcharge GPIO interface.
 */

typedef struct GpioCfg {
	GpioOps ops;

	int gpio_num;		/* GPIO number */
	uint32_t *dw_regs;	/* Pointer to DW regs */
	uint32_t current_dw0;	/* Current DW0 register value */

	/* Use to save and restore GPIO configuration */
	uint32_t save_dw0;
	uint32_t save_dw1;
	uint32_t save_dw2;
	CleanupFunc cleanup;

	int (*configure)(struct GpioCfg *, const struct pad_config *,
			size_t num_pads);
} GpioCfg;

GpioCfg *new_icelake_gpio(int gpio_num);
GpioCfg *new_icelake_gpio_input(int gpio_num);
GpioCfg *new_icelake_gpio_output(int gpio_num, unsigned value);
GpioOps *new_icelake_gpio_input_from_coreboot(uint32_t port);

#endif /* __DRIVERS_GPIO_ICELAKE_H__ */
