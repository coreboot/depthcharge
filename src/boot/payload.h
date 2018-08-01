/*
 * Copyright 2018 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#ifndef __BOOT_PAYLOAD_H__
#define __BOOT_PAYLOAD_H__

/*
 * payload_run() - Load and run a named payload file from the given flash area
 *
 * @area_name: Flashmap area name containing a CBFS
 * @payload_name: Name of CBFS file to run
 * @return non-zero on error (on success this does not return)
 */
int payload_run(const char *area_name, const char *payload_name);

#endif // __BOOT_PAYLOAD_H__
