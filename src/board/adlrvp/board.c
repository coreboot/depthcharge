// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020 Intel Corporation */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"
#include <libpayload.h>
#include <sysinfo.h>

#include <libpayload.h>
#include <sysinfo.h>

/*
 * Each USB Type-C port consists of a TCP (USB3) and a USB2 port from
 * the SoC. This table captures the mapping.
 *
 * SoC USB2 ports are numbered 1..10
 * SoC TCP (USB3) ports are numbered 0..3
 */
#define USBC_PORT_0_USB2_NUM    1
#define USBC_PORT_0_USB3_NUM    0

#define USBC_PORT_1_USB2_NUM    2
#define USBC_PORT_1_USB3_NUM    1

#define USBC_PORT_2_USB2_NUM    3
#define USBC_PORT_2_USB3_NUM    2

#define USBC_PORT_3_USB2_NUM    5
#define USBC_PORT_3_USB3_NUM    3

static const struct tcss_port_map typec_map[] = {
	[0] = { USBC_PORT_0_USB3_NUM, USBC_PORT_0_USB2_NUM },
	[1] = { USBC_PORT_1_USB3_NUM, USBC_PORT_1_USB2_NUM },
	[2] = { USBC_PORT_2_USB3_NUM, USBC_PORT_2_USB2_NUM },
	[3] = { USBC_PORT_3_USB3_NUM, USBC_PORT_3_USB2_NUM },
};

int board_tcss_get_port_mapping(const struct tcss_port_map **map)
{
	if (map) {
		*map = typec_map;
		return ARRAY_SIZE(typec_map);
	}

	return 0;
}

static int cr50_irq_status(void)
{
	/* FIX ME: Confirm H1_PCH_INT_L PIN GPE */
	return alderlake_get_gpe(GPE0_DW2_03); /* GPP_E3 */
}

static void alderlake_setup_tpm(void)
{
	/* SPI TPM */
	const IntelGspiSetupParams gspi0_params = {
		.dev = PCH_DEV_GSPI0,
		.cs_polarity = SPI_POLARITY_LOW,
		.clk_phase = SPI_CLOCK_PHASE_FIRST,
		.clk_polarity = SPI_POLARITY_LOW,
		.ref_clk_mhz = 100,
		.gspi_clk_mhz = 1,
	};
	tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0_params),
		cr50_irq_status)->ops);
}

static int board_setup(void)
{
	uint8_t secondary_bus;

	sysinfo_install_flags(NULL);

	/* Chrome EC (eSPI) */
	if (CONFIG(DRIVER_EC_CROS_LPC)) {
		CrosEcLpcBus *cros_ec_lpc_bus =
				new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
		CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
		register_vboot_ec(&cros_ec->vboot);
	}

	if (CONFIG(DRIVER_TPM_SPI))
		alderlake_setup_tpm();

	/* PCH Power */
	power_set_ops(&alderlake_power_ops);

	/* SATA SSD */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCH_DEV_SATA);
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* CPU NVME SSD */
	secondary_bus = pci_read_config8(PCH_DEV_CPU_RP0, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_1 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_1->ctrlr.list_node, &fixed_block_dev_controllers);

	/* PCH NVME SSD */
	secondary_bus = pci_read_config8(PCH_DEV_PCIE0, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_2 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_2->ctrlr.list_node, &fixed_block_dev_controllers);

	/* PCH NVME SSD */
	secondary_bus = pci_read_config8(PCH_DEV_PCIE4, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_3 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_3->ctrlr.list_node, &fixed_block_dev_controllers);

	/* PCH NVME SSD */
	secondary_bus = pci_read_config8(PCH_DEV_PCIE5, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_4 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_4->ctrlr.list_node, &fixed_block_dev_controllers);

	/* PCH NVME SSD */
	secondary_bus = pci_read_config8(PCH_DEV_PCIE8, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_5 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_5->ctrlr.list_node, &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);
