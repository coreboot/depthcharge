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

#include <libpayload.h>
#include <stdint.h>

int raise(int sig);
int raise(int sig)
{
	printf("libgcc raises signal ");
	if (sig == 8)
		printf("SIGFPE."); // Probably divide by zero.
	else
		printf("%d.", sig);
	halt();
}

void __aeabi_unwind_cpp_pr0(void);
void __aeabi_unwind_cpp_pr0(void)
{
}
