##
## Copyright (c) 2013 Google Inc.
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

uip-c-ccopts := -fno-strict-aliasing

uip-y += uip_arp.c
uip-y += uip.c
uip-y += uip_debug.c
uip-y += uiplib.c
uip-$(CONFIG_UIP_UDP) += uip_udp_packet.c

net-y += net.c
