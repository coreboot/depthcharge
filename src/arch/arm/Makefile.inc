##
## This file is part of the depthcharge project.
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
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

depthcharge-y += commonparams.c
depthcharge-y += gcc.c
depthcharge-y += pci_stub.c
depthcharge-y += physmem.c
depthcharge-y += memcpy.S
depthcharge-y += memset.S

trampoline-y += gcc.c
trampoline-y += memcpy.S
trampoline-y += memset.S
trampoline-y += trampoline.c
