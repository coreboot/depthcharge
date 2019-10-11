/*
 * Copyright 2018 Google Inc.
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

#ifndef __VBOOT_UTIL_INIT_FUNCS_H__
#define __VBOOT_UTIL_INIT_FUNCS_H__

#include "base/init_funcs.h"
#include "base/list.h"

typedef struct VbootInitFunc
{
	int (*init)(struct VbootInitFunc *init);
	void *data;
	ListNode list_node;
} VbootInitFunc;

extern ListNode vboot_init_funcs;

int run_vboot_init_funcs(void);

/* Shorthand macro to register a VbootInitFunc like a normal INIT_FUNC(). */
#define VBOOT_INIT_FUNC(func) \
	static VbootInitFunc func##_data_ = { .init = &func }; \
	static int func##_setup_(void) { \
		list_insert_after(&func##_data_.list_node, &vboot_init_funcs); \
		return 0; \
	} \
	INIT_FUNC(func##_setup_)

#endif /* __VBOOT_UTIL_INIT_FUNCS_H__ */
