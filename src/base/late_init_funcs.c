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

#include <assert.h>

#include "late_init_funcs.h"
#include "list.h"

ListNode late_init_funcs;

int run_late_init_funcs(void)
{
	LateInitFunc *init_func;
	list_for_each(init_func, late_init_funcs, list_node) {
		assert(init_func->init);
		if (init_func->init(init_func))
			return 1;
	}
	return 0;
}
