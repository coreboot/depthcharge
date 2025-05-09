/*
 * Copyright 2015 Google LLC
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

#include <vb2_gpt.h>

#include "drivers/storage/stream.h"
#include "drivers/storage/blockdev.h"

/*
 * Function to allocate and initialize GptData structure using block device
 * pointer. This structure is required for all vboot operations on GPT.
 */
GptData *alloc_gpt(BlockDev *bdev);

/* Free the allocated GPT pointer. */
void free_gpt(BlockDev *bdev, GptData *gpt);

/* Returns partition name as an ASCII string. Caller should free the data. */
char *gpt_get_entry_name(GptEntry *e);

/* Returns true to stop iteration, false to continue */
typedef bool (*gpt_foreach_callback_t)(void *ctx, int index, GptEntry *e,
				       char *partition_name);

/* Iterate over all partitions until cb returns true */
bool gpt_foreach_partition(GptData *gpt, gpt_foreach_callback_t cb, void *ctx);

/* Find entry with specified name */
GptEntry *gpt_find_partition(GptData *gpt, const char *partition_name);

/* Get number of entries in GPT */
int gpt_get_number_of_partitions(GptData *gpt);

/* Get GPT entry at specified index */
GptEntry *gpt_get_partition(GptData *gpt, unsigned int index);

/* Return codes of IO functions */
enum gpt_io_ret {
	GPT_IO_SUCCESS = 0,
	GPT_IO_SIZE_NOT_ALIGNED,
	GPT_IO_NO_PARTITION,
	GPT_IO_OUT_OF_RANGE,
	GPT_IO_TRANSFER_ERROR,
	GPT_IO_SPARSE_TOO_SMALL,
	GPT_IO_SPARSE_WRONG_HEADER_SIZE,
	GPT_IO_SPARSE_BLOCK_SIZE_NOT_ALIGNED,
	GPT_IO_SPARSE_WRONG_CHUNK_SIZE,
	GPT_IO_SPARSE_WRONG_CHUNK_TYPE,
};

/* Read content of the partition starting from offset specified in disk blocks */
enum gpt_io_ret gpt_read_partition(BlockDev *disk, GptData *gpt, const char *partition_name,
				   const uint64_t blocks_offset, void *data, size_t data_len);

/*
 * Write content of data buffer to the partition starting from offset specified
 * in disk blocks
 */
enum gpt_io_ret gpt_write_partition(BlockDev *disk, GptData *gpt, const char *partition_name,
				    const uint64_t blocks_offset, void *data, size_t data_len);

/* Erase content of the partition */
enum gpt_io_ret gpt_erase_partition(BlockDev *disk, GptData *gpt,
		    const char *partition_name);

#endif /* __BASE_GPT_H__ */
