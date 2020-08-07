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

depthcharge-y += debug.c
depthcharge-y += disk.c
ifneq ($(CONFIG_HEADLESS),y)
depthcharge-$(CONFIG_LEGACY_CLAMSHELL_UI) += display_legacy.c
depthcharge-$(CONFIG_LEGACY_MENU_UI) += display_legacy.c
depthcharge-$(CONFIG_MENU_UI) += display.c
else
depthcharge-$(CONFIG_LEGACY_CLAMSHELL_UI) += headless_stub_legacy.c
depthcharge-$(CONFIG_LEGACY_MENU_UI) += headless_stub_legacy.c
depthcharge-$(CONFIG_MENU_UI) += headless_stub.c
endif
depthcharge-y += keyboard.c
depthcharge-y += legacy.c
depthcharge-y += memory.c
depthcharge-y += misc.c
depthcharge-y += switches.c
depthcharge-y += time.c
depthcharge-y += tpm.c

ifeq ($(CONFIG_EC_SOFTWARE_SYNC),y)
depthcharge-y += ec.c
else
depthcharge-y += ec_stub.c
endif

depthcharge-y += ec_util.c

ifeq ($(CONFIG_AUXFW_SOFTWARE_SYNC),y)
depthcharge-y += auxfw.c
else
depthcharge-y += auxfw_stub.c
endif
