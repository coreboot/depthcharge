/*
 * Copyright 2012 Google LLC
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

#ifndef __ARCH_X86_BOOT_H__
#define __ARCH_X86_BOOT_H__

#include "arch/x86/boot/bootparam.h"
#include "vboot/boot.h"

int boot_x86_linux(struct boot_info *bi);

#endif /* __ARCH_X86_BOOT_H__ */
