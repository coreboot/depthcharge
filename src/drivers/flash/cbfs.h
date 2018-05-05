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

#ifndef __DRIVERS_FLASH_CBFS_H__
#define __DRIVERS_FLASH_CBFS_H__

struct cbfs_media;

/* Return a cbfs_media structure representing the RO CBFS -- NULL on error. */
struct cbfs_media *cbfs_ro_media(void);

/**
 * cbfs_media_from_fmap() - Set up a CBFS media struct for an area
 *
 * @area_name:	Name of FMAP area to use (e.g. RW_LEGACY)
 * @media:	Filled out with the corresponding CBFS media
 * @return 0 if OK, non-zero on error
 */
int cbfs_media_from_fmap(const char *area_name, struct cbfs_media *media);

#endif
