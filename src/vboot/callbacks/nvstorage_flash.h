/*
 * Copyright 2014 Google Inc.
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

#ifndef __VBOOT_CALLBACKS_NVSTORAGE_FLASH__
#define __VBOOT_CALLBACKS_NVSTORAGE_FLASH__

/*
 * Note that this functions's return value could change within a single boot
 * sequence, never cache it, call this function each time explicitly.
 */
int nvstorage_flash_get_offet(void);
int nvstorage_flash_get_blob_size(void);

#endif
