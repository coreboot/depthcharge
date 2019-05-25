/*
 * Copyright 2015 Google Inc.
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

#include <libpayload.h>
#include <vboot_api.h>
#include <gpt.h>

#include "boot/bcb.h"

uint8_t VbExOverrideGptEntryPriority(const GptEntry *e)
{
#if CONFIG_BCB_SUPPORT
	if (bcb_override_priority(e->name))
		return 15;
#endif
	return 0;
}
