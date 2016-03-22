/*
 * Copyright 2013 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_BOOT_H__
#define __ARCH_ARM_BOOT_H__

#include "boot/fit.h"

int boot_arm_linux(void *fdt, FitImageNode *kernel);

void boot_arm_linux_jump(void *fdt, void *entry) __attribute__((noreturn));

#endif /* __ARCH_ARM_BOOT_H__ */
