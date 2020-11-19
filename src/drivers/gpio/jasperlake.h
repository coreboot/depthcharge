/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2019 Intel Corporation.
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

#ifndef _DRIVERS_GPIO_JASPERLAKE_H_
#define _DRIVERS_GPIO_JASPERLAKE_H_

#include <stdint.h>
#include "base/cleanup_funcs.h"
#include "drivers/gpio/gpio.h"

/*
 * Most of the fixed numbers and macros are based on the GPP groups.
 * The GPIO groups are accessed through register blocks called
 * communities.
 */

#define GPP_A			0x0
#define GPP_B			0x1
#define GPP_G			0x2
#define GPP_C			0x3
#define GPP_R			0x4
#define GPP_D			0x5
#define GPP_S			0x6
#define GPP_H			0x7
#define GPP_VGPIO		0x8
#define GPP_F			0x9
#define GPP_GPD			0xA
#define GPP_E			0xD

#define GPIO_NUM_GROUPS		12
#define GPIO_MAX_NUM_PER_GROUP	24
#define GPIO_DWx_WIDTH(x)	(4 * (x +1))

/*
 * GPIOs are ordered monotonically increasing to match ACPI/OS driver.
 */

/* Group F */
#define GPP_F0		0
#define GPP_F1		1
#define GPP_F2		2
#define GPP_F3		3
#define GPP_F4		4
#define GPP_F5		5
#define GPP_F6		6
#define GPP_F7		7
#define GPP_F8		8
#define GPP_F9		9
#define GPP_F10		10
#define GPP_F11		11
#define GPP_F12		12
#define GPP_F13		13
#define GPP_F14		14
#define GPP_F15		15
#define GPP_F16		16
#define GPP_F17		17
#define GPP_F18		18
#define GPP_F19		19

/* Group B */
#define GPIO_SPI0_IO_2		20
#define GPIO_SPI0_IO_3		21
#define GPIO_SPI0_MOSI_IO_0	22
#define GPIO_SPI0_MOSI_IO_1	23
#define GPIO_SPI0_TPM_CSB	24
#define GPIO_SPI0_FLASH_0_CSB	25
#define GPIO_SPI0_FLASH_1_CSB	26
#define GPIO_SPI0_CLK		27
#define GPIO_SPI0_CLK_LOOPBK	28
#define GPP_B0			29
#define GPP_B1			30
#define GPP_B2			31
#define GPP_B3			32
#define GPP_B4			33
#define GPP_B5			34
#define GPP_B6			35
#define GPP_B7			36
#define GPP_B8			37
#define GPP_B9			38
#define GPP_B10			39
#define GPP_B11			40
#define GPP_B12			41
#define GPP_B13			42
#define GPP_B14			43
#define GPP_B15			44
#define GPP_B16			45
#define GPP_B17			46
#define GPP_B18			47
#define GPP_B19			48
#define GPP_B20			49
#define GPP_B21			50
#define GPP_B22			51
#define GPP_B23			52
#define GPIO_GSPI0_CLK_LOOPBK	53
#define GPIO_GSPI1_CLK_LOOPBK	54

/* Group A */
#define GPP_A0			55
#define GPP_A1			56
#define GPP_A2			57
#define GPP_A3			58
#define GPP_A4			59
#define GPP_A5			60
#define GPP_A6			61
#define GPP_A7			62
#define GPP_A8			63
#define GPP_A9			64
#define GPP_A10			65
#define GPP_A11			66
#define GPP_A12			67
#define GPP_A13			68
#define GPP_A14			69
#define GPP_A15			70
#define GPP_A16			71
#define GPP_A17			72
#define GPP_A18			73
#define GPP_A19			74
#define GPIO_ESPI_CLK_LOOPBK	75

/* Group S */
#define GPP_S0		76
#define GPP_S1		77
#define GPP_S2		78
#define GPP_S3		79
#define GPP_S4		80
#define GPP_S5		81
#define GPP_S6		82
#define GPP_S7		83

/* Group R */
#define GPP_R0		84
#define GPP_R1		85
#define GPP_R2		86
#define GPP_R3		87
#define GPP_R4		88
#define GPP_R5		89
#define GPP_R6		90
#define GPP_R7		91

#define GPIO_COM0_START		GPP_F0
#define GPIO_COM0_END		GPP_R7
#define NUM_GPIO_COM0_PADS	(GPIO_COM0_END - GPIO_COM0_START + 1)

/* Group H */
#define GPP_H0		92
#define GPP_H1		93
#define GPP_H2		94
#define GPP_H3		95
#define GPP_H4		96
#define GPP_H5		97
#define GPP_H6		98
#define GPP_H7		99
#define GPP_H8		100
#define GPP_H9		101
#define GPP_H10		102
#define GPP_H11		103
#define GPP_H12		104
#define GPP_H13		105
#define GPP_H14		106
#define GPP_H15		107
#define GPP_H16		108
#define GPP_H17		109
#define GPP_H18		110
#define GPP_H19		111
#define GPP_H20		112
#define GPP_H21		113
#define GPP_H22		114
#define GPP_H23		115

/* Group D */
#define GPP_D0			116
#define GPP_D1			117
#define GPP_D2			118
#define GPP_D3			119
#define GPP_D4			120
#define GPP_D5			121
#define GPP_D6			122
#define GPP_D7			123
#define GPP_D8			124
#define GPP_D9			125
#define GPP_D10			126
#define GPP_D11			127
#define GPP_D12			128
#define GPP_D13			129
#define GPP_D14			130
#define GPP_D15			131
#define GPP_D16			132
#define GPP_D17			133
#define GPP_D18			134
#define GPP_D19			135
#define GPP_D20			136
#define GPP_D21			137
#define GPP_D22			138
#define GPP_D23			139
#define GPIO_GSPI2_CLK_LOOPBK	140
#define GPIO_SPI1_CLK_LOOPBK	141

/* Group VGPIO */
#define VGPIO_0		142
#define VGPIO_3		143
#define VGPIO_4		144
#define VGPIO_5		145
#define VGPIO_6		146
#define VGPIO_7		147
#define VGPIO_8		148
#define VGPIO_9		149
#define VGPIO_10	150
#define VGPIO_11	151
#define VGPIO_12	152
#define VGPIO_13	153
#define VGPIO_18	154
#define VGPIO_19	155
#define VGPIO_20	156
#define VGPIO_21	157
#define VGPIO_22	158
#define VGPIO_23	159
#define VGPIO_24	160
#define VGPIO_25	161
#define VGPIO_30	162
#define VGPIO_31	163
#define VGPIO_32	164
#define VGPIO_33	165
#define VGPIO_34	166
#define VGPIO_35	167
#define VGPIO_36	168
#define VGPIO_37	169
#define VGPIO_39	170

/* Group C */
#define GPP_C0		171
#define GPP_C1		172
#define GPP_C2		173
#define GPP_C3		174
#define GPP_C4		175
#define GPP_C5		176
#define GPP_C6		177
#define GPP_C7		178
#define GPP_C8		179
#define GPP_C9		180
#define GPP_C10		181
#define GPP_C11		182
#define GPP_C12		183
#define GPP_C13		184
#define GPP_C14		185
#define GPP_C15		186
#define GPP_C16		187
#define GPP_C17		188
#define GPP_C18		189
#define GPP_C19		190
#define GPP_C20		191
#define GPP_C21		192
#define GPP_C22		193
#define GPP_C23		194

#define GPIO_COM1_START		GPP_H0
#define GPIO_COM1_END		GPP_C23
#define NUM_GPIO_COM1_PADS	(GPIO_COM1_END - GPIO_COM1_START + 1)

/* Group GPD */
#define GPD0			195
#define GPD1			196
#define GPD2			197
#define GPD3			198
#define GPD4			199
#define GPD5			200
#define GPD6			201
#define GPD7			202
#define GPD8			203
#define GPD9			204
#define GPD10			205
#define GPIO_INPUT3VSEL		206
#define GPIO_SLP_SUSB		207
#define GPIO_WAKEB		208
#define GPIO_DRAM_RESETB	209

#define GPIO_COM2_START		GPD0
#define GPIO_COM2_END		GPIO_DRAM_RESETB
#define NUM_GPIO_COM2_PADS	(GPIO_COM2_END - GPIO_COM2_START + 1)

/* Group E */
#define GPIO_L_BKLTEN	210
#define GPIO_L_BKLTCTL	211
#define GPIO_L_VDDEN	212
#define GPIO_SYS_PWROK	213
#define GPIO_SYS_RESETB	214
#define GPIO_MLK_RSTB	215
#define GPP_E0		216
#define GPP_E1		217
#define GPP_E2		218
#define GPP_E3		219
#define GPP_E4		220
#define GPP_E5		221
#define GPP_E6		222
#define GPP_E7		223
#define GPP_E8		224
#define GPP_E9		225
#define GPP_E10		226
#define GPP_E11		227
#define GPP_E12		228
#define GPP_E13		229
#define GPP_E14		230
#define GPP_E15		231
#define GPP_E16		232
#define GPP_E17		233
#define GPP_E18		234
#define GPP_E19		235
#define GPP_E20		236
#define GPP_E21		237
#define GPP_E22		238
#define GPP_E23		239

#define GPIO_COM4_START		GPIO_L_BKLTEN
#define GPIO_COM4_END		GPP_E23
#define NUM_GPIO_COM4_PADS	(GPIO_COM4_END - GPIO_COM4_START + 1)

/* Group G */
#define GPP_G0		240
#define GPP_G1		241
#define GPP_G2		242
#define GPP_G3		243
#define GPP_G4		244
#define GPP_G5		245
#define GPP_G6		246
#define GPP_G7		247

#define GPIO_COM5_START		GPP_G0
#define GPIO_COM5_END		GPP_G7
#define NUM_GPIO_COM5_PADS	(GPIO_COM5_END - GPIO_COM5_START + 1)

#define TOTAL_PADS	248

#define COMM_0		0
#define COMM_1		1
#define COMM_2		2
#define COMM_4		3
#define COMM_5		4
#define TOTAL_GPIO_COMM	5

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
#define HOSTSW_OWN_REG_OFFSET	0xc0
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

GpioCfg *new_jasperlake_gpio(int gpio_num);
GpioCfg *new_jasperlake_gpio_input(int gpio_num);
GpioCfg *new_jasperlake_gpio_output(int gpio_num, unsigned value);
GpioOps *new_jasperlake_gpio_input_from_coreboot(uint32_t port);


#endif
