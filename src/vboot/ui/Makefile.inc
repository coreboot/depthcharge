##
## Copyright 2020 Google Inc.
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

depthcharge-y += archive.c
depthcharge-y += bitmap.c

ifneq ($(CONFIG_HEADLESS),y)
depthcharge-y += display.c
else
depthcharge-y += display_headless.c
endif

depthcharge-y += draw.c
depthcharge-y += input.c
depthcharge-y += layout.c
depthcharge-y += log.c
depthcharge-y += loop.c
depthcharge-y += menu.c
depthcharge-y += navigation.c
depthcharge-y += screens.c
