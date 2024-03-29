##
## Copyright 2013 Google LLC
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

depthcharge-$(CONFIG_DRIVER_POWER_AS3722) += as3722.c
depthcharge-$(CONFIG_DRIVER_POWER_EXYNOS) += exynos.c
depthcharge-$(CONFIG_DRIVER_POWER_IPQ806X) += ipq806x.c
depthcharge-$(CONFIG_DRIVER_POWER_IPQ40XX) += ipq40xx.c
depthcharge-$(CONFIG_DRIVER_POWER_PCH) += pch.c
depthcharge-$(CONFIG_DRIVER_POWER_RK808) += rk808.c
depthcharge-$(CONFIG_DRIVER_POWER_TPS65913) += tps65913.c
depthcharge-$(CONFIG_DRIVER_POWER_MAX77620) += max77620.c
depthcharge-$(CONFIG_DRIVER_POWER_ARMADA38X) += armada38x.c
depthcharge-y += power.c
depthcharge-y += sysinfo.c
depthcharge-$(CONFIG_ARCH_ARM_V8) += psci.c
depthcharge-$(CONFIG_DRIVER_POWER_FCH) += fch.c
