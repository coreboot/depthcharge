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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "config.h"
#include "drivers/storage/blockdev.h"

/* Read bcb partition and handle command. */
void bcb_handle_command(void);
/* Returns 1 if override of priority is required. */
uint8_t bcb_override_priority(const uint16_t *name);
/* Sets command and recovery values in BCB partition. */
void bcb_request_recovery(const char *command, const char *recovery);

/* Functions to be implemented by board */
BlockDevCtrlr *bcb_board_bdev_ctrlr(void);
