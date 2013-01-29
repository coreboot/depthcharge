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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "netboot/appcall.h"

static AppcallFunc appcall_func;

void set_appcall_func(AppcallFunc func)
{
	appcall_func = func;
}

AppcallFunc get_appcall_func(void)
{
	return appcall_func;
}

void netboot_appcall(void)
{
	if (appcall_func)
		appcall_func();
	else
		printf("No appcall function selected.\n");
}
