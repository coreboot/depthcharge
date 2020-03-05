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

depthcharge-y += display.c
depthcharge-y += coreboot_fb.c
depthcharge-$(CONFIG_DRIVER_VIDEO_ARCTICSAND_BACKLIGHT) += arctic_sand_backlight.c
depthcharge-$(CONFIG_DRIVER_VIDEO_EC_PWM_BACKLIGHT) += ec_pwm_backlight.c
depthcharge-$(CONFIG_DRIVER_VIDEO_EXYNOS5) += exynos5.c
depthcharge-$(CONFIG_DRIVER_VIDEO_INTEL_I915) += intel_i915.c
depthcharge-$(CONFIG_DRIVER_VIDEO_LP5562) += led_lp5562.c
depthcharge-$(CONFIG_DRIVER_VIDEO_LP5562) += led_lp5562_programs.c
depthcharge-$(CONFIG_DRIVER_VIDEO_LP5562) += led_lp5562_calibration.c
depthcharge-$(CONFIG_DRIVER_VIDEO_RK3288) += rk3288.c
depthcharge-$(CONFIG_DRIVER_VIDEO_RK3399) += rk3399.c
depthcharge-$(CONFIG_DRIVER_VIDEO_TEGRA132) += tegra132.c
depthcharge-$(CONFIG_DRIVER_VIDEO_TEGRA210) += tegra132.c
depthcharge-$(CONFIG_DRIVER_VIDEO_WW_RING) += ww_ring.c
depthcharge-$(CONFIG_DRIVER_VIDEO_WW_RING) += ww_ring_programs.c
depthcharge-$(CONFIG_DRIVER_VIDEO_MTK) += mtk_ddp.c
depthcharge-$(CONFIG_DRIVER_VIDEO_SC7180) += sc7180.c
