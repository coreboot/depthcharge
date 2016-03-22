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
 */

#ifndef __DRIVERS_STORAGE_MTD_STREAM_H__
#define __DRIVERS_STORAGE_MTD_STREAM_H__

#include "drivers/storage/stream.h"
#include "drivers/storage/mtd/mtd.h"

StreamCtrlr *new_mtd_stream(MtdDevCtrlr *info);

#endif  /* __DRIVERS_STORAGE_MTD_STREAM_H__ */
