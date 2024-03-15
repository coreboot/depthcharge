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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __DRIVERS_INPUT_INPUT_H__
#define __DRIVERS_INPUT_INPUT_H__

#include <commonlib/list.h>

typedef struct OnDemandInput {
	void (*init)(void);
	int need_init;

	struct list_node list_node;
} OnDemandInput;

extern struct list_node on_demand_input_devices;

void input_init(void);

#endif /* __DRIVERS_INPUT_INPUT_H__ */
