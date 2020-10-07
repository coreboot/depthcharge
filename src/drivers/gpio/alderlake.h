/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Intel Corporation.
 *
 * GPIO driver for Intel Alder Lake SOC.
 */

#ifndef __DRIVERS_GPIO_ALDERLAKE_H__
#define __DRIVERS_GPIO_ALDERLAKE_H__

#include <stdint.h>
#include "base/cleanup_funcs.h"
#include "drivers/gpio/gpio.h"

/*
 * Most of the fixed numbers and macros are based on the GPP groups.
 * The GPIO groups are accessed through register blocks called
 * communities.
 */
/* GPIO COMM 0 */
#define GPP_B			0x0
#define GPP_T			0x1
#define GPP_A			0x2
/* GPIO COMM 1 */
#define GPP_S			0x3
#define GPP_H			0x4
#define GPP_D			0x5
/* GPIO COMM 2 */
#define GPD			0x6
/* GPIO COMM 4 */
#define GPP_C			0x7
#define GPP_F			0x8
#define GPP_HVMOS		0x9
#define GPP_E			0xA
/* GPIO COMM 5 */
#define GPP_R			0xB
#define GPP_SPI0		0xC

#define GPIO_MAX_NUM_PER_GROUP	26

#define GPIO_DWx_WIDTH(x)	(4 * (x + 1))

/*
 * GPIOs are ordered monotonically increasing to match ACPI/OS driver.
 */
/* Group B */
#define GPP_B0			0
#define GPP_B1			1
#define GPP_B2			2
#define GPP_B3			3
#define GPP_B4			4
#define GPP_B5			5
#define GPP_B6			6
#define GPP_B7			7
#define GPP_B8			8
#define GPP_B9			9
#define GPP_B10			10
#define GPP_B11			11
#define GPP_B12			12
#define GPP_B13			13
#define GPP_B14			14
#define GPP_B15			15
#define GPP_B16			16
#define GPP_B17			17
#define GPP_B18			18
#define GPP_B19			19
#define GPP_B20			20
#define GPP_B21			21
#define GPP_B22			22
#define GPP_B23			23
#define GPP_B24			24 /* GSPI0_CLK_LOOPBK */
#define GPP_B25			25 /* GSPI1_CLK_LOOPBK */

/* Group T */
#define GPP_T0			26
#define GPP_T1			27
#define GPP_T2			28
#define GPP_T3			29
#define GPP_T4			30
#define GPP_T5			31
#define GPP_T6			32
#define GPP_T7			33
#define GPP_T8			34
#define GPP_T9			35
#define GPP_T10			36
#define GPP_T11			37
#define GPP_T12			38
#define GPP_T13			39
#define GPP_T14			40
#define GPP_T15			41

/* Group A */
#define GPP_A0			42
#define GPP_A1			43
#define GPP_A2			44
#define GPP_A3			45
#define GPP_A4			46
#define GPP_A5			47
#define GPP_A6			48
#define GPP_A7			49
#define GPP_A8			50
#define GPP_A9			51
#define GPP_A10			52
#define GPP_A11			53
#define GPP_A12			54
#define GPP_A13			55
#define GPP_A14			56
#define GPP_A15			57
#define GPP_A16			58
#define GPP_A17			59
#define GPP_A18			60
#define GPP_A19			61
#define GPP_A20			62
#define GPP_A21			63
#define GPP_A22			64
#define GPP_A23			65
#define GPP_ESPI_CLK_LOOPBK	66

#define GPIO_COM0_START		GPP_B0
#define GPIO_COM0_END		GPP_ESPI_CLK_LOOPBK
#define NUM_GPIO_COM0_PADS	(GPIO_COM0_END - GPIO_COM0_START + 1)

/* Group S */
#define GPP_S0			67
#define GPP_S1			68
#define GPP_S2			69
#define GPP_S3			70
#define GPP_S4			71
#define GPP_S5			72
#define GPP_S6			73
#define GPP_S7			74

/* Group H */
#define GPP_H0			75
#define GPP_H1			76
#define GPP_H2			77
#define GPP_H3			78
#define GPP_H4			79
#define GPP_H5			80
#define GPP_H6			81
#define GPP_H7			82
#define GPP_H8			83
#define GPP_H9			84
#define GPP_H10			85
#define GPP_H11			86
#define GPP_H12			87
#define GPP_H13			88
#define GPP_H14			89
#define GPP_H15			90
#define GPP_H16			91
#define GPP_H17			92
#define GPP_H18			93
#define GPP_H19			94
#define GPP_H20			95
#define GPP_H21			96
#define GPP_H22			97
#define GPP_H23			98

/* Group D */
#define GPP_D0			99
#define GPP_D1			100
#define GPP_D2			101
#define GPP_D3			102
#define GPP_D4			103
#define GPP_D5			104
#define GPP_D6			105
#define GPP_D7			106
#define GPP_D8			107
#define GPP_D9			108
#define GPP_D10			109
#define GPP_D11			110
#define GPP_D12			111
#define GPP_D13			112
#define GPP_D14			113
#define GPP_D15			114
#define GPP_D16			115
#define GPP_D17			116
#define GPP_D18			117
#define GPP_D19			118
#define GPP_GSPI2_CLK_LOOPBK	119

#define GPIO_COM1_START		GPP_S0
#define GPIO_COM1_END		GPP_GSPI2_CLK_LOOPBK
#define NUM_GPIO_COM1_PADS	(GPIO_COM1_END - GPIO_COM1_START + 1)

/* Group GPD */
#define GPD0			120
#define GPD1			121
#define GPD2			122
#define GPD3			123
#define GPD4			124
#define GPD5			125
#define GPD6			126
#define GPD7			127
#define GPD8			128
#define GPD9			129
#define GPD10			130
#define GPD11			131
#define GPD_INPUT3VSEL		132
#define GPD_SLP_LANB		133
#define GPD_SLP_SUSB		134
#define GPD_WAKEB		135
#define GPD_DRAM_RESETB		136

#define GPIO_COM2_START		GPD0
#define GPIO_COM2_END		GPD_DRAM_RESETB
#define NUM_GPIO_COM2_PADS	(GPIO_COM2_END - GPIO_COM2_START + 1)

/* Group C */
#define GPP_C0			137
#define GPP_C1			138
#define GPP_C2			139
#define GPP_C3			140
#define GPP_C4			141
#define GPP_C5			142
#define GPP_C6			143
#define GPP_C7			144
#define GPP_C8			145
#define GPP_C9			146
#define GPP_C10			147
#define GPP_C11			148
#define GPP_C12			149
#define GPP_C13			150
#define GPP_C14			151
#define GPP_C15			152
#define GPP_C16			153
#define GPP_C17			154
#define GPP_C18			155
#define GPP_C19			156
#define GPP_C20			157
#define GPP_C21			158
#define GPP_C22			159
#define GPP_C23			160

/* Group F */
#define GPP_F0			161
#define GPP_F1			162
#define GPP_F2			163
#define GPP_F3			164
#define GPP_F4			165
#define GPP_F5			166
#define GPP_F6			167
#define GPP_F7			168
#define GPP_F8			169
#define GPP_F9			170
#define GPP_F10			171
#define GPP_F11			172
#define GPP_F12			173
#define GPP_F13			174
#define GPP_F14			175
#define GPP_F15			176
#define GPP_F16			177
#define GPP_F17			178
#define GPP_F18			179
#define GPP_F19			180
#define GPP_F20			181
#define GPP_F21			182
#define GPP_F22			183
#define GPP_F23			184
#define GPP_F_CLK_LOOPBK	185

/* Group HVMOS */
#define GPP_L_BKLTEN		186
#define GPP_L_BKLTCTL		187
#define GPP_L_VDDEN		188
#define GPP_SYS_PWROK		189
#define GPP_SYS_RESETB		190
#define GPP_MLK_RSTB		191

/* Group E */
#define GPP_E0			192
#define GPP_E1			193
#define GPP_E2			194
#define GPP_E3			195
#define GPP_E4			196
#define GPP_E5			197
#define GPP_E6			198
#define GPP_E7			199
#define GPP_E8			200
#define GPP_E9			201
#define GPP_E10			202
#define GPP_E11			203
#define GPP_E12			204
#define GPP_E13			205
#define GPP_E14			206
#define GPP_E15			207
#define GPP_E16			208
#define GPP_E17			209
#define GPP_E18			210
#define GPP_E19			211
#define GPP_E20			212
#define GPP_E21			213
#define GPP_E22			214
#define GPP_E23			215
#define GPP_E_CLK_LOOPBK	216

#define GPIO_COM4_START		GPP_C0
#define GPIO_COM4_END		GPP_E_CLK_LOOPBK
#define NUM_GPIO_COM4_PADS	(GPIO_COM4_END - GPIO_COM4_START + 1)

/* Group R */
#define GPP_R0			217
#define GPP_R1			218
#define GPP_R2			219
#define GPP_R3			220
#define GPP_R4			221
#define GPP_R5			222
#define GPP_R6			223
#define GPP_R7			224

/* Group SPI0 */
#define GPP_SPI0_IO_2		225
#define GPP_SPI0_IO_3		226
#define GPP_SPI0_MOSI_IO_0	227
#define GPP_SPI0_MOSI_IO_1	228
#define GPP_SPI0_TPM_CSB	229
#define GPP_SPI0_FLASH_0_CSB	230
#define GPP_SPI0_FLASH_1_CSB	231
#define GPP_SPI0_CLK		232

#define GPIO_COM5_START		GPP_R0
#define GPIO_COM5_END		GPP_SPI0_CLK
#define NUM_GPIO_COM5_PADS	(GPIO_COM5_END - GPIO_COM5_START + 1)

#define TOTAL_GPIO_COMM		(COMM_5 + 1)
#define TOTAL_PADS		(GPIO_COM5_END + 1)

#define	PAD_CFG_DW_OFFSET	0x700
#define	HOSTSW_OWN_PADS_PER	24
#define	HOSTSW_OWN_REG_OFFSET	0xb0
#define	HOSTSW_OWN_GPIO		1
#define	HOSTSW_OWN_ACPI		0

/*
 * The 'attrs' field carries the termination in bits 13:10 to match up with
 * thd DW1 pad configuration register. Additionally, other attributes can
 * be applied such as the ones below. Bit allocation matters.
 */
#define	HOSTSW_SHIFT		0
#define	HOSTSW_MASK		1
#define	HOSTSW_ACPI		HOSTSW_OWN_ACPI
#define	HOSTSW_GPIO		HOSTSW_OWN_GPIO

struct pad_config {
	uint16_t pad;
	uint16_t attrs;
	uint32_t dw0;
	uint32_t dw1;
	uint32_t dw2;
};

#define PAD_FIELD_VAL(field_, val_) \
	(((val_) & field_ ## _MASK) << field_ ## _SHIFT)

#define PAD_FIELD(field_, setting_) \
	PAD_FIELD_VAL(field_, field_ ## _ ## setting_)


/*
 * DW0
 */
/* PADRSTCFG - when to reset the pad config */
#define PADRSTCFG_SHIFT         30
#define PADRSTCFG_MASK          0x3
#if CONFIG(DW0_GPD)
#define	PADRSTCFG_DSW_PWROK	0
#define	PADRSTCFG_DEEP		1
#define	PADRSTCFG_PLTRST	2
#define	PADRSTCFG_RSMRST	3
#else
#define	PADRSTCFG_RSMRST	0
#define	PADRSTCFG_DEEP		1
#define	PADRSTCFG_PLTRST	2
#define	PADRSTCFG_RSVD		3
#endif

/* RXPADSTSEL - raw signal or internal state */
#define	RXPADSTSEL_SHIFT	29
#define	RXPADSTSEL_MASK		0x1
#define	RXPADSTSEL_RAW		0
#define	RXPADSTSEL_INTERNAL	1
/* RXRAW1 - drive 1 instead instead of pad value */
#define	RXRAW1_SHIFT		28
#define	RXRAW1_MASK		0x1
#define	RXRAW1_NO		0
#define	RXRAW1_YES		1
/* RXEVCFG - Interrupt and wake types */
#define	RXEVCFG_SHIFT		25
#define	RXEVCFG_MASK		0x3
#define	RXEVCFG_LEVEL		0
#define	RXEVCFG_EDGE		1
#define	RXEVCFG_DISABLE		2
#define	RXEVCFG_EITHEREDGE	3
/* PREGFRXSEL - use filtering on Rx pad */
#define	PREGFRXSEL_SHIFT	24
#define	PREGFRXSEL_MASK		0x1
#define	PREGFRXSEL_NO		0
#define	PREGFRXSEL_YES		1
/* RXINV - invert signal to SMI, SCI, NMI, or IRQ routing. */
#define	RXINV_SHIFT		23
#define	RXINV_MASK		0x1
#define	RXINV_NO		0
#define	RXINV_YES		1
/* GPIROUTIOXAPIC - route to io-xapic or not */
#define	GPIROUTIOXAPIC_SHIFT	20
#define	GPIROUTIOXAPIC_MASK	0x1
#define	GPIROUTIOXAPIC_NO	0
#define	GPIROUTIOXAPIC_YES	1
/* GPIROUTSCI - route to SCI */
#define	GPIROUTSCI_SHIFT	19
#define	GPIROUTSCI_MASK		0x1
#define	GPIROUTSCI_NO		0
#define	GPIROUTSCI_YES		1
/* GPIROUTSMI - route to SMI */
#define	GPIROUTSMI_SHIFT	18
#define	GPIROUTSMI_MASK		0x1
#define	GPIROUTSMI_NO		0
#define	GPIROUTSMI_YES		1
/* GPIROUTNMI - route to NMI */
#define	GPIROUTNMI_SHIFT	17
#define	GPIROUTNMI_MASK		0x1
#define	GPIROUTNMI_NO		0
#define	GPIROUTNMI_YES		1
/* PMODE - mode of pad */
#define	PMODE_SHIFT		10
#define	PMODE_MASK		0x7
#define	PMODE_GPIO		0
#define	PMODE_NF1		1
#define	PMODE_NF2		2
#define	PMODE_NF3		3
#define	PMODE_NF4		4
#define	PMODE_NF5		5
#define	PMODE_NF6		6
#define	PMODE_NF7		7
/* GPIORXDIS - Disable Rx */
#define	GPIORXDIS_SHIFT		9
#define	GPIORXDIS_MASK		0x1
#define	GPIORXDIS_NO		0
#define	GPIORXDIS_YES		1
/* GPIOTXDIS - Disable Tx */
#define	GPIOTXDIS_SHIFT		8
#define	GPIOTXDIS_MASK		0x1
#define	GPIOTXDIS_NO		0
#define	GPIOTXDIS_YES		1
/* GPIORXSTATE - Internal state after glitch filter */
#define	GPIORXSTATE_SHIFT	1
#define	GPIORXSTATE_MASK	0x1
/* GPIOTXSTATE - Drive value onto pad */
#define	GPIOTXSTATE_SHIFT	0
#define	GPIOTXSTATE_MASK	0x1

/*
 * DW1
 */

/* TERM - termination control */
#define	PAD_TERM_SHIFT		10
#define	PAD_TERM_MASK		0xf
#define	PAD_TERM_NONE		0
#define	PAD_TERM_20K_PD		4
#define	PAD_TERM_20K_PU		12
#define	PAD_TERM_NATIVE		15

/*
 * DW2: Only applicable to GPD_3
 */
/* DEBONDUR - Debounce duration */
#define	DEBONDUR_SHIFT		1
#define	DEBONDUR_MASK		0xf
#define	DEBONDUR_NONE		0
#define	DEBONDUR_DEBOUNCE_3	3
#define	DEBONDUR_DEBOUNCE_4	4
#define	DEBONDUR_DEBOUNCE_5	5
#define	DEBONDUR_DEBOUNCE_6	6
#define	DEBONDUR_DEBOUNCE_7	7
#define	DEBONDUR_DEBOUNCE_8	8
#define	DEBONDUR_DEBOUNCE_9	9
#define	DEBONDUR_DEBOUNCE_10	10
#define	DEBONDUR_DEBOUNCE_11	11
#define	DEBONDUR_DEBOUNCE_12	12
#define	DEBONDUR_DEBOUNCE_13	13
#define	DEBONDUR_DEBOUNCE_14	14
#define	DEBONDUR_DEBOUNCE_15	15
/* DEBONEN - Debounce enable */
#define	DEBON_SHIFT		0
#define	DEBON_MASK		1
#define	DEBON_DISABLE		0
#define	DEBON_ENABLE		1

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

#define _DW1_VALS(term) \
	(PAD_FIELD(PAD_TERM, term))
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

#define _PAD_CFG_ATTRS(pad_, term_, dw0_, dw1_, dw2_, attrs_)		\
	{								\
		.pad = pad_,						\
		.attrs = PAD_FIELD(PAD_TERM,  term_) | attrs_,		\
		.dw0 = dw0_,						\
                .dw1 = dw1_,						\
                .dw2 = dw2_,						\
	}

/* Default to ACPI owned. Ownership only matters for GPI pads. */
#define _PAD_CFG(pad_, term_, dw0_, dw1_, dw2_) \
	_PAD_CFG_ATTRS(pad_, term_, dw0_, dw1_, dw2_, PAD_FIELD(HOSTSW, ACPI))

/* General purpose output. By default no termination. */
#define PAD_CFG_GPO(pad_, val_, rst_, debondu_, debon_) \
	_PAD_CFG(pad_, NONE, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, YES, NO) \
		| PAD_FIELD_VAL(GPIOTXSTATE, val_), \
	_DW1_VALS(NONE), \
	_DW2_VALS(debondu_, debon_))

/* General purpose input with no special IRQ routing. */
#define PAD_CFG_GPI(pad_, term_, rst_) \
	_PAD_CFG(pad_, term_, \
	_DW0_VALS(rst_, RAW, NO, LEVEL, NO, NO, NO, NO, NO, NO, GPIO, NO, YES),\
	_DW1_VALS(term_), \
	_DW2_VALS(NONE, DISABLE))

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

GpioCfg *new_alderlake_gpio(int gpio_num);
GpioCfg *new_alderlake_gpio_input(int gpio_num);
GpioCfg *new_alderlake_gpio_output(int gpio_num, unsigned int value);
GpioOps *new_alderlake_gpio_input_from_coreboot(uint32_t port);

#endif /* __DRIVERS_GPIO_ALDERLAKE_H__ */
