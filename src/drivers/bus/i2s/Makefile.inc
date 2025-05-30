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

subdirs-y += apollolake baytrail broadwell braswell intel_common

depthcharge-$(CONFIG_DRIVER_BUS_I2S_EXYNOS5) += exynos5.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8173) += mtk_v1.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8183) += mtk_v1.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8186) += mtk_v1.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8192) += mtk_v1.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8188) += mtk_v2.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8189) += mtk_v3.c mt8189.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8195) += mtk_v2.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8196) += mtk_v3.c mt8196.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_TEGRA) += tegra.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_ROCKCHIP) += rockchip.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_QCOM) += qcom_lpass.c
