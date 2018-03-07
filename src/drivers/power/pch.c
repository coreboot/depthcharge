/*
 * Copyright 2012 Google Inc.
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

#include "drivers/power/pch.h"
#include "drivers/power/power.h"

#define PM1_STS         0x00
#define   PWRBTN_STS    (1 << 8)
#define PM1_EN          0x02
#define PM1_CNT         0x04
#define   SLP_EN        (1 << 13)
#define   SLP_TYP       (7 << 10)
#define   SLP_TYP_S0    (0 << 10)
#define   SLP_TYP_S1    (1 << 10)
#define   SLP_TYP_S3    (5 << 10)
#define   SLP_TYP_S4    (6 << 10)
#define   SLP_TYP_S5    (7 << 10)

#define RST_CNT         0xcf9
#define   SYS_RST       (1 << 1)
#define   RST_CPU       (1 << 2)
#define   FULL_RST      (1 << 3)

/*
 * Do a hard reset through the chipset's reset control register. This
 * register is available on all x86 systems (at least those built in
 * the last 10ys)
 *
 * This function never returns.
 */
static int pch_cold_reboot(PowerOps *me)
{
	outb(SYS_RST | RST_CPU | FULL_RST, RST_CNT);
	halt();
}

struct power_off_args {
	/* PCI device for power-mgmt. */
	pcidev_t pci_dev;
	/* Do we need to keep PCI dev for power-mgmt enabled? */
	int en_pci_dev;
	/* Register address to read pmbase from. */
	uint16_t pmbase_reg;
	/* BAR for power-mgmt operations. */
	uint16_t pmbase;
	uint16_t pmbase_mask;
	/* Registers to disable GPE wakes. */
	uint16_t gpe_en_reg;
	int num_gpe_regs;
	int no_config_space;
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
 * of the chipset. This will currently only work on Intel chipsets.
 * However, adapting it to new chipsets is fairly simple. You will
 * have to find the IO address of the power management register block
 * in your southbridge, and look up the appropriate SLP_TYP_S5 value
 * from your southbridge's data sheet.
 *
 * This function never returns.
 */
static int __pch_power_off_common(struct power_off_args *args)
{
	int i;
	uint16_t pmbase;

	/*
	 * Make sure this is an Intel chipset with the LPC device hard coded
	 * at 0:1f.0.
	 */
	uint16_t id = pci_read_config16(args->pci_dev, 0x00);
	if (!args->no_config_space && id != 0x8086) {
		printf("Power off is not implemented for this chipset. "
		       "Halting the CPU.\n");
		return -1;
	}

	/*
	 * Find the base address of the power-management registers, if not
	 * already provided by the caller.
	 */
	pmbase = args->pmbase;
	if (!pmbase) {
		if (args->no_config_space) {
			printf("pmbase needed when no config space present.\n");
			return -1;
		}

		pmbase = pci_read_config16(args->pci_dev, args->pmbase_reg);
		pmbase &= args->pmbase_mask;
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
	for (i = 0; i < args->num_gpe_regs; i++)
		outl(0x00000000, pmbase + args->gpe_en_reg +
		     i * sizeof(uint32_t));

	/* Clear Power Button Status. */
	outw(PWRBTN_STS, pmbase + PM1_STS);

	/* PMBASE + 4, Bit 10-12, Sleeping Type, set to 111 -> S5, soft_off */
	uint32_t reg32 = inl(pmbase + PM1_CNT);

	/* Set Sleeping Type to S5 (poweroff). */
	reg32 &= ~(SLP_EN | SLP_TYP);
	reg32 |= SLP_TYP_S5;
	outl(reg32, pmbase + PM1_CNT);

	/* Now set the Sleep Enable bit. */
	reg32 |= SLP_EN;
	outl(reg32, pmbase + PM1_CNT);

	halt();
}

static int pch_power_off_common(uint16_t bar_mask, uint16_t gpe_en_reg)
{
	struct power_off_args args;
	memset(&args, 0, sizeof(args));

	args.pci_dev = PCI_DEV(0, 0x1f, 0);
	args.pmbase_reg = 0x40;
	args.pmbase_mask = bar_mask;
	args.gpe_en_reg = gpe_en_reg;
	args.num_gpe_regs = 1;

	return __pch_power_off_common(&args);
}

static int pch_power_off(PowerOps *me)
{
	return pch_power_off_common(0xfffe, 0x2c);
}

static int baytrail_power_off(PowerOps *me)
{
	return pch_power_off_common(0xfff8, 0x28);
}

static int braswell_power_off(PowerOps *me)
{
	return pch_power_off_common(0xfff8, 0x28);
}

static int skylake_power_off(PowerOps *me)
{
	/*
	 * Skylake has 4 GPE en registers and the bar lives within the
	 * PMC device at 0:1f.2.
	 */
	struct power_off_args args;
	memset(&args, 0, sizeof(args));

	args.pci_dev = PCI_DEV(0, 0x1f, 2);
	args.pmbase_reg = 0x40;
	args.pmbase_mask = 0xfff0;
	args.gpe_en_reg = 0x90;
	args.num_gpe_regs = 4;

	return __pch_power_off_common(&args);
}

static int cannonlake_power_off(PowerOps *me)
{
	/*
	 * Cannonlake has 4 GPE en registers and the bar lives within the
	 * PMC device at 0:1f.2.
	 */
	struct power_off_args args;
	memset(&args, 0, sizeof(args));

	args.pci_dev = PCI_DEV(0, 0x1f, 2);
	args.pmbase_reg = 0x20;
	args.pmbase_mask = 0xff80;
	args.gpe_en_reg = 0x70;
	args.num_gpe_regs = 4;
	/* Check if the config space is present. */
	args.no_config_space = pci_read_config16(args.pci_dev, 0x00) == 0xffff;

	return __pch_power_off_common(&args);
}

static int apollolake_power_off(PowerOps *me)
{
	struct power_off_args args;
	memset(&args, 0, sizeof(args));

	args.pci_dev = PCI_DEV(0, 0xd, 1);
	args.en_pci_dev = 1;
	/*
	 * This is hard-coded because reading BAR4 from PMC dev does not work
	 * correctly. It simply returns 0.
	 */
	args.pmbase = 0x400;
	args.gpe_en_reg = 0x30;
	args.num_gpe_regs = 4;
	/* Check if the config space is present. */
	args.no_config_space = pci_read_config16(args.pci_dev, 0x00) == 0xffff;

	return __pch_power_off_common(&args);
}

PowerOps pch_power_ops = {
	.cold_reboot = &pch_cold_reboot,
	.power_off = &pch_power_off
};

PowerOps baytrail_power_ops = {
	.cold_reboot = &pch_cold_reboot,
	.power_off = &baytrail_power_off
};

PowerOps braswell_power_ops = {
	.cold_reboot = &pch_cold_reboot,
	.power_off = &braswell_power_off
};

PowerOps skylake_power_ops = {
	.cold_reboot = &pch_cold_reboot,
	.power_off = &skylake_power_off
};

PowerOps apollolake_power_ops = {
	.cold_reboot = &pch_cold_reboot,
	.power_off = &apollolake_power_off
};

PowerOps cannonlake_power_ops = {
	.cold_reboot = &pch_cold_reboot,
	.power_off = &cannonlake_power_off
};
