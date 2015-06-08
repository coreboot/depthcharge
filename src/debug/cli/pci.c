/*
 * Copyright (C) 2015 Google Inc.
 *
 * Command for testing PCI.
 */

#include "common.h"

static int do_pcir(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned bus, dev, func, reg;
	u32 device;
	int size;

	if (argc < 5)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get bus, device, function, and register */
	bus = strtoul(argv[1], NULL, 16);
	dev = strtoul(argv[2], NULL, 16);
	func = strtoul(argv[3], NULL, 16);
	reg = strtoul(argv[4], NULL, 16);

	device = PCI_ADDR(bus, dev, func, reg);

	switch (size) {
	case 4:
		printf("0x%08x\n", pci_read_config32(device, reg));
		break;
	case 2:
		printf("0x%04x\n", pci_read_config16(device, reg));
		break;
	case 1:
		printf("0x%02x\n", pci_read_config8(device, reg));
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static int do_pciw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned bus, dev, func, reg;
	int size;
	u32 device, val;

	if (argc < 6)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get bus, device, function, and register */
	bus = strtoul(argv[1], NULL, 16);
	dev = strtoul(argv[2], NULL, 16);
	func = strtoul(argv[3], NULL, 16);
	reg = strtoul(argv[4], NULL, 16);
	val = strtoul(argv[5], NULL, 16);

	device = PCI_ADDR(bus, dev, func, reg);

	switch (size) {
	case 4:
		pci_write_config32(device, reg, val);
		break;
	case 2:
		pci_write_config16(device, reg, (u16)val);
		break;
	case 1:
		pci_write_config8(device, reg, (u8)val);
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static int do_lspci(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned bus, dev, func;

	for (bus = 0; bus <= 0xff; bus++) {
		for (dev = 0; dev <= 0x1f; dev++) {
			for (func = 0; func <= 7; func++) {
				u32 viddid = pci_read_config32(
					PCI_DEV(bus, dev, func), 0);
				if (viddid == 0xffffffff ||
				    viddid == 0x00000000 ||
				    viddid == 0xffff0000 ||
				    viddid == 0x0000ffff)
					continue;
				printf("%02x %02x %1x %04x %04x\n",
				       bus, dev, func, viddid & 0xffff,
				       viddid >> 16);
			}
		}
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	lspci,	1,	1,
	"List PCI devices",
	"\n"
);

U_BOOT_CMD(
	pcir,	5,	1,
	"PCI config input",
	"[.b, .w, .l] bus device function register"
);

U_BOOT_CMD(
	pciw,	6,	1,
	"PCI config output",
	"[.b, .w, .l] bus device function register value"
);
