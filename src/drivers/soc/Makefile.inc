##
## Copyright 2016 Google Inc.
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

depthcharge-$(CONFIG_DRIVER_SOC_SKYLAKE) += skylake.c
depthcharge-$(CONFIG_DRIVER_SOC_SKYLAKE) += intel_common.c
depthcharge-$(CONFIG_DRIVER_SOC_APOLLOLAKE) += apollolake.c
depthcharge-$(CONFIG_DRIVER_SOC_APOLLOLAKE) += intel_common.c
depthcharge-$(CONFIG_DRIVER_SOC_CANNONLAKE) += cannonlake.c
depthcharge-$(CONFIG_DRIVER_SOC_CANNONLAKE) += intel_common.c
depthcharge-$(CONFIG_DRIVER_SOC_STONEYRIDGE) += stoneyridge.c
depthcharge-$(CONFIG_DRIVER_SOC_GLK) += apollolake.c
depthcharge-$(CONFIG_DRIVER_SOC_GLK) += intel_common.c
depthcharge-$(CONFIG_DRIVER_SOC_ICELAKE) += icelake.c
depthcharge-$(CONFIG_DRIVER_SOC_ICELAKE) += intel_common.c
depthcharge-$(CONFIG_DRIVER_SOC_QCOM_QUP_SE) += qcom_qup_se.c
depthcharge-$(CONFIG_DRIVER_SOC_JASPERLAKE) += intel_common.c
