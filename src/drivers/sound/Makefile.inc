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

depthcharge-$(CONFIG_DRIVER_SOUND_AMD_ACP) += amd_acp.c
depthcharge-$(CONFIG_DRIVER_SOUND_AMD_ACP_V1) += amd_acp_v1.c
depthcharge-$(CONFIG_DRIVER_SOUND_AMD_ACP_V2) += amd_acp_v2.c
depthcharge-$(CONFIG_DRIVER_INTEL_GENERIC_AUDIO) += intel_audio_setup.c
depthcharge-$(CONFIG_DRIVER_SOUND_CS35L53) += cs35l53.c
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
depthcharge-$(CONFIG_DRIVER_SOUND_NAU8318_BEEP) += nau8318.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98363_SNDW) += max98363_sndw.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98373_SNDW) += max98373_sndw.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98390) += max98390.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98396) += max98396.c
depthcharge-$(CONFIG_DRIVER_SOUND_MAX98927) += max98927.c
depthcharge-$(CONFIG_DRIVER_SOUND_PCAT_BEEP) += pcat_beep.c
depthcharge-$(CONFIG_DRIVER_SOUND_ROUTE) += route.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1011) += rt1011.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1015) += rt1015.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1015P) += rt1015p.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1019) += rt1019.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1019_BEEP) += rt1019b.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT1320_SNDW) += rt1320_sndw.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5645) += rt5645.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5677) += rt5677.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5682) += rt5682.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT5682S) += rt5682s.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT712_SNDW) += rt7xx_sndw.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT721_SNDW) += rt7xx_sndw.c
depthcharge-$(CONFIG_DRIVER_SOUND_RT722_SNDW) += rt7xx_sndw.c
depthcharge-$(CONFIG_DRIVER_SOUND_SSM4567) += ssm4567.c
depthcharge-$(CONFIG_DRIVER_SOUND_TAS2563) += tas2563.c
depthcharge-$(CONFIG_DRIVER_SOUND_TEGRA_AUDIO_HUB) += tegra_ahub.c
depthcharge-y += dummy.c sound.c