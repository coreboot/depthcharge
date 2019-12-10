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
#define GPP_G		 	0x0
#define GPP_B			0x1
#define GPP_A		 	0x2
#define GPP_S			0x3
#define GPP_R			0x4
#define GPP_GPD			0x5
#define GPP_H			0x6
#define GPP_D			0x7
#define GPP_VGPIO		0x8
#define GPP_C			0x9
#define GPP_E			0xA

#define GPIO_NUM_GROUPS		11
#define GPIO_MAX_NUM_PER_GROUP	24

#define GPIO_DWx_WIDTH(x)		(4 * (x +1))
/*
 * GPIOs are ordered monotonically increasing to match ACPI/OS driver.
 */
/* Group B */
#define GPIO_RSVD_0	0
#define GPIO_RSVD_1	1
#define GPIO_RSVD_2	2
#define GPIO_RSVD_3	3
#define GPIO_RSVD_4	4
#define GPIO_RSVD_5	5
#define GPIO_RSVD_6	6
#define GPIO_RSVD_7	7
#define GPIO_RSVD_8	8
#define GPP_B0	9
#define GPP_B1	10
#define GPP_B2	11
#define GPP_B3	12
#define GPP_B4	13
#define GPP_B5	14
#define GPP_B6	15
#define GPP_B7	16
#define GPP_B8	17
#define GPP_B9	18
#define GPP_B10	19
#define GPP_B11	20
#define GPP_B12	21
#define GPP_B13	22
#define GPP_B14	23
#define GPP_B15	24
#define GPP_B16	25
#define GPP_B17	26
#define GPP_B18	27
#define GPP_B19	28
#define GPP_B20	29
#define GPP_B21	30
#define GPP_B22	31
#define GPP_B23	32
#define GPIO_RSVD_9	33
#define GPIO_RSVD_10	34

/* Group A */
#define GPP_A0	35
#define GPP_A1	36
#define GPP_A2	37
#define GPP_A3	38
#define GPP_A4	39
#define GPP_A5	40
#define GPP_A6	41
#define GPP_A7	42
#define GPP_A8	43
#define GPP_A9	44
#define GPP_A10	45
#define GPP_A11	46
#define GPP_A12	47
#define GPP_A13	48
#define GPP_A14	49
#define GPP_A15	50
#define GPP_A16	51
#define GPP_A17	52
#define GPP_A18	53
#define GPP_A19	54
#define GPIO_RSVD_11	55

/* Group S */
#define GPP_S0	56
#define GPP_S1	57
#define GPP_S2	58
#define GPP_S3	59
#define GPP_S4	60
#define GPP_S5	61
#define GPP_S6	62
#define GPP_S7	63

/* Group R */
#define GPP_R0	64
#define GPP_R1	65
#define GPP_R2	66
#define GPP_R3	67
#define GPP_R4	68
#define GPP_R5	69
#define GPP_R6	70
#define GPP_R7	71

#define NUM_GPIO_COM0_PADS	(GPP_R7 - GPIO_RSVD_0 + 1)

/* Group H */
#define GPP_H0	72
#define GPP_H1	73
#define GPP_H2	74
#define GPP_H3	75
#define GPP_H4	76
#define GPP_H5	77
#define GPP_H6	78
#define GPP_H7	79
#define GPP_H8	80
#define GPP_H9	81
#define GPP_H10	82
#define GPP_H11	83
#define GPP_H12	84
#define GPP_H13	85
#define GPP_H14	86
#define GPP_H15	87
#define GPP_H16	88
#define GPP_H17	89
#define GPP_H18	90
#define GPP_H19	91
#define GPP_H20	92
#define GPP_H21	93
#define GPP_H22	94
#define GPP_H23	95

/* Group D */
#define GPP_D0	96
#define GPP_D1	97
#define GPP_D2	98
#define GPP_D3	99
#define GPP_D4	100
#define GPP_D5	101
#define GPP_D6	102
#define GPP_D7	103
#define GPP_D8	104
#define GPP_D9	105
#define GPP_D10	106
#define GPP_D11	107
#define GPP_D12	108
#define GPP_D13	109
#define GPP_D14	110
#define GPP_D15	111
#define GPP_D16	112
#define GPP_D17	113
#define GPP_D18	114
#define GPP_D19	115
#define GPP_D20	116
#define GPP_D21	117
#define GPP_D22	118
#define GPP_D23	119
#define GPIO_RSVD_12	120
#define GPIO_RSVD_13	121

/* Group VGPIO */
#define vGPIO_0	122
#define vGPIO_3	123
#define vGPIO_4	124
#define vGPIO_5	125
#define vGPIO_6	126
#define vGPIO_7	127
#define vGPIO_8	128
#define vGPIO_9	129
#define vGPIO_10	130
#define vGPIO_11	131
#define vGPIO_12	132
#define vGPIO_13	133
#define vGPIO_18	134
#define vGPIO_19	135
#define vGPIO_20	136
#define vGPIO_21	137
#define vGPIO_22	138
#define vGPIO_23	139
#define vGPIO_24	140
#define vGPIO_25	141
#define vGPIO_30	142
#define vGPIO_31	143
#define vGPIO_32	144
#define vGPIO_33	145
#define vGPIO_34	146
#define vGPIO_35	147
#define vGPIO_36	148
#define vGPIO_37	149
#define vGPIO_39	150

/* Group C */
#define GPP_C0	151
#define GPP_C1	152
#define GPP_C2	153
#define GPP_C3	154
#define GPP_C4	155
#define GPP_C5	156
#define GPP_C6	157
#define GPP_C7	158
#define GPP_C8	159
#define GPP_C9	160
#define GPP_C10	161
#define GPP_C11	162
#define GPP_C12	163
#define GPP_C13	164
#define GPP_C14	165
#define GPP_C15	166
#define GPP_C16	167
#define GPP_C17	168
#define GPP_C18	169
#define GPP_C19	170
#define GPP_C20	171
#define GPP_C21	172
#define GPP_C22	173
#define GPP_C23	174

#define NUM_GPIO_COM1_PADS	(GPP_C23 - GPP_H0 + 1)

/* Group GPD */
#define GPD0	175
#define GPD1	176
#define GPD2	177
#define GPD3	178
#define GPD4	179
#define GPD5	180
#define GPD6	181
#define GPD7	182
#define GPD8	183
#define GPD9	184
#define GPD10	185
#define GPIO_RSVD_14	186
#define GPIO_RSVD_15	187
#define GPIO_RSVD_16	188
#define GPIO_RSVD_17	189

#define NUM_GPIO_COM2_PADS	(GPIO_RSVD_17 - GPD0 + 1)

/* Group E */
#define GPIO_RSVD_18	190
#define GPIO_RSVD_19	191
#define GPIO_RSVD_20	192
#define GPIO_RSVD_21	193
#define GPIO_RSVD_22	194
#define GPIO_RSVD_23	195
#define GPP_E0	196
#define GPP_E1	197
#define GPP_E2	198
#define GPP_E3	199
#define GPP_E4	200
#define GPP_E5	201
#define GPP_E6	202
#define GPP_E7	203
#define GPP_E8	204
#define GPP_E9	205
#define GPP_E10	206
#define GPP_E11	207
#define GPP_E12	208
#define GPP_E13	209
#define GPP_E14	210
#define GPP_E15	211
#define GPP_E16	212
#define GPP_E17	213
#define GPP_E18	214
#define GPP_E19	215
#define GPP_E20	216
#define GPP_E21	217
#define GPP_E22	218
#define GPP_E23	219
#define GPIO_RSVD_24	220
#define GPIO_RSVD_25	221
#define GPIO_RSVD_26	222
#define GPIO_RSVD_27	223
#define GPIO_RSVD_28	224
#define GPIO_RSVD_29	225
#define GPIO_RSVD_30	226
#define GPIO_RSVD_31	227
#define GPIO_RSVD_32	228
#define GPIO_RSVD_33	229
#define GPIO_RSVD_34	230
#define GPIO_RSVD_35	231
#define GPIO_RSVD_36	232

#define NUM_GPIO_COM4_PADS	(GPIO_RSVD_36 - GPIO_RSVD_18 + 1)

/* Group G */
#define GPP_G0	233
#define GPP_G1	234
#define GPP_G2	235
#define GPP_G3	236
#define GPP_G4	237
#define GPP_G5	238
#define GPP_G6	239
#define GPP_G7	240

#define NUM_GPIO_COM5_PADS	(GPP_G7 - GPP_G0 + 1)

#define TOTAL_PADS	241

#define COMM_0		0
#define COMM_1		1
#define COMM_2		2
#define COMM_3		3
#define COMM_4		4
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

GpioCfg *new_jasperlake_gpio(int gpio_num);
GpioCfg *new_jasperlake_gpio_input(int gpio_num);
GpioCfg *new_jasperlake_gpio_output(int gpio_num, unsigned value);
GpioOps *new_jasperlake_gpio_input_from_coreboot(uint32_t port);


#endif
