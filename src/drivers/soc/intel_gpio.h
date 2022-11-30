/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _INTEL_GPIO_H_
#define _INTEL_GPIO_H_

#include "drivers/soc/intel_common.h"

struct gpio_pad_cfg {
	uintptr_t pcr_base_addr;
	uint16_t dw_offset;
	uint16_t hostsw_offset;
};

struct gpio_community {
	int port_id;
	/* Inclusive pads within the community. */
	int min;
	int max;
	/* GPIO PAD configuration */
	struct gpio_pad_cfg pad_cfg;
};

#define GPIO_DWx_WIDTH(x)	(4 * (x + 1))

/*
 * The 'attrs' field carries the termination in bits 13:10 to match up with
 * thd DW1 pad configuration register. Additionally, other attributes can
 * be applied such as the ones below. Bit allocation matters.
 */
#define HOSTSW_ACPI             0
#define HOSTSW_GPIO             1
#define HOSTSW_SHIFT            0
#define HOSTSW_MASK             1
#define HOSTSW_OWN_PADS_PER	24

#define PAD_FIELD_VAL(field_, val_) \
	(((val_) & field_ ## _MASK) << field_ ## _SHIFT)

#define PAD_FIELD(field_, setting_) \
	PAD_FIELD_VAL(field_, field_ ## _ ## setting_)

/*
 * DW0
 */
/* PADRSTCFG - when to reset the pad config */
#define PADRSTCFG_SHIFT		30
#define PADRSTCFG_MASK		0x3
#define PADRSTCFG_RSMRST	0
#define PADRSTCFG_DEEP		1
#define PADRSTCFG_PLTRST	2
#define PADRSTCFG_RSVD		3

/* RXPADSTSEL - raw signal or internal state */
#define RXPADSTSEL_SHIFT	29
#define RXPADSTSEL_MASK		0x1
#define RXPADSTSEL_RAW		0
#define RXPADSTSEL_INTERNAL	1
/* RXRAW1 - drive 1 instead instead of pad value */
#define RXRAW1_SHIFT		28
#define RXRAW1_MASK		0x1
#define RXRAW1_NO		0
#define RXRAW1_YES		1
/* RXEVCFG - Interrupt and wake types */
#define RXEVCFG_SHIFT		25
#define RXEVCFG_MASK		0x3
#define RXEVCFG_LEVEL		0
#define RXEVCFG_EDGE		1
#define RXEVCFG_DISABLE		2
#define RXEVCFG_EITHEREDGE	3
/* PREGFRXSEL - use filtering on Rx pad */
#define PREGFRXSEL_SHIFT	24
#define PREGFRXSEL_MASK		0x1
#define PREGFRXSEL_NO		0
#define PREGFRXSEL_YES		1
/* RXINV - invert signal to SMI, SCI, NMI, or IRQ routing. */
#define RXINV_SHIFT		23
#define RXINV_MASK		0x1
#define RXINV_NO		0
#define RXINV_YES		1
/* GPIROUTIOXAPIC - route to io-xapic or not */
#define GPIROUTIOXAPIC_SHIFT	20
#define GPIROUTIOXAPIC_MASK	0x1
#define GPIROUTIOXAPIC_NO	0
#define GPIROUTIOXAPIC_YES	1
/* GPIROUTSCI - route to SCI */
#define GPIROUTSCI_SHIFT	19
#define GPIROUTSCI_MASK		0x1
#define GPIROUTSCI_NO		0
#define GPIROUTSCI_YES		1
/* GPIROUTSMI - route to SMI */
#define GPIROUTSMI_SHIFT	18
#define GPIROUTSMI_MASK		0x1
#define GPIROUTSMI_NO		0
#define GPIROUTSMI_YES		1
/* GPIROUTNMI - route to NMI */
#define GPIROUTNMI_SHIFT	17
#define GPIROUTNMI_MASK		0x1
#define GPIROUTNMI_NO		0
#define GPIROUTNMI_YES		1
/* PMODE - mode of pad */
#define PMODE_SHIFT		10
#define PMODE_MASK		0x7
#define PMODE_GPIO		0
#define PMODE_NF1		1
#define PMODE_NF2		2
#define PMODE_NF3		3
#define PMODE_NF4		4
#define PMODE_NF5		5
#define PMODE_NF6		6
#define PMODE_NF7		7
/* GPIORXDIS - Disable Rx */
#define GPIORXDIS_SHIFT		9
#define GPIORXDIS_MASK		0x1
#define GPIORXDIS_NO		0
#define GPIORXDIS_YES		1
/* GPIOTXDIS - Disable Tx */
#define GPIOTXDIS_SHIFT		8
#define GPIOTXDIS_MASK		0x1
#define GPIOTXDIS_NO		0
#define GPIOTXDIS_YES		1
/* GPIORXSTATE - Internal state after glitch filter */
#define GPIORXSTATE_SHIFT	1
#define GPIORXSTATE_MASK	0x1
/* GPIOTXSTATE - Drive value onto pad */
#define GPIOTXSTATE_SHIFT	0
#define GPIOTXSTATE_MASK	0x1

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

/*
 * DW2: Only applicable to GPD_3
 */
/* DEBONDUR - Debounce duration */
#define DEBONDUR_SHIFT		1
#define DEBONDUR_MASK		0xf
#define DEBONDUR_NONE		0
#define DEBONDUR_DEBOUNCE_3	3
#define DEBONDUR_DEBOUNCE_4	4
#define DEBONDUR_DEBOUNCE_5	5
#define DEBONDUR_DEBOUNCE_6	6
#define DEBONDUR_DEBOUNCE_7	7
#define DEBONDUR_DEBOUNCE_8	8
#define DEBONDUR_DEBOUNCE_9	9
#define DEBONDUR_DEBOUNCE_10	10
#define DEBONDUR_DEBOUNCE_11	11
#define DEBONDUR_DEBOUNCE_12	12
#define DEBONDUR_DEBOUNCE_13	13
#define DEBONDUR_DEBOUNCE_14	14
#define DEBONDUR_DEBOUNCE_15	15
/* DEBONEN - Debounce enable */
#define DEBON_SHIFT		0
#define DEBON_MASK		1
#define DEBON_DISABLE		0
#define DEBON_ENABLE		1

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

const struct gpio_community *gpio_get_community(int pad);
GpioCfg *new_platform_gpio(int gpio_num);
GpioCfg *new_platform_gpio_input(int gpio_num);
GpioCfg *new_platform_gpio_output(int gpio_num, unsigned int value);

#endif /* _INTEL_GPIO_H_ */
