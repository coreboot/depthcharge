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

subdirs-y += baytrail broadwell braswell

depthcharge-$(CONFIG_DRIVER_BUS_I2S_EXYNOS5) += exynos5.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_MT8173) += mt8173.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_TEGRA) += tegra.c
depthcharge-$(CONFIG_DRIVER_BUS_I2S_ROCKCHIP) += rockchip.c
