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

subdirs-y := callbacks crossystem nvdata stages ui util

depthcharge-y += stages.c
depthcharge-y += boot_policy.c
depthcharge-y += firmware_id.c
depthcharge-y += nvdata.c

ifeq ($(CONFIG_MOCK_TPM),y)
depthcharge-y += mocked_secdata_tpm.c
else
depthcharge-y += secdata_tpm.c
endif

# Where "main" lives.
vboot-y += main.c
