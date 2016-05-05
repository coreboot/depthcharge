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

depthcharge-y += gpio.c
depthcharge-y += sysinfo.c

depthcharge-$(CONFIG_DRIVER_GPIO_BAYTRAIL) += baytrail.c
depthcharge-$(CONFIG_DRIVER_GPIO_BRASWELL) += braswell.c
depthcharge-$(CONFIG_DRIVER_GPIO_EXYNOS5250) += exynos5250.c
depthcharge-$(CONFIG_DRIVER_GPIO_EXYNOS5420) += exynos5420.c
depthcharge-$(CONFIG_DRIVER_GPIO_IMGTEC_PISTACHIO) += imgtec_pistachio.c
depthcharge-$(CONFIG_DRIVER_GPIO_IPQ_806X) += ipq806x.c
depthcharge-$(CONFIG_DRIVER_GPIO_IPQ40XX) += ipq40xx.c
depthcharge-$(CONFIG_DRIVER_GPIO_LYNXPOINT_LP) += lynxpoint_lp.c
depthcharge-$(CONFIG_DRIVER_GPIO_MT8173) += mtk_gpio.c
depthcharge-$(CONFIG_DRIVER_GPIO_PANTHERPOINT) += pantherpoint.c
depthcharge-$(CONFIG_DRIVER_GPIO_PCH) += pch.c
depthcharge-$(CONFIG_DRIVER_GPIO_ROCKCHIP) += rockchip.c
depthcharge-$(CONFIG_DRIVER_GPIO_RK3288) += rk3288.c
depthcharge-$(CONFIG_DRIVER_GPIO_RK3399) += rk3399.c
depthcharge-$(CONFIG_DRIVER_GPIO_SKYLAKE) += skylake.c
depthcharge-$(CONFIG_DRIVER_GPIO_TEGRA) += tegra.c
