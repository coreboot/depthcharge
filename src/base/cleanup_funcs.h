/*
 * Copyright 2013 Google LLC
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

#ifndef __BASE_CLEANUP_FUNCS_H__
#define __BASE_CLEANUP_FUNCS_H__

#include <commonlib/list.h>

typedef enum CleanupType
{
	CleanupOnReboot = 0x1,
	CleanupOnPowerOff = 0x2,
	CleanupOnHandoff = 0x4,
	CleanupOnLegacy = 0x8,
} CleanupType;

typedef struct CleanupFunc
{
	// A non-zero return value indicates an error.
	int (*cleanup)(struct CleanupFunc *cleanup, CleanupType type);
	// Under what circumstance(s) to call this function.
	CleanupType types;
	void *data;

	struct list_node list_node;
} CleanupFunc;

extern struct list_node cleanup_funcs;

// Call all cleanup functions and print any errors.
void run_cleanup_funcs(CleanupType type);

#endif /* __BASE_CLEANUP_FUNCS_H__ */
