/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 * Copyright 2018 Intel Corporation.
 *
 * GPIO driver for Intel Cannon Lake, Coffee Lake, and Whiskey Lake SOC.
 */

#ifndef __DRIVERS_GPIO_CANNONLAKE_H__
#define __DRIVERS_GPIO_CANNONLAKE_H__

#include <stdint.h>
#include "base/cleanup_funcs.h"
#include "drivers/gpio/gpio.h"

/*
 * Most of the fixed numbers and macros are based on the GPP groups.
 * The GPIO groups are accessed through register blocks called
 * communities.
 */
#define GPP_A			0
#define GPP_B			1
#define GPP_G			2
#define GROUP_SPI		3
#define GPP_D			4
#define GPP_F			5
#define GPP_H			6
#define GROUP_VGPIO		7
#define GPD			9
#define GROUP_AZA		0xA
#define GROUP_CPU		0xB
#define GPP_C			0xC
#define GPP_E			0xD
#define GROUP_JTAG		0xE
#define GROUP_HVMOS		0xF

#define GPIO_NUM_GROUPS		15
#define GPIO_MAX_NUM_PER_GROUP	24

#define GPIO_DWx_WIDTH(x)		(4 * (x +1))

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
#define GPIO_RSVD_0		24
/* Group B */
#define GPP_B0			25
#define GPP_B1			26
#define GPP_B2			27
#define GPP_B3			28
#define GPP_B4			29
#define GPP_B5			30
#define GPP_B6			31
#define GPP_B7			32
#define GPP_B8			33
#define GPP_B9			34
#define GPP_B10			35
#define GPP_B11			36
#define GPP_B12			37
#define GPP_B13			38
#define GPP_B14			39
#define GPP_B15			40
#define GPP_B16			41
#define GPP_B17			42
#define GPP_B18			43
#define GPP_B19			44
#define GPP_B20			45
#define GPP_B21			46
#define GPP_B22			47
#define GPP_B23			48
#define GPIO_RSVD_1		49
#define GPIO_RSVD_2		50
/* Group G */
#define GPP_G0			51
#define GPP_G1			52
#define GPP_G2			53
#define GPP_G3			54
#define GPP_G4			55
#define GPP_G5			56
#define GPP_G6			57
#define GPP_G7			58
/* Group SPI */
#define GPIO_RSVD_3		59
#define GPIO_RSVD_4		60
#define GPIO_RSVD_5		61
#define GPIO_RSVD_6		62
#define GPIO_RSVD_7		63
#define GPIO_RSVD_8		64
#define GPIO_RSVD_9		65
#define GPIO_RSVD_10		66
#define GPIO_RSVD_11		67

#define NUM_GPIO_COM0_PADS	(GPIO_RSVD_11 - GPP_A0 + 1)

/* Group D */
#define GPP_D0			68
#define GPP_D1			69
#define GPP_D2			70
#define GPP_D3			71
#define GPP_D4			72
#define GPP_D5			73
#define GPP_D6			74
#define GPP_D7			75
#define GPP_D8			76
#define GPP_D9			77
#define GPP_D10			78
#define GPP_D11			79
#define GPP_D12			80
#define GPP_D13			81
#define GPP_D14			82
#define GPP_D15			83
#define GPP_D16			84
#define GPP_D17			85
#define GPP_D18			86
#define GPP_D19			87
#define GPP_D20			88
#define GPP_D21			89
#define GPP_D22			90
#define GPP_D23			91
#define GPIO_RSVD_12		92
/* Group F */
#define GPP_F0			93
#define GPP_F1			94
#define GPP_F2			95
#define GPP_F3			96
#define GPP_F4			97
#define GPP_F5			98
#define GPP_F6			99
#define GPP_F7			100
#define GPP_F8			101
#define GPP_F9			102
#define GPP_F10			103
#define GPP_F11			104
#define GPP_F12			105
#define GPP_F13			106
#define GPP_F14			107
#define GPP_F15			108
#define GPP_F16			109
#define GPP_F17			110
#define GPP_F18			111
#define GPP_F19			112
#define GPP_F20			113
#define GPP_F21			114
#define GPP_F22			115
#define GPP_F23			116
/* Group H */
#define GPP_H0			117
#define GPP_H1			118
#define GPP_H2			119
#define GPP_H3			120
#define GPP_H4			121
#define GPP_H5			122
#define GPP_H6			123
#define GPP_H7			124
#define GPP_H8			125
#define GPP_H9			126
#define GPP_H10			127
#define GPP_H11			128
#define GPP_H12			129
#define GPP_H13			130
#define GPP_H14			131
#define GPP_H15			132
#define GPP_H16			133
#define GPP_H17			134
#define GPP_H18			135
#define GPP_H19			136
#define GPP_H20			137
#define GPP_H21			138
#define GPP_H22			139
#define GPP_H23			140
/* Group VGPIO */
#define GPIO_RSVD_13		141
#define GPIO_RSVD_14		142
#define GPIO_RSVD_15		143
#define GPIO_RSVD_16		144
#define GPIO_RSVD_17		145
#define GPIO_RSVD_18		146
#define GPIO_RSVD_19		147
#define GPIO_RSVD_20		148
#define GPIO_RSVD_21		149
#define GPIO_RSVD_22		150
#define GPIO_RSVD_23		151
#define GPIO_RSVD_24		152
#define GPIO_RSVD_25		153
#define GPIO_RSVD_26		154
#define GPIO_RSVD_27		155
#define GPIO_RSVD_28		156
#define GPIO_RSVD_29		157
#define GPIO_RSVD_30		158
#define GPIO_RSVD_31		159
#define GPIO_RSVD_32		160
#define GPIO_RSVD_33		161
#define GPIO_RSVD_34		162
#define GPIO_RSVD_35		163
#define GPIO_RSVD_36		164
#define GPIO_RSVD_37		165
#define GPIO_RSVD_38		166
#define GPIO_RSVD_39		167
#define GPIO_RSVD_40		168
#define GPIO_RSVD_41		169
#define GPIO_RSVD_42		170
#define GPIO_RSVD_43		171
#define GPIO_RSVD_44		172
#define GPIO_RSVD_45		173
#define GPIO_RSVD_46		174
#define GPIO_RSVD_47		175
#define GPIO_RSVD_48		176
#define GPIO_RSVD_49		177
#define GPIO_RSVD_50		178
#define GPIO_RSVD_51		179
#define GPIO_RSVD_52		180

#define NUM_GPIO_COM1_PADS	(GPIO_RSVD_52 - GPP_D0 + 1)

/* Group C */
#define GPP_C0			181
#define GPP_C1			182
#define GPP_C2			183
#define GPP_C3			184
#define GPP_C4			185
#define GPP_C5			186
#define GPP_C6			187
#define GPP_C7			188
#define GPP_C8			189
#define GPP_C9			190
#define GPP_C10			191
#define GPP_C11			192
#define GPP_C12			193
#define GPP_C13			194
#define GPP_C14			195
#define GPP_C15			196
#define GPP_C16			197
#define GPP_C17			198
#define GPP_C18			199
#define GPP_C19			200
#define GPP_C20			201
#define GPP_C21			202
#define GPP_C22			203
#define GPP_C23			204
/* Group E */
#define GPP_E0			205
#define GPP_E1			206
#define GPP_E2			207
#define GPP_E3			208
#define GPP_E4			209
#define GPP_E5			210
#define GPP_E6			211
#define GPP_E7			212
#define GPP_E8			213
#define GPP_E9			214
#define GPP_E10			215
#define GPP_E11			216
#define GPP_E12			217
#define GPP_E13			218
#define GPP_E14			219
#define GPP_E15			220
#define GPP_E16			221
#define GPP_E17			222
#define GPP_E18			223
#define GPP_E19			224
#define GPP_E20			225
#define GPP_E21			226
#define GPP_E22			227
#define GPP_E23			228
/* Group Jtag */
#define GPIO_RSVD_53		229
#define GPIO_RSVD_54		230
#define GPIO_RSVD_55		231
#define GPIO_RSVD_56		232
#define GPIO_RSVD_57		233
#define GPIO_RSVD_58		234
#define GPIO_RSVD_59		235
#define GPIO_RSVD_60		236
#define GPIO_RSVD_61		237
/* Group HVMOS */
#define GPIO_RSVD_62		238
#define GPIO_RSVD_63		239
#define GPIO_RSVD_64		240
#define GPIO_RSVD_65		241
#define GPIO_RSVD_66		242
#define GPIO_RSVD_67		243

#define NUM_GPIO_COM4_PADS	(GPIO_RSVD_67 - GPP_C0 + 1)

/* Group GPD  */
#define GPD0			244
#define GPD1			245
#define GPD2			246
#define GPD3			247
#define GPD4			248
#define GPD5			249
#define GPD6			250
#define GPD7			251
#define GPD8			252
#define GPD9			253
#define GPD10			254
#define GPD11			255

#define NUM_GPIO_COM2_PADS	(GPD11 - GPD0 + 1)

/* Group AZA */
#define HDA_BCLK		256
#define HDA_RSTB		257
#define HDA_SYNC		258
#define HDA_SDO			259
#define HDA_SDI_0		260
#define HDA_SDI_1		261
#define SSP1_SFRM		262
#define SSP1_TXD		263
/* Group CPU */
#define GPIO_RSVD_68		264
#define GPIO_RSVD_69		265
#define GPIO_RSVD_70		266
#define GPIO_RSVD_71		267
#define GPIO_RSVD_72		268
#define GPIO_RSVD_73		269
#define GPIO_RSVD_74		270
#define GPIO_RSVD_75		271
#define GPIO_RSVD_76		272
#define GPIO_RSVD_77		273
#define GPIO_RSVD_78		274

#define NUM_GPIO_COM3_PADS	(GPIO_RSVD_78 - HDA_BCLK + 1)

#define TOTAL_PADS		275

/*
 * Register defines
 */

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

/*
 * DW0
 */

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
#define  RXEVCFG_EDGE		1 /* 0 for rising; 1 for failing */
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

/*
 * DW1
 */

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
 * This encodes all the fields found within the dw2 register for each
 * pad. It directly follows the register specification:
 *   debounce duration - This value shouldn't be less than 3, Time should be
 *                       calculated as (2 ^ debounce) * (glitch filter clock)
 *   debounce enable - This bit enables or disables decouncer with the pad
 */
#define _DW2_VALS(deduration, debounen) \
	(PAD_FIELD(DEBONDUR, deduration) | \
	 PAD_FIELD(DEBON, debounen))

#define _PAD_CFG_ATTRS(pad_, term_, dw0_, dw2_, attrs_)			\
	{								\
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
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, func_, NO, NO),\
	_DW2_VALS(NONE, DISABLE))

/* General purpose output. By default no termination. */
#define PAD_CFG_GPO(pad_, val_, rst_, debondu_, debon_) \
	_PAD_CFG(pad_, NONE, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, YES, NO)\
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
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, YES, NO, NO, NO, GPIO, NO,YES),\
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

	int (*configure)(struct GpioCfg *gpio, const struct pad_config *pad,
			 size_t num_pads);
} GpioCfg;

GpioCfg *new_cannonlake_gpio(int gpio_num);
GpioCfg *new_cannonlake_gpio_input(int gpio_num);
GpioCfg *new_cannonlake_gpio_output(int gpio_num, unsigned int value);
GpioOps *new_cannonlake_gpio_input_from_coreboot(uint32_t port);

#endif /* __DRIVERS_GPIO_CANNONLAKE_H__ */
