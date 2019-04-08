/*
 * Copyright (C) 2016 Google Inc.
 * Copyright (C) 2016 Intel Corporation.
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

#ifndef __DRIVERS_GPIO_APOLLOLAKE_H__
#define __DRIVERS_GPIO_APOLLOLAKE_H__

#include <stdint.h>
#include "base/cleanup_funcs.h"
#include <config.h>

#if CONFIG(DRIVER_SOC_GLK)
#include "drivers/gpio/glk_defs.h"
#else
#include "drivers/gpio/apl_defs.h"
#endif

/* Register defines. */
#define MISCCFG_OFFSET          0x10
#define  GPIO_DRIVER_IRQ_ROUTE_MASK     8
#define  GPIO_DRIVER_IRQ_ROUTE_IRQ14    0
#define  GPIO_DRIVER_IRQ_ROUTE_IRQ15    8
#define  GPE_DW_SHIFT           8
#define  GPE_DW_MASK            0xfff00
#define PAD_OWN_REG_OFFSET      0x20
#define  PAD_OWN_PADS_PER       8
#define  PAD_OWN_WIDTH_PER      4
#define  PAD_OWN_MASK           0x03
#define  PAD_OWN_HOST           0x00
#define  PAD_OWN_ME             0x01
#define  PAD_OWN_ISH            0x02
#define HOSTSW_OWN_REG_OFFSET   0x80
#define  HOSTSW_OWN_PADS_PER    24
#define  HOSTSW_OWN_ACPI        0
#define  HOSTSW_OWN_GPIO        1

        /* PADRSTCFG - when to reset the pad config */
#define  PADRSTCFG_SHIFT        30
#define  PADRSTCFG_MASK         0x3
#define  PADRSTCFG_DSW_PWROK    0
#define  PADRSTCFG_DEEP         1
#define  PADRSTCFG_PLTRST       2
#define  PADRSTCFG_RSMRST       3
        /* RXPADSTSEL - raw signal or internal state */
#define  RXPADSTSEL_SHIFT       29
#define  RXPADSTSEL_MASK        0x1
#define  RXPADSTSEL_RAW         0
#define  RXPADSTSEL_INTERNAL    1
        /* RXRAW1 - drive 1 instead instead of pad value */
#define  RXRAW1_SHIFT           28
#define  RXRAW1_MASK            0x1
#define  RXRAW1_NO              0
#define  RXRAW1_YES             1
        /* RXEVCFG - Interrupt and wake types */
#define  RXEVCFG_SHIFT          25
#define  RXEVCFG_MASK           0x3
#define  RXEVCFG_LEVEL          0
#define  RXEVCFG_EDGE           1
#define  RXEVCFG_DRIVE0         2
        /* PREGFRXSEL - use filtering on Rx pad */
#define  PREGFRXSEL_SHIFT       24
#define  PREGFRXSEL_MASK        0x1
#define  PREGFRXSEL_NO          0
#define  PREGFRXSEL_YES         1
        /* RXINV - invert signal to SMI, SCI, NMI, or IRQ routing. */
#define  RXINV_SHIFT            23
#define  RXINV_MASK             0x1
#define  RXINV_NO               0
#define  RXINV_YES              1
        /* GPIROUTIOXAPIC - route to io-xapic or not */
#define  GPIROUTIOXAPIC_SHIFT   20
#define  GPIROUTIOXAPIC_MASK    0x1
#define  GPIROUTIOXAPIC_NO      0
#define  GPIROUTIOXAPIC_YES     1
        /* GPIROUTSCI - route to SCI */
#define  GPIROUTSCI_SHIFT       19
#define  GPIROUTSCI_MASK        0x1
#define  GPIROUTSCI_NO          0
#define  GPIROUTSCI_YES         1
       /* GPIROUTSMI - route to SMI */
#define  GPIROUTSMI_SHIFT       18
#define  GPIROUTSMI_MASK        0x1
#define  GPIROUTSMI_NO          0
#define  GPIROUTSMI_YES         1
        /* GPIROUTNMI - route to NMI */
#define  GPIROUTNMI_SHIFT       17
#define  GPIROUTNMI_MASK        0x1
#define  GPIROUTNMI_NO          0
#define  GPIROUTNMI_YES         1
        /* PMODE - mode of pad */
#define  PMODE_SHIFT            10
#define  PMODE_MASK             0x3
#define  PMODE_GPIO             0
#define  PMODE_NF1              1
#define  PMODE_NF2              2
#define  PMODE_NF3              3
        /* GPIORXDIS - Disable Rx */
#define  GPIORXDIS_SHIFT        9
#define  GPIORXDIS_MASK         0x1
#define  GPIORXDIS_NO           0
#define  GPIORXDIS_YES          1
        /* GPIOTXDIS - Disable Tx */
#define  GPIOTXDIS_SHIFT        8
#define  GPIOTXDIS_MASK         0x1
#define  GPIOTXDIS_NO           0
#define  GPIOTXDIS_YES          1
        /* GPIORXSTATE - Internal state after glitch filter */
#define  GPIORXSTATE_SHIFT      1
#define  GPIORXSTATE_MASK       0x1
        /* GPIOTXSTATE - Drive value onto pad */
#define  GPIOTXSTATE_SHIFT      0
#define  GPIOTXSTATE_MASK       0x1
 /* TERM - termination control */
#define  PAD_TERM_SHIFT         10
#define  PAD_TERM_MASK          0xf
#define  PAD_TERM_NONE          0
#define  PAD_TERM_5K_PD         2
#define  PAD_TERM_1K_PU         9
#define  PAD_TERM_2K_PU         11
#define  PAD_TERM_5K_PU         10
#define  PAD_TERM_20K_PU        12
#define  PAD_TERM_667_PU        13
#define  PAD_TERM_NATIVE        15

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

#define _PAD_CFG_ATTRS(pad_, term_, dw0_, attrs_)       		\
       {                                                                \
                .pad = pad_,                                            \
                .attrs = PAD_FIELD(PAD_TERM,  term_) | attrs_,          \
                .dw0 = dw0_,                                            \
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
#define HOSTSW_SHIFT            0
#define HOSTSW_MASK             1
#define HOSTSW_ACPI             HOSTSW_OWN_ACPI
#define HOSTSW_GPIO             HOSTSW_OWN_GPIO

typedef struct pad_config {
	uint16_t pad;
	uint16_t attrs;
	uint32_t dw0;
} pad_config;

/*
 * Depthcharge GPIO interface.
 */

typedef struct GpioCfg {
	GpioOps ops;

	int gpio_num;		/* GPIO number */
	uint32_t *dw_regs;	/* Pointer to DW regs */
	uint32_t current_dw0;	/* Current DW0 register value */

	/* Used to save and restore GPIO configuration */
	uint32_t save_dw0;
	uint32_t save_dw1;
	CleanupFunc cleanup;

	int (*configure)(struct GpioCfg *, const pad_config *);
} GpioCfg;

GpioCfg *new_apollolake_gpio(int gpio_num);
GpioCfg *new_apollolake_gpio_input(int gpio_num);
GpioCfg *new_apollolake_gpio_output(int gpio_num, unsigned value);

#endif /* __DRIVERS_GPIO_APOLLOLAKE_H__ */
