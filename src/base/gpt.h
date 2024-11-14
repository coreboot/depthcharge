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

#ifndef __BASE_GPT_H__
#define __BASE_GPT_H__

/* Headers from vboot for GPT manipulation. */
#include <gpt.h>
#include <gpt_misc.h>

#include "drivers/storage/stream.h"
#include "drivers/storage/blockdev.h"

/*
 * Function to allocate and initialize GptData structure using block device
 * pointer. This structure is required for all vboot operations on GPT.
 */
GptData *alloc_gpt(BlockDev *bdev);

/* Free the allocated GPT pointer. */
void free_gpt(BlockDev *bdev, GptData *gpt);

#endif /* __BASE_GPT_H__ */
