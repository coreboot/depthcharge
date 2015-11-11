##
## Copyright 2012 Google Inc.
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
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

depthcharge-y += i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_CYGNUS) += cygnus.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_DESIGNWARE) += designware.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_DESIGNWARE_PCI) += designware_pci.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_EXYNOS5_USI) += exynos5_usi.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IMGTEC) += imgtec_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ806X) += ipq806x_gsbi.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ806X) += ipq806x_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_IPQ806X) += ipq806x_qup.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_MT8173) += mtk_i2c.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_ROCKCHIP) += rockchip.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_S3C24X0) += s3c24x0.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_TEGRA) += tegra.c
depthcharge-$(CONFIG_DRIVER_BUS_I2C_ARMADA38X) += armada38x_i2c.c
