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

#ifndef __BASE_INIT_FUNCS_H__
#define __BASE_INIT_FUNCS_H__

typedef int (*init_func_t)(void);

#define INIT_FUNC(func) \
	init_func_t __init_func_ptr__##func \
		__attribute__((section(".init_funcs"))) = &func;

int run_init_funcs(void);

#endif /* __BASE_INIT_FUNCS_H__ */
