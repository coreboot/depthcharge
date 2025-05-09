/*
 * Copyright 2021 Google LLC
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

#include "base/sparse.h"

/********************** Sparse Image Handling ****************************/

/*
 * Sparse Image Header.
 * The canonical definition of the sparse format (including the magic values
 * from below) is in AOSP's libsparse:
 * https://android.googlesource.com/platform/system/core/+/refs/heads/master/libsparse/sparse_format.h
 */
struct sparse_image_hdr {
	/* Magic number for sparse image 0xed26ff3a. */
	uint32_t magic;
	/* Major version = 0x1 */
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t file_hdr_size;
	uint16_t chunk_hdr_size;
	/* Size of block in bytes. */
	uint32_t blk_size;
	/* # of blocks in the non-sparse image. */
	uint32_t total_blks;
	/* # of chunks in the sparse image. */
	uint32_t total_chunks;
	uint32_t image_checksum;
};

#define SPARSE_IMAGE_MAGIC 0xed26ff3a
#define CHUNK_TYPE_RAW 0xCAC1
#define CHUNK_TYPE_FILL 0xCAC2
#define CHUNK_TYPE_DONT_CARE 0xCAC3
#define CHUNK_TYPE_CRC32 0xCAC4

/* Chunk header in sparse image */
struct sparse_chunk_hdr {
	uint16_t type;
	uint16_t reserved;
	/* Chunk size is in number of blocks */
	uint32_t size_in_blks;
	/* Size in bytes of chunk header and data */
	uint32_t total_size_bytes;
};

/* Check if given image is sparse */
int is_sparse_image(void *image_addr)
{
	struct sparse_image_hdr *hdr = image_addr;

	/* AOSP sparse format supports major version 0x1 only */
	return ((hdr->magic == SPARSE_IMAGE_MAGIC) &&
		(hdr->major_version == 0x1));
}

struct img_buff {
	void *data;
	uint64_t size;
};

/*
 * Initialize img_buff structure with passed in data and size. Verifies that
 * buff and data is not NULL.
 * Returns 0 on success and -1 on error.
 */
static int img_buff_init(struct img_buff *buff, void *data, uint64_t size)
{
	if ((buff == NULL) || (data == NULL))
		return -1;

	buff->data = data;
	buff->size = size;

	return 0;
}

/*
 * Obtain current pointer to data and advance data pointer by size. If there is
 * not enough data to advance, then it returns NULL.
 */
static void *img_buff_advance(struct img_buff *buff, uint64_t size)
{
	void *data;

	if (buff->size < size)
		return NULL;

	data = buff->data;
	buff->data = (uint8_t *)(buff->data) + size;
	buff->size -= size;

	return data;
}

/* Write sparse image to the disk */
enum gpt_io_ret write_sparse_image(BlockDev *disk, uint64_t part_start_lba,
				   uint64_t part_size_lba, void *image_addr,
				   uint64_t image_size)
{
	struct img_buff buff;
	struct sparse_image_hdr *img_hdr;
	struct sparse_chunk_hdr *chunk_hdr;
	uint64_t bdev_block_size = disk->block_size;

	if (img_buff_init(&buff, image_addr, image_size))
		return GPT_IO_SPARSE_TOO_SMALL;

	img_hdr = img_buff_advance(&buff, sizeof(*img_hdr));

	if (img_hdr == NULL)
		return GPT_IO_SPARSE_TOO_SMALL;

	TRACE_SPARSE("Magic          : %x\n", img_hdr->magic);
	TRACE_SPARSE("Major Version  : %x\n", img_hdr->major_version);
	TRACE_SPARSE("Minor Version  : %x\n", img_hdr->minor_version);
	TRACE_SPARSE("File Hdr Size  : %x\n", img_hdr->file_hdr_size);
	TRACE_SPARSE("Chunk Hdr Size : %x\n", img_hdr->chunk_hdr_size);
	TRACE_SPARSE("Blk Size       : %x\n", img_hdr->blk_size);
	TRACE_SPARSE("Total blks     : %x\n", img_hdr->total_blks);
	TRACE_SPARSE("Total chunks   : %x\n", img_hdr->total_chunks);
	TRACE_SPARSE("Checksum       : %x\n", img_hdr->image_checksum);

	/* Is image header size as expected? */
	if (img_hdr->file_hdr_size != sizeof(*img_hdr))
		return GPT_IO_SPARSE_WRONG_HEADER_SIZE;

	/* Is image block size multiple of bdev block size? */
	if (img_hdr->blk_size !=
	    ALIGN_DOWN(img_hdr->blk_size, bdev_block_size))
		return GPT_IO_SPARSE_BLOCK_SIZE_NOT_ALIGNED;

	/* Is chunk header size as expected? */
	if (img_hdr->chunk_hdr_size != sizeof(*chunk_hdr))
		return GPT_IO_SPARSE_WRONG_HEADER_SIZE;

	int i;
	BlockDevOps *ops = &disk->ops;

	/* Perform the following operation on each chunk */
	for (i = 0; i < img_hdr->total_chunks; i++) {
		/* Get chunk header */
		chunk_hdr = img_buff_advance(&buff, sizeof(*chunk_hdr));

		if (chunk_hdr == NULL)
			return GPT_IO_SPARSE_TOO_SMALL;

		TRACE_SPARSE("Chunk %d\n", i);
		TRACE_SPARSE("Type         : %x\n", chunk_hdr->type);
		TRACE_SPARSE("Size in blks : %x\n", chunk_hdr->size_in_blks);
		TRACE_SPARSE("Total size   : %x\n", chunk_hdr->total_size_bytes);
		TRACE_SPARSE("Part addr    : %llx\n", part_start_lba);

		/* Size in bytes and lba of the area occupied by chunk range */
		uint64_t chunk_size_bytes, chunk_size_lba;

		chunk_size_bytes =
			(uint64_t)chunk_hdr->size_in_blks * img_hdr->blk_size;
		chunk_size_lba = chunk_size_bytes / bdev_block_size;

		/* Should not write past partition size */
		if (part_size_lba < chunk_size_lba) {
			TRACE_SPARSE("part_size_lba:%llx\n", part_size_lba);
			TRACE_SPARSE("chunk_size_lba:%llx\n", chunk_size_lba);
			return GPT_IO_SPARSE_WRONG_CHUNK_SIZE;
		}

		switch (chunk_hdr->type) {
		case CHUNK_TYPE_RAW: {

			uint8_t *data_ptr;

			/*
			 * For Raw chunk type:
			 * chunk_size_bytes + chunk_hdr_size = chunk_total_size
			 */
			if ((chunk_size_bytes + sizeof(*chunk_hdr)) !=
			    chunk_hdr->total_size_bytes) {
				TRACE_SPARSE("chunk_size_bytes:%llx\n",
					     chunk_size_bytes + sizeof(*chunk_hdr));
				TRACE_SPARSE("total_size_bytes:%x\n",
					     chunk_hdr->total_size_bytes);
				return GPT_IO_SPARSE_WRONG_CHUNK_SIZE;
			}

			data_ptr = img_buff_advance(&buff, chunk_size_bytes);
			if (data_ptr == NULL)
				return GPT_IO_SPARSE_TOO_SMALL;

			if (ops->write(ops, part_start_lba, chunk_size_lba,
				       data_ptr) != chunk_size_lba)
				return GPT_IO_TRANSFER_ERROR;

			break;
		}
		case CHUNK_TYPE_FILL: {

			uint32_t *data_fill;

			/*
			 * For fill chunk type:
			 * chunk_hdr_size + 4 bytes = chunk_total_size_bytes
			 */
			if (sizeof(uint32_t) + sizeof(*chunk_hdr) !=
			    chunk_hdr->total_size_bytes) {
				TRACE_SPARSE("chunk_size_bytes:%zx\n",
					     sizeof(uint32_t) + sizeof(*chunk_hdr));
				TRACE_SPARSE("total_size_bytes:%x\n",
					     chunk_hdr->total_size_bytes);
				return GPT_IO_SPARSE_WRONG_CHUNK_SIZE;
			}

			data_fill = img_buff_advance(&buff, sizeof(*data_fill));
			if (!data_fill)
				return GPT_IO_SPARSE_TOO_SMALL;

			/* Perform fill_write operation */
			if (blockdev_fill_write(ops, part_start_lba, chunk_size_lba,
						*data_fill) != chunk_size_lba)
				return GPT_IO_TRANSFER_ERROR;

			break;
		}
		case CHUNK_TYPE_DONT_CARE: {
			/*
			 * For dont care chunk type:
			 * chunk_hdr_size = chunk_total_size_bytes
			 * data in sparse image = 0 bytes
			 */
			if (sizeof(*chunk_hdr) != chunk_hdr->total_size_bytes) {
				TRACE_SPARSE("chunk_size_bytes:%zx\n", sizeof(*chunk_hdr));
				TRACE_SPARSE("total_size_bytes:%x\n",
					     chunk_hdr->total_size_bytes);
				return GPT_IO_SPARSE_WRONG_CHUNK_SIZE;
			}
			break;
		}
		case CHUNK_TYPE_CRC32: {
			/*
			 * For crc32 chunk type:
			 * chunk_hdr_size + 4 bytes = chunk_total_size_bytes
			 */
			if (sizeof(uint32_t) + sizeof(*chunk_hdr) !=
			    chunk_hdr->total_size_bytes) {
				TRACE_SPARSE("chunk_size_bytes:%zx\n",
					     sizeof(uint32_t) + sizeof(*chunk_hdr));
				TRACE_SPARSE("total_size_bytes:%x\n",
					     chunk_hdr->total_size_bytes);
				return GPT_IO_SPARSE_WRONG_CHUNK_SIZE;
			}

			/* Data present in chunk sparse image = 4 bytes */
			if (img_buff_advance(&buff, sizeof(uint32_t)) == NULL)
				return GPT_IO_SPARSE_TOO_SMALL;
			break;
		}
		default: {
			/* Unknown chunk type */
			TRACE_SPARSE("Unknown chunk type %d\n", chunk_hdr->type);
			return GPT_IO_SPARSE_WRONG_CHUNK_TYPE;
		}
		}
		/* Update partition address and size accordingly */
		part_start_lba += chunk_size_lba;
		part_size_lba -= chunk_size_lba;
	}

	return 0;
}
