/*
 * Copyright 2012 Google Inc.
 * Copyright (C) 2017 Advanced Micro Devices, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "drivers/power/fch.h"
#include "drivers/power/power.h"

#define ACPI_MMIO_REGION 0xfed80000
#define PMIO_REGS (phys_to_virt(ACPI_MMIO_REGION) + 0x300)

#define PM1_STS         0x00
#define   PWRBTN_STS    (1 << 8)
#define PM1_EN          0x02
#define PM1_CNT         0x04
#define   SLP_EN        (1 << 13)
#define   SLP_TYP       (7 << 10)
#define   SLP_TYP_S0    (0 << 10)
#define   SLP_TYP_S1    (1 << 10)
#define   SLP_TYP_S3    (3 << 10)
#define   SLP_TYP_S4    (4 << 10)
#define   SLP_TYP_S5    (5 << 10)

#define RST_CNT         0xcf9
#define   SYS_RST       (1 << 1)
#define   RST_CPU       (1 << 2)
#define   FULL_RST      (1 << 3)

/*
 * Do a hard reset through the chipset's reset control register. This
 * register is available on all x86 systems.
 *
 * This function never returns.
 */
static int fch_cold_reboot(PowerOps *me)
{
	outb(SYS_RST | RST_CPU | FULL_RST, RST_CNT);
	halt();
}

struct power_off_args {
	/* PCI device for power-mgmt. */
	pcidev_t pci_dev;
	/* Do we need to keep PCI dev for power-mgmt enabled? */
	int en_pci_dev;
	/* Base address for ACPI registers */
	uint16_t pm1base;
	/* Registers to disable GPE wakes. */
	uint16_t gpe_base;
	int num_gpe_regs;
};

static void busmaster_disable_on_bus(int bus, struct power_off_args *args)
{
	for (int slot = 0; slot < 0x20; slot++) {
		for (int func = 0; func < 8; func++) {
			pcidev_t dev = PCI_DEV(bus, slot, func);

			if (args->en_pci_dev && (dev == args->pci_dev))
				continue;

			uint32_t ids = pci_read_config32(dev, REG_VENDOR_ID);

			if (ids == 0xffffffff || ids == 0x00000000 ||
			    ids == 0x0000ffff || ids == 0xffff0000)
				continue;

			// Disable Bus Mastering for this one device.
			uint32_t cmd = pci_read_config32(dev, REG_COMMAND);
			cmd &= ~REG_COMMAND_BM;
			pci_write_config32(dev, REG_COMMAND, cmd);

			// If this is a bridge the follow it.
			uint8_t hdr = pci_read_config8(dev, REG_HEADER_TYPE);
			hdr &= 0x7f;
			if (hdr == HEADER_TYPE_BRIDGE ||
			    hdr == HEADER_TYPE_CARDBUS) {
				uint32_t busses;
				busses = pci_read_config32(
						dev, REG_PRIMARY_BUS);
				busmaster_disable_on_bus((busses >> 8) & 0xff,
							 args);
			}
		}
	}
}

static void busmaster_disable(struct power_off_args *args)
{
	busmaster_disable_on_bus(0, args);
}

/*
 * Power down the machine by using the power management sleep control
 * of the chipset.
 *
 * This function never returns.
 */
static int fch_power_off_common(struct power_off_args *args)
{
	/* Make sure this is an AMD chipset. */
	uint16_t id = pci_read_config16(args->pci_dev, 0x00);
	if (id != 0x1022) {
		printf("Power off is not implemented for this chipset. "
		       "Halting the CPU.\n");
		return -1;
	}

	/*
	 * Mask interrupts or system might stay in a coma (not executing
	 * code anymore, but not powered off either.
	 */
	asm volatile ("cli" ::: "memory");

	/* Turn off all bus master enable bits. */
	busmaster_disable(args);

	/*
	 * Avoid any GPI waking the system from S5 or the system might stay
	 * in a coma.
	 */
	if (args->num_gpe_regs > 1)
		printf("Disabling GPI not currently implemented for multiple"
				" GPE registers at this time\n");
	outl(0x00000000, args->gpe_base + 1);

	/* Clear Power Button Status. */
	outw(PWRBTN_STS, args->pm1base + PM1_STS);

	/* PMBASE + 4, Bit 10-12, Sleeping Type, set to 111 -> S5, soft_off */
	uint32_t reg32 = inl(args->pm1base + PM1_CNT);

	/* Set Sleeping Type to S5 (poweroff). */
	reg32 &= ~(SLP_EN | SLP_TYP);
	reg32 |= SLP_TYP_S5;
	outl(reg32, args->pm1base + PM1_CNT);

	/* Now set the Sleep Enable bit. */
	reg32 |= SLP_EN;
	outl(reg32, args->pm1base + PM1_CNT);

	halt();
}

static int kern_power_off(PowerOps *me)
{
	uint8_t *pmregs = (uint8_t *)PMIO_REGS;
	struct power_off_args args;
	memset(&args, 0, sizeof(args));

	args.pci_dev = PCI_DEV(0, 0x14, 3);
	args.pm1base = (uint16_t)*(uint16_t *)(pmregs + 0x60);
	args.gpe_base = (uint16_t)*(uint16_t *)(pmregs + 0x68);
	args.num_gpe_regs = 1;
	args.en_pci_dev = 1;

	return fch_power_off_common(&args);
}

PowerOps kern_power_ops = {
	.cold_reboot = &fch_cold_reboot,
	.power_off = &kern_power_off
};
