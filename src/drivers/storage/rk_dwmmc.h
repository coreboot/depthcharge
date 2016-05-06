/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RK_DWMMC_H
#define __RK_DWMMC_H

#define RK_CLRSETBITS(clr, set) ((((clr) | (set)) << 16) | set)

DwmciHost *new_rkdwmci_host(uintptr_t ioaddr, uint32_t src_hz,
				  int bus_width, int removable,
				  GpioOps *card_detect);
#endif

