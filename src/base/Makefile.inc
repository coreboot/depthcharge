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

depthcharge-y += android_misc.c
depthcharge-y += cleanup_funcs.c
depthcharge-y += dt_set_macs.c
depthcharge-y += dt_set_wifi_calibration.c
depthcharge-y += elog.c
depthcharge-y += fw_config.c
depthcharge-y += gpt.c
depthcharge-y += init_funcs.c
depthcharge-y += late_init_funcs.c
depthcharge-y += ranges.c
depthcharge-y += sparse.c
depthcharge-y += state_machine.c
depthcharge-y += timestamp.c
depthcharge-y += vpd_decode.c
ifeq ($(CONFIG_VPD_QCOM),y)
depthcharge-y += vpd_util_qcom.c
else
depthcharge-y += vpd_util.c
endif
