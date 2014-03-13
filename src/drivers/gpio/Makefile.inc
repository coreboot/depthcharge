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

depthcharge-$(CONFIG_DRIVER_GPIO_EXYNOS5250) += exynos5250.c
depthcharge-$(CONFIG_DRIVER_GPIO_EXYNOS5420) += exynos5420.c
depthcharge-y += gpio.c
depthcharge-$(CONFIG_DRIVER_GPIO_PCH) += pch.c
depthcharge-$(CONFIG_DRIVER_GPIO_PANTHERPOINT) += pantherpoint.c
depthcharge-$(CONFIG_DRIVER_GPIO_LYNXPOINT_LP) += lynxpoint_lp.c
depthcharge-$(CONFIG_DRIVER_GPIO_BAYTRAIL) += baytrail.c
depthcharge-y += sysinfo.c
depthcharge-$(CONFIG_DRIVER_GPIO_TEGRA) += tegra.c
