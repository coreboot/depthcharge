##
## Copyright 2013 Google Inc.
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

depthcharge-$(CONFIG_DRIVER_BUS_SPI_BITBANG) += bitbang.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_EXYNOS5) += exynos5.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_INTEL_GSPI) += intel_gspi.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_IPQ806X) += ipq806x.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_IPQ40XX) += ipq40xx.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_QCS405) += qcs405.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_ROCKCHIP) += rockchip.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_TEGRA) += tegra.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_BCM_QSPI) += bcm_qspi.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_MTK) += mtk.c
depthcharge-$(CONFIG_DRIVER_BUS_SPI_ARMADA38X_SPI) += armada38x_spi.c
