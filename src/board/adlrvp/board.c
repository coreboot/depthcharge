// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020 Intel Corporation */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/ufs.h"
#include "drivers/storage/ufs_intel.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/spi.h"
#include "drivers/tpm/tpm.h"
#include <libpayload.h>
#include <sysinfo.h>

#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98373.h"

#include "drivers/storage/sdhci.h"

#include <libpayload.h>
#include <sysinfo.h>

#define EMMC_CLOCK_MIN		400000
#define EMMC_CLOCK_MAX		200000000

#define AUD_VOLUME	4000
#define AUD_BITDEPTH	16
#define AUD_SAMPLE_RATE	48000
#define AUD_NUM_CHANNELS 2
#define AUD_I2C0	PCI_DEV(0, 0x15, 0)
#define AUD_I2C_ADDR	0x32
#define I2C_FS_HZ	400000

static int gsc_irq_status(void)
{
	/* FIX ME: Confirm H1_PCH_INT_L PIN GPE */
	return alderlake_get_gpe(GPE0_DW2_03); /* GPP_E3 */
}

static void alderlake_setup_tpm(void)
{
	/* SPI TPM */
	const IntelGspiSetupParams gspi1_params = {
		.dev = PCH_DEV_GSPI1,
		.cs_polarity = SPI_POLARITY_LOW,
		.clk_phase = SPI_CLOCK_PHASE_FIRST,
		.clk_polarity = SPI_POLARITY_LOW,
		.ref_clk_mhz = 100,
		.gspi_clk_mhz = 1,
	};
	tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi1_params),
		gsc_irq_status)->ops);
}

#if CONFIG(DRIVER_SOUND_MAX98373)
static uintptr_t get_ssp_base_address(void)
{
	switch (CONFIG_ADLRVP_MAX98373_I2S_PORT) {
	case 1:
		return SSP_I2S1_START_ADDRESS;
	case 2:
	default:
		return SSP_I2S2_START_ADDRESS;
	}
}

static void adlrvp_setup_max98373_i2s(void)
{
	I2s *i2s = new_i2s_structure(&max98373_settings, AUD_BITDEPTH, 0,
			get_ssp_base_address());
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
			AUD_NUM_CHANNELS, AUD_VOLUME);
	/* Connect Audio codec to the I2S source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	/* Speaker amp is Maxim 98373 codec on I2C0 */
	DesignwareI2c *i2c0 = new_pci_designware_i2c(AUD_I2C0, I2C_FS_HZ,
			ALDERLAKE_DW_I2C_MHZ);
	Max98373Codec *speaker_amp = new_max98373_codec(&i2c0->ops, AUD_I2C_ADDR);
	list_insert_after(&speaker_amp->component.list_node,
			&sound_route->components);
	sound_set_ops(&sound_route->ops);
}
#endif

static int board_setup(void)
{
	uint8_t secondary_bus;

	sysinfo_install_flags(NULL);

	/* Chrome EC (eSPI) */
	if (CONFIG(DRIVER_EC_CROS_LPC)) {
		CrosEcLpcBus *cros_ec_lpc_bus;
		if (CONFIG(CROS_EC_ENABLE_MEC)) {
			cros_ec_lpc_bus =
					new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_MEC);
		}
		else {
			cros_ec_lpc_bus =
					new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
		}

		CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
		register_vboot_ec(&cros_ec->vboot);
	}

	if (CONFIG(DRIVER_TPM_SPI))
		alderlake_setup_tpm();

	/* PCH Power */
	power_set_ops(&alderlake_power_ops);
#if CONFIG(BOARD_ADLRVP_N)
	if (CONFIG(DRIVER_STORAGE_SDHCI_PCI)) {
		/* eMMC */
		SdhciHost *emmc =
		new_pci_sdhci_host(PCH_DEV_EMMC,
					SDHCI_PLATFORM_SUPPORTS_HS400ES,
					EMMC_CLOCK_MIN, EMMC_CLOCK_MAX);
		list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
					&fixed_block_dev_controllers);

		/* SD Card */
		const pcidev_t dev = PCH_DEV_PCIE6;
		pcidev_t remapped;

		remapped = remap_pci_dev(dev);

		if (remapped == (pcidev_t)-1) {
			printf("%s: Failed to remap %2X:%X\n",
			       __func__, PCI_SLOT(dev), PCI_FUNC(dev));
		}
		else {
			SdhciHost *sd = probe_pci_sdhci_host(remapped, SDHCI_PLATFORM_REMOVABLE);
			if (sd) {
				sd->name = "sd";
				list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
						  &removable_block_dev_controllers);
			}
		}
	}

	/* UFS */
	if (CONFIG(DRIVER_STORAGE_UFS_INTEL)) {
		IntelUfsCtlr *intel_ufs = new_intel_ufs_ctlr(PCH_DEV_UFS1);
		list_insert_after(&intel_ufs->ufs.bctlr.list_node, &fixed_block_dev_controllers);
	}

	/* PCH NVME SSD */
	secondary_bus = pci_read_config8(PCH_DEV_PCIE8, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_1 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_1->ctrlr.list_node, &fixed_block_dev_controllers);

#else
	/* SATA SSD */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCH_DEV_SATA);
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* CPU NVME SSD */
	secondary_bus = pci_read_config8(SA_DEV_CPU_RP0, REG_SECONDARY_BUS);
	NvmeCtrlr *nvme_1 = new_nvme_ctrlr(PCI_DEV(secondary_bus, 0, 0));
	list_insert_after(&nvme_1->ctrlr.list_node, &fixed_block_dev_controllers);

	/* UFS */
	if (CONFIG(DRIVER_STORAGE_UFS_INTEL)) {
		IntelUfsCtlr *intel_ufs = new_intel_ufs_ctlr(PCH_DEV_UFS1);
		list_insert_after(&intel_ufs->ufs.bctlr.list_node,
			&fixed_block_dev_controllers);
	}

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

	/* SD PCIe AIC on PCH RP8 x4 slot */
	if (CONFIG(DRIVER_STORAGE_SDHCI_PCI)) {
		SdhciHost *sd = probe_pci_sdhci_host(PCH_DEV_PCIE7, SDHCI_PLATFORM_REMOVABLE);
		if (sd) {
			sd->name = "sd";
			list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
		}
	}
#endif

#if CONFIG(DRIVER_SOUND_MAX98373)
	adlrvp_setup_max98373_i2s();
#endif

	return 0;
}

INIT_FUNC(board_setup);
