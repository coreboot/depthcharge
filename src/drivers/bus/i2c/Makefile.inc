##
## Copyright 2012-2025 Google LLC
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

depthcharge-y += i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_CROS_EC_TUNNEL) += cros_ec_tunnel.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_CYGNUS) += cygnus.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_DESIGNWARE) += designware.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_DESIGNWARE_PCI) += designware_pci.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_EXYNOS5_USI) += exynos5_usi.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ806X) += ipq806x_gsbi.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ806X) += ipq806x_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ806X) += ipq806x_qup.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ40XX) += ipq40xx_blsp.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ40XX) += ipq40xx_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ40XX) += ipq40xx_qup.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_QCS405) += qcs405_blsp.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_QCS405) += qcs405_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_QCS405) += qcs405_qup.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8173) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8183) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8186) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8188) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8189) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8196) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_ROCKCHIP) += rockchip.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_S3C24X0) += s3c24x0.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_TEGRA) += tegra.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_ARMADA38X) += armada38x_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_QCOM_QUP) += qcom_qupv3_i2c.c
