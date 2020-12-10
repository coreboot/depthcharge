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

depthcharge-$(CONFIG_DRIVER_SOUND_GPIO_AMP) += gpio_amp.c
depthcharge-$(CONFIG_DRIVER_SOUND_GPIO_BUZZER) += gpio_buzzer.c
depthcharge-$(CONFIG_DRIVER_SOUND_GPIO_EDGE_BUZZER) += gpio_edge_buzzer.c
depthcharge-$(CONFIG_DRIVER_SOUND_GPIO_I2S) += gpio_i2s.c
depthcharge-$(CONFIG_DRIVER_SOUND_GPIO_PDM) += gpio_pdm.c
depthcharge-$(CONFIG_DRIVER_SOUND_HDA) += hda_codec.c
depthcharge-$(CONFIG_DRIVER_SOUND_I2S) += i2s.c
depthcharge-$(CONFIG_DRIVER_SOUND_IPQ806X) += ipq806x.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98090) += max98090.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98095) += max98095.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98373) += max98373.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98390) += max98390.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98927) += max98927.c
depthcharge-$(CONFIG_DRIVER_SOUND_PCAT_BEEP) += pcat_beep.c
depthcharge-$(CONFIG_DRIVER_SOUND_ROUTE) += route.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1011) += rt1011.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1015) += rt1015.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1015P) += rt1015p.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5645) += rt5645.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5677) += rt5677.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5682) += rt5682.c
depthcharge-$(CONFIG_DRIVER_SOUND_SSM4567) += ssm4567.c
depthcharge-$(CONFIG_DRIVER_SOUND_TEGRA_AUDIO_HUB) += tegra_ahub.c
depthcharge-y += dummy.c sound.c
