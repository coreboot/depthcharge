/*
 * Copyright 2012 Google Inc.
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

#ifndef __IMAGE_SYMBOLS_H__
#define __IMAGE_SYMBOLS_H__

#include <stdint.h>

// C level variable definitions for symbols defined in the linker script.

extern char _start[];
extern char _edata[];
extern char _heap[];
extern char _eheap[];
extern char _estack[];
extern char _stack[];
extern char _exc_estack[];
extern char _exc_stack[];
extern char _end[];
extern char _kernel_start[];
extern char _kernel_end[];
extern char _fit_fdt_start[];
extern char _fit_fdt_end[];
extern char _init_funcs_start[];
extern char _init_funcs_end[];

#endif /* __IMAGE_SYMBOLS_H__ */
