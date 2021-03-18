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

#include <assert.h>
#include <libpayload.h>

#include "base/cleanup_funcs.h"
#include "base/timestamp.h"
#include "debug/dev.h"

ListNode cleanup_funcs;

void run_cleanup_funcs(CleanupType type)
{
	CleanupFunc *func;
	list_for_each(func, cleanup_funcs, list_node) {
		assert(func->cleanup);
		if ((func->types & type)) {
			int res = func->cleanup(func, type);
			if (res)
				printf("cleanup(%d) failed! %p = %d\n",
				       type, func->cleanup, res);
		}
	}

	dc_dev_gdb_exit(type);

	printf("Exiting depthcharge with code %d at timestamp: %llu\n",
	       type, get_us_since_boot());
}
