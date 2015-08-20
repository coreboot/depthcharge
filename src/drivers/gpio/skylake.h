/*
 * Copyright (C) 2015 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#ifndef __DRIVERS_GPIO_SKYLAKE_H__
#define __DRIVERS_GPIO_SKYLAKE_H__

#include <stdint.h>

/*
 * There are 8 GPIO groups. GPP_A -> GPP_G and GPD. GPD is the special case
 * where that group is not so generic. So most of the fixed numbers and macros
 * are based on the GPP groups. The GPIO groups are accessed through register
 * blocks called communities.
 */
#define GPP_A			0
#define GPP_B			1
#define GPP_C			2
#define GPP_D			3
#define GPP_E			4
#define GPP_F			5
#define GPP_G			6
#define GPD			7
#define GPIO_NUM_GROUPS		8
#define GPIO_MAX_NUM_PER_GROUP	24

/*
 * GPIOs are ordered monotonically increasing to match ACPI/OS driver.
 */

/* Group A */
#define GPP_A0			0
#define GPP_A1			1
#define GPP_A2			2
#define GPP_A3			3
#define GPP_A4			4
#define GPP_A5			5
#define GPP_A6			6
#define GPP_A7			7
#define GPP_A8			8
#define GPP_A9			9
#define GPP_A10			10
#define GPP_A11			11
#define GPP_A12			12
#define GPP_A13			13
#define GPP_A14			14
#define GPP_A15			15
#define GPP_A16			16
#define GPP_A17			17
#define GPP_A18			18
#define GPP_A19			19
#define GPP_A20			20
#define GPP_A21			21
#define GPP_A22			22
#define GPP_A23			23
/* Group B */
#define GPP_B0			24
#define GPP_B1			25
#define GPP_B2			26
#define GPP_B3			27
#define GPP_B4			28
#define GPP_B5			29
#define GPP_B6			30
#define GPP_B7			31
#define GPP_B8			32
#define GPP_B9			33
#define GPP_B10			34
#define GPP_B11			35
#define GPP_B12			36
#define GPP_B13			37
#define GPP_B14			38
#define GPP_B15			39
#define GPP_B16			40
#define GPP_B17			41
#define GPP_B18			42
#define GPP_B19			43
#define GPP_B20			44
#define GPP_B21			45
#define GPP_B22			46
#define GPP_B23			47
/* Group C */
#define GPP_C0			48
#define GPP_C1			49
#define GPP_C2			50
#define GPP_C3			51
#define GPP_C4			52
#define GPP_C5			53
#define GPP_C6			54
#define GPP_C7			55
#define GPP_C8			56
#define GPP_C9			57
#define GPP_C10			58
#define GPP_C11			59
#define GPP_C12			60
#define GPP_C13			61
#define GPP_C14			62
#define GPP_C15			63
#define GPP_C16			64
#define GPP_C17			65
#define GPP_C18			66
#define GPP_C19			67
#define GPP_C20			68
#define GPP_C21			69
#define GPP_C22			70
#define GPP_C23			71
/* Group D */
#define GPP_D0			72
#define GPP_D1			73
#define GPP_D2			74
#define GPP_D3			75
#define GPP_D4			76
#define GPP_D5			77
#define GPP_D6			78
#define GPP_D7			79
#define GPP_D8			80
#define GPP_D9			81
#define GPP_D10			82
#define GPP_D11			83
#define GPP_D12			84
#define GPP_D13			85
#define GPP_D14			86
#define GPP_D15			87
#define GPP_D16			88
#define GPP_D17			89
#define GPP_D18			90
#define GPP_D19			91
#define GPP_D20			92
#define GPP_D21			93
#define GPP_D22			94
#define GPP_D23			95
/* Group E */
#define GPP_E0			96
#define GPP_E1			97
#define GPP_E2			98
#define GPP_E3			99
#define GPP_E4			100
#define GPP_E5			101
#define GPP_E6			102
#define GPP_E7			103
#define GPP_E8			104
#define GPP_E9			105
#define GPP_E10			106
#define GPP_E11			107
#define GPP_E12			108
#define GPP_E13			109
#define GPP_E14			110
#define GPP_E15			111
#define GPP_E16			112
#define GPP_E17			113
#define GPP_E18			114
#define GPP_E19			115
#define GPP_E20			116
#define GPP_E21			117
#define GPP_E22			118
#define GPP_E23			119
/* Group F */
#define GPP_F0			120
#define GPP_F1			121
#define GPP_F2			122
#define GPP_F3			123
#define GPP_F4			124
#define GPP_F5			125
#define GPP_F6			126
#define GPP_F7			127
#define GPP_F8			128
#define GPP_F9			129
#define GPP_F10			130
#define GPP_F11			131
#define GPP_F12			132
#define GPP_F13			133
#define GPP_F14			134
#define GPP_F15			135
#define GPP_F16			136
#define GPP_F17			137
#define GPP_F18			138
#define GPP_F19			139
#define GPP_F20			140
#define GPP_F21			141
#define GPP_F22			142
#define GPP_F23			143
/* Group G */
#define GPP_G0			144
#define GPP_G1			145
#define GPP_G2			146
#define GPP_G3			147
#define GPP_G4			148
#define GPP_G5			149
#define GPP_G6			150
#define GPP_G7			151
/* Group GPD  */
#define GPD0			152
#define GPD1			153
#define GPD2			154
#define GPD3			155
#define GPD4			156
#define GPD5			157
#define GPD6			158
#define GPD7			159
#define GPD8			160
#define GPD9			161
#define GPD10			162
#define GPD11			163

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
#define HOSTSW_OWN_REG_OFFSET	0xd0
#define  HOSTSW_OWN_PADS_PER	24
#define  HOSTSW_OWN_ACPI	0
#define  HOSTSW_OWN_GPIO	1
#define PAD_CFG_DW_OFFSET	0x400
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
#define  RXEVCFG_EDGE		1
#define  RXEVCFG_DRIVE0		2
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
#define PAD_CFG_DW_OFFSET	0x400
	/* TERM - termination control */
#define  PAD_TERM_SHIFT		10
#define  PAD_TERM_MASK		0xf
#define  PAD_TERM_NONE		0
#define  PAD_TERM_5K_PD		2
#define  PAD_TERM_1K_PU		9
#define  PAD_TERM_2K_PU		11
#define  PAD_TERM_5K_PU		10
#define  PAD_TERM_20K_PU	12
#define  PAD_TERM_667_PU	13
#define  PAD_TERM_NATIVE	15

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

#define _PAD_CFG_ATTRS(pad_, term_, dw0_, attrs_)			\
	{								\
		.pad = pad_,						\
		.attrs = PAD_FIELD(PAD_TERM,  term_) | attrs_,		\
		.dw0 = dw0_,						\
	}

/* Default to ACPI owned. Ownership only matters for GPI pads. */
#define _PAD_CFG(pad_, term_, dw0_) \
	_PAD_CFG_ATTRS(pad_, term_, dw0_, PAD_FIELD(HOSTSW, ACPI))

/* Native Function - No Rx buffer manipulation */
#define PAD_CFG_NF(pad_, term_, rst_, func_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, func_, NO, NO))

/* General purpose output. By default no termination. */
#define PAD_CFG_GPO(pad_, val_, rst_) \
	_PAD_CFG(pad_, NONE, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, YES, NO) \
		| PAD_FIELD_VAL(GPIOTXSTATE, val_))

/* General purpose input with no special IRQ routing. */
#define PAD_CFG_GPI(pad_, term_, rst_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, NO, YES))

/* General purpose input passed through to IOxAPIC. Assume APIC logic can
 * handle polarity/edge/level constraints. */
#define PAD_CFG_GPI_APIC(pad_, term_, rst_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, YES, NO, NO, NO, GPIO, NO, YES))

/* General purpose input routed to SCI. This assumes edge triggered events. */
#define PAD_CFG_GPI_ACPI_SCI(pad_, term_, rst_, inv_) \
	_PAD_CFG_ATTRS(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, EDGE, NO, inv_, \
		NO, YES, NO, NO, GPIO, NO, YES), PAD_FIELD(HOSTSW, ACPI))

/* General purpose input routed to SMI. This assumes edge triggered events. */
#define PAD_CFG_GPI_ACPI_SMI(pad_, term_, rst_, inv_) \
	_PAD_CFG_ATTRS(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, EDGE, NO, inv_, \
		NO, NO, YES, NO, GPIO, NO, YES), PAD_FIELD(HOSTSW, ACPI))

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
};

/*
 * Depthcharge GPIO interface.
 */

typedef struct GpioCfg {
	GpioOps ops;
	int gpio_num;
	int (*configure)(struct GpioCfg *, const struct pad_config *);
} GpioCfg;

GpioCfg *new_skylake_gpio(int gpio_num);
GpioCfg *new_skylake_gpio_input(int gpio_num);
GpioCfg *new_skylake_gpio_output(int gpio_num);
GpioOps *new_skylake_gpio_input_from_coreboot(uint32_t port);

#endif /* __DRIVERS_GPIO_SKYLAKE_H__ */
