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

#include <libpayload.h>
#include <lzma.h>
#include <vboot_api.h>

#include "vboot/util/flag.h"

uint32_t VbExIsShutdownRequested(void)
{
	int lidsw = flag_fetch(FLAG_LIDSW);
	int pwrsw = flag_fetch(FLAG_PWRSW);
	uint32_t shutdown_request = 0;

	if (lidsw < 0 || pwrsw < 0) {
		// There isn't any way to return an error, so just hang.
		printf("Failed to fetch lid or power switch flag.\n");
		halt();
	}

	if (!lidsw) {
		printf("Lid is closed.\n");
		shutdown_request |= VB_SHUTDOWN_REQUEST_LID_CLOSED;
	}
	if (pwrsw) {
		printf("Power key pressed.\n");
		shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;
	}

	return shutdown_request;
}
