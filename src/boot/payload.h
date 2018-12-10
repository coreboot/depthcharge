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

/**
 * Holds information about each supported bootloader
 *
 * This information is read from a file.
 */
struct altfw_info {
	ListNode list_node;
	const char *filename;	/* Filename of bootloader */
	const char *name;	/* User-friendly name of bootloader */
	const char *desc;	/* Description text */
	int seqnum;		/* Sequence number (1=first, 2=second) */
};

/**
 * payload_run() - Load and run a named payload file from the given flash area
 *
 * @media: CBFS media to load from
 * @payload_name: Name of CBFS file to run
 * @return non-zero on error (on success this does not return)
 */
int payload_run(struct cbfs_media *media, const char *payload_name);

/**
 * Read and parse the list of alternative-firmware bootloaders
 *
 * The file format is multiple lines each terminated by \n.
 *
 * Each line has four fields used to fill in the above struct:
 *
 * seqnum;filename;name;desc
 *
 * @media:	Returns the CBFS media to read from (useful if the caller then
 *		wants to call payload_run())
 * @return	list of alternative-firmware bootloaders (which may be empty)
 */
struct ListNode *payload_get_altfw_list(struct cbfs_media *media);

#endif // __BOOT_PAYLOAD_H__
