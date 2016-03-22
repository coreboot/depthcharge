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

net-$(CONFIG_DRIVER_NET_IPQ806X) += athrs17_phy.c
net-$(CONFIG_DRIVER_NET_IPQ806X) += ipq806x.c
net-y += asix.c
net-y += mii.c
net-y += net.c
net-y += smsc95xx.c
net-y += usb_eth.c
