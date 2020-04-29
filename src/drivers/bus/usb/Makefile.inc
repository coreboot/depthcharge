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

depthcharge-y += usb.c
depthcharge-$(CONFIG_DRIVER_LYNXPOINT_NO_EHCI) += lynxpoint.c
depthcharge-$(CONFIG_DRIVER_USB_TIGERLAKE_TCSS) += tigerlake_tcss.c

# Not worth a Kconfig, let's just rely on --gc-sections
depthcharge-y += exynos.c
