##
## Copyright 2012 Google LLC
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##

depthcharge-$(CONFIG_DRIVER_AHCI) += ahci.c
depthcharge-y += blockdev.c
depthcharge-$(CONFIG_DRIVER_STORAGE_MMC) += mmc.c
depthcharge-$(CONFIG_DRIVER_STORAGE_MMC_DW) += dw_mmc.c
depthcharge-$(CONFIG_DRIVER_STORAGE_IPQ_806X) += ipq806x_mmc.c ipq806x_clocks.c
depthcharge-$(CONFIG_DRIVER_STORAGE_IPQ_40XX) += ipq40xx_mmc.c ipq40xx_clocks.c
depthcharge-$(CONFIG_DRIVER_STORAGE_MMC_TEGRA) += tegra_mmc.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_STORAGE_DWMMC_RK3288) += rk3288_dwmmc.c
depthcharge-$(CONFIG_DRIVER_STORAGE_DWMMC_RK3399) += rk3399_dwmmc.c
depthcharge-$(CONFIG_DRIVER_STORAGE_SDHCI_RK3399) += rk3399_sdhci.c
depthcharge-$(CONFIG_DRIVER_STORAGE_MSHC_S5P) += s5p_mshc.c
depthcharge-$(CONFIG_DRIVER_STORAGE_COMMON) += storage_common.c
depthcharge-y += usb.c
depthcharge-$(CONFIG_DRIVER_SDHCI) += sdhci.c mem_sdhci.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_STORAGE_BAYHUB) += bayhub.c
depthcharge-$(CONFIG_DRIVER_STORAGE_GENESYSLOGIC) += sdhci_gli.c
depthcharge-$(CONFIG_DRIVER_STORAGE_SDHCI_PCI) += pci_sdhci.c
depthcharge-$(CONFIG_DRIVER_STORAGE_SPI_GPT) += spi_gpt.c
depthcharge-$(CONFIG_DRIVER_STORAGE_NVME) += nvme.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_STORAGE_UFS) += ufs.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_STORAGE_UFS_INTEL) += ufs_intel.c
depthcharge-$(CONFIG_DRIVER_STORAGE_UFS_MTK) += mtk_ufs.c
subdirs-y += mtd
depthcharge-$(CONFIG_DRIVER_STORAGE_MMC_MTK) += mtk_mmc.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_STORAGE_MMC_MVMAP2315) += mvmap2315_mmc.c bouncebuf.c
depthcharge-$(CONFIG_DRIVER_STORAGE_SDHCI_MSM) += sdhci_msm.c
depthcharge-$(CONFIG_DRIVER_STORAGE_MMC_RTK) += rtk_mmc.c
