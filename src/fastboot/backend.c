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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/gpt.h"
#include "fastboot/backend.h"

#define BACKEND_DEBUG

#ifdef BACKEND_DEBUG
#define BE_LOG(args...)		printf(args);
#else
#define BE_LOG(args...)
#endif

/* Weak variables for board-specific data */
size_t fb_bdev_count __attribute__((weak)) = 0;
struct bdev_info fb_bdev_list[] __attribute__((weak)) = {{}} ;
size_t fb_part_count __attribute__((weak)) = 0;
struct part_info fb_part_list[] __attribute__((weak)) = {{}} ;

/* Image partition details */
struct image_part_details {
	struct bdev_info *bdev_entry;
	struct part_info *part_entry;
	GptData *gpt;
	GptEntry *gpt_entry;
	size_t part_addr;
	size_t part_size_lba;
};

/********************** Stub implementations *****************************/
backend_ret_t __attribute__((weak)) board_write_partition(const char *name,
							  void *image_addr,
							  size_t image_size)
{
	return BE_NOT_HANDLED;
}


/********************** Sparse Image Handling ****************************/

/* Sparse Image Header */
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

#define SPARSE_IMAGE_MAGIC	0xed26ff3a
#define CHUNK_TYPE_RAW		0xCAC1
#define CHUNK_TYPE_FILL	0xCAC2
#define CHUNK_TYPE_DONT_CARE	0xCAC3
#define CHUNK_TYPE_CRC32	0xCAC4

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

/* Write sparse image to partition */
static backend_ret_t write_sparse_image(struct image_part_details *img,
					void *image_addr, size_t image_size)
{
	struct sparse_image_hdr *img_hdr = image_addr;
	struct sparse_chunk_hdr *chunk_hdr;
	size_t bdev_block_size = img->bdev_entry->bdev->block_size;

	BE_LOG("Magic          : %x\n", img_hdr->magic);
	BE_LOG("Major Version  : %x\n", img_hdr->major_version);
	BE_LOG("Minor Version  : %x\n", img_hdr->minor_version);
	BE_LOG("File Hdr Size  : %x\n", img_hdr->file_hdr_size);
	BE_LOG("Chunk Hdr Size : %x\n", img_hdr->chunk_hdr_size);
	BE_LOG("Blk Size       : %x\n", img_hdr->blk_size);
	BE_LOG("Total blks     : %x\n", img_hdr->total_blks);
	BE_LOG("Total chunks   : %x\n", img_hdr->total_chunks);
	BE_LOG("Checksum       : %x\n", img_hdr->image_checksum);

	/* Is image header size as expected? */
	if (img_hdr->file_hdr_size != sizeof(*img_hdr))
		return BE_SPARSE_HDR_ERR;

	/* Is image block size multiple of bdev block size? */
	if (img_hdr->blk_size != ALIGN_DOWN(img_hdr->blk_size, bdev_block_size))
		return BE_IMAGE_SIZE_MULTIPLE_ERR;

	/* Is chunk header size as expected? */
	if (img_hdr->chunk_hdr_size != sizeof(*chunk_hdr))
		return BE_CHUNK_HDR_ERR;

	int i;
	size_t part_addr = img->part_addr;
	size_t part_size_lba = img->part_size_lba;
	BlockDevOps *ops = &img->bdev_entry->bdev->ops;
	/* data_ptr points to first byte after image header */
	uint8_t *data_ptr = image_addr;
	data_ptr += sizeof(*img_hdr);

	/* Perform the following operation on each chunk */
	for (i = 0; i < img_hdr->total_chunks; i++) {
		/* Get chunk header */
		chunk_hdr = (struct sparse_chunk_hdr *)data_ptr;

		BE_LOG("Chunk %d\n", i);
		BE_LOG("Type         : %x\n", chunk_hdr->type);
		BE_LOG("Size in blks : %x\n", chunk_hdr->size_in_blks);
		BE_LOG("Total size   : %x\n", chunk_hdr->total_size_bytes);

		/*
		 * Make data_ptr point to chunk data(if any):
		 * Raw chunk data = chunk_size_in_blks * img_blk_size
		 * Fill chunk data = 4 bytes of fill data
		 * CRC32 chunk data = 4 bytes of CRC32
		 */
		data_ptr += sizeof(*chunk_hdr);

		/* Size in bytes and lba of the area occupied by chunk range */
		size_t chunk_size_bytes, chunk_size_lba;
		chunk_size_bytes = chunk_hdr->size_in_blks * img_hdr->blk_size;
		chunk_size_lba = chunk_size_bytes / bdev_block_size;

		switch (chunk_hdr->type) {
		case CHUNK_TYPE_RAW: {

			/*
			 * For Raw chunk type:
			 * chunk_size_bytes + chunk_hdr_size = chunk_total_size
			 */
			if ((chunk_size_bytes + sizeof(*chunk_hdr))!=
			    chunk_hdr->total_size_bytes) {
				BE_LOG("chunk_size_bytes:%zx\n",
				       chunk_size_bytes + sizeof(*chunk_hdr));
				BE_LOG("total_size_bytes:%x\n",
				       chunk_hdr->total_size_bytes);
				return BE_CHUNK_HDR_ERR;
			}

			/* Should not write past partition size */
			if (part_size_lba < chunk_size_lba) {
				BE_LOG("part_size_lba:%zx\n", part_size_lba);
				BE_LOG("chunk_size_lba:%zx\n", chunk_size_lba);
				return BE_IMAGE_OVERFLOW_ERR;
			}

			if (ops->write(ops, part_addr, chunk_size_lba, data_ptr)
			    != chunk_size_lba)
				return BE_WRITE_ERR;

			/*
			 * Data present in chunk sparse image is
			 * chunk_size_bytes
			 */
			data_ptr += chunk_size_bytes;

			break;
		}
		case CHUNK_TYPE_FILL: {
			/*
			 * For fill chunk type:
			 * chunk_hdr_size + 4 bytes = chunk_total_size_bytes
			 */
			if (sizeof(uint32_t) + sizeof(*chunk_hdr) !=
			    chunk_hdr->total_size_bytes) {
				BE_LOG("chunk_size_bytes:%zx\n",
				       sizeof(uint32_t) + sizeof(*chunk_hdr));
				BE_LOG("total_size_bytes:%x\n",
				       chunk_hdr->total_size_bytes);
				return BE_CHUNK_HDR_ERR;
			}


			/* Perform fill_write operation */
			if (ops->fill_write(ops, part_addr, chunk_size_lba,
					    *(uint32_t *)data_ptr)
			    != chunk_size_lba)
				return BE_WRITE_ERR;

			/* Data present in chunk sparse image is 4 bytes */
			data_ptr += sizeof(uint32_t);

			break;
		}
		case CHUNK_TYPE_DONT_CARE: {
			/*
			 * For dont care chunk type:
			 * chunk_hdr_size = chunk_total_size_bytes
			 * data in sparse image = 0 bytes
			 */
			if (sizeof(*chunk_hdr) != chunk_hdr->total_size_bytes) {
				BE_LOG("chunk_size_bytes:%zx\n",
				       sizeof(*chunk_hdr));
				BE_LOG("total_size_bytes:%x\n",
				       chunk_hdr->total_size_bytes);
				return BE_CHUNK_HDR_ERR;
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
				BE_LOG("chunk_size_bytes:%zx\n",
				       sizeof(uint32_t) + sizeof(*chunk_hdr));
				BE_LOG("total_size_bytes:%x\n",
				       chunk_hdr->total_size_bytes);
				return BE_CHUNK_HDR_ERR;
			}

			/* TODO(furquan): Verify CRC32 header? */

			/* Data present in chunk sparse image = 4 bytes */
			data_ptr += sizeof(uint32_t);
		}
		default: {
			/* Unknown chunk type */
			BE_LOG("Unknown chunk type %d\n", chunk_hdr->type);
			return BE_CHUNK_HDR_ERR;
		}
		}
		/* Update partition address and size accordingly */
		part_addr += chunk_size_lba;
		part_size_lba -= chunk_size_lba;
	}

	return BE_SUCCESS;
}

/********************** Raw Image Handling *******************************/

static backend_ret_t write_raw_image(struct image_part_details *img,
				     void *image_addr, size_t image_size)
{
	struct bdev_info *bdev_entry = img->bdev_entry;
	size_t part_addr = img->part_addr;
	size_t part_size_lba = img->part_size_lba;
	size_t image_size_lba;

	BlockDevOps *ops = &bdev_entry->bdev->ops;

	size_t block_size = bdev_entry->bdev->block_size;

	/* Ensure that image size is multiple of block size */
	if (image_size != ALIGN_DOWN(image_size, block_size))
		return BE_IMAGE_SIZE_MULTIPLE_ERR;

	image_size_lba = image_size / block_size;

	/* Ensure image size is less than partition size */
	if (part_size_lba < image_size_lba) {
		BE_LOG("part_size_lba:%zx\n", part_size_lba);
		BE_LOG("image_size_lba:%zx\n", image_size_lba);
		return BE_IMAGE_OVERFLOW_ERR;
	}

	if (ops->write(ops, part_addr, image_size_lba, image_addr) !=
	    image_size_lba)
		return BE_WRITE_ERR;

	return BE_SUCCESS;
}

/********************** Image Partition handling ******************************/

struct part_info *get_part_info(const char *name)
{
	int i;

	for (i = 0; i < fb_part_count; i++) {
		if (!strcmp(name, fb_part_list[i].part_name))
			return &fb_part_list[i];
	}

	return NULL;
}

static struct bdev_info *get_bdev_info(const char *name)
{
	int i;

	for (i = 0; i < fb_bdev_count; i++) {
		if (!strcmp(name, fb_bdev_list[i].name))
			return &fb_bdev_list[i];
	}

	return NULL;
}

static backend_ret_t backend_fill_bdev_info(void)
{
	ListNode *devs;
	int count = get_all_bdevs(BLOCKDEV_FIXED, &devs);

	if (count == 0)
		return BE_BDEV_NOT_FOUND;

	int i;

	for (i = 0; i < fb_bdev_count; i++) {
		BlockDev *bdev;
		BlockDevCtrlr *bdev_ctrlr;

		bdev_ctrlr = fb_bdev_list[i].bdev_ctrlr;

		if (bdev_ctrlr == NULL)
			return BE_BDEV_NOT_FOUND;

		list_for_each(bdev, *devs, list_node) {

			if (bdev_ctrlr->ops.is_bdev_owned(&bdev_ctrlr->ops,
							  bdev)) {
				fb_bdev_list[i].bdev = bdev;
				break;
			}
		}

		if (fb_bdev_list[i].bdev == NULL)
			return BE_BDEV_NOT_FOUND;
	}

	return BE_SUCCESS;
}

static backend_ret_t backend_do_init(void)
{
	static int backend_data_init = 0;

	if (backend_data_init)
		return BE_SUCCESS;

	if ((fb_bdev_count == 0) || (fb_bdev_list == NULL))
		return BE_BDEV_NOT_FOUND;

	if (backend_fill_bdev_info() != BE_SUCCESS)
		return BE_BDEV_NOT_FOUND;

	if((fb_part_count == 0) || (fb_part_list == NULL))
		return BE_PART_NOT_FOUND;

	backend_data_init = 1;
	return BE_SUCCESS;
}

static backend_ret_t fill_img_part_info(struct image_part_details *img,
					const char *name)
{
	struct bdev_info *bdev_entry;
	struct part_info *part_entry;
	GptData *gpt = NULL;
	GptEntry *gpt_entry = NULL;
	size_t part_addr;
	size_t part_size_lba;

	/* Get partition info from board-specific partition table */
	part_entry = get_part_info(name);
	if (part_entry == NULL)
		return BE_PART_NOT_FOUND;

	/* Get bdev info from part_entry */
	bdev_entry = part_entry->bdev_info;
	if (bdev_entry == NULL)
		return BE_BDEV_NOT_FOUND;

	/*
	 * If partition is GPT based, we have to go through a level of
	 * indirection to read the GPT entries and identify partition address
	 * and size. If the partition is not GPT based, board provides address
	 * and size of the partition on block device
	 */
	if (part_entry->gpt_based) {
		/* Allocate GPT structure used by cgptlib */
		gpt = alloc_gpt(bdev_entry->bdev);

		if (gpt == NULL)
			goto fail;

		/* Find nth entry based on GUID & instance provided by board */
		gpt_entry = GptFindNthEntry(gpt, &part_entry->guid,
					    part_entry->instance);

		if (gpt_entry == NULL)
			goto fail;

		/* Get partition addr and size from GPT entry */
		part_addr = gpt_entry->starting_lba;
		part_size_lba = GptGetEntrySizeLba(gpt_entry);
	} else {
		/* Take board provided partition addr and size */
		part_addr = part_entry->base;
		part_size_lba = part_entry->size;
	}

	/* Fill image partition details structure */
	img->bdev_entry = bdev_entry;
	img->part_entry = part_entry;
	img->gpt = gpt;
	img->gpt_entry = gpt_entry;
	img->part_addr = part_addr;
	img->part_size_lba = part_size_lba;

	return BE_SUCCESS;

fail:
	/* In case of failure, ensure gpt is freed */
	if (gpt)
		free_gpt(bdev_entry->bdev, gpt);
	return BE_GPT_ERR;
}

static void clean_img_part_info(struct image_part_details *img)
{
	if (img->gpt)
		free_gpt(img->bdev_entry->bdev, img->gpt);
}

/********************** Backend API functions *******************************/

backend_ret_t backend_write_partition(const char *name, void *image_addr,
				      size_t image_size)
{
	backend_ret_t ret;
	struct image_part_details img;

	ret = backend_do_init();
	if (ret != BE_SUCCESS)
		return ret;

	ret = fill_img_part_info(&img, name);

	if (ret != BE_SUCCESS)
		return ret;

	if (is_sparse_image(image_addr)) {
		BE_LOG("Writing sparse image to %s...\n", name);
		ret = write_sparse_image(&img, image_addr, image_size);
	} else {
		BE_LOG("Writing raw image to %s...\n", name);
		ret = write_raw_image(&img, image_addr, image_size);
	}

	if (img.gpt)
		GptUpdateKernelWithEntry(img.gpt, img.gpt_entry,
					 GPT_UPDATE_ENTRY_RESET);

	clean_img_part_info(&img);

	return ret;
}

backend_ret_t backend_erase_partition(const char *name)
{
	backend_ret_t ret;
	struct image_part_details img;

	ret = backend_do_init();
	if (ret != BE_SUCCESS)
		return ret;

	ret = fill_img_part_info(&img, name);

	if (ret != BE_SUCCESS)
		return ret;

	struct bdev_info *bdev_entry = img.bdev_entry;

	BlockDevOps *ops = &bdev_entry->bdev->ops;
	size_t part_size_lba = img.part_size_lba;
	size_t part_addr = img.part_addr;

	/* Perform fill_write operation on the partition */
	if (ops->fill_write(ops, part_addr, part_size_lba, 0xFF)
	    != part_size_lba)
		ret = BE_WRITE_ERR;
	/* If operation was successful, update GPT entry if required */
	else if (img.gpt)
		GptUpdateKernelWithEntry(img.gpt, img.gpt_entry,
					 GPT_UPDATE_ENTRY_INVALID);

	clean_img_part_info(&img);

	return ret;
}

uint64_t backend_get_part_size_bytes(const char *name)
{
	uint64_t ret = 0;
	struct image_part_details img;

	if (backend_do_init() != BE_SUCCESS)
		return ret;

	if (fill_img_part_info(&img, name) != BE_SUCCESS)
		return ret;

	ret = (uint64_t)img.part_size_lba * img.bdev_entry->bdev->block_size;

	clean_img_part_info(&img);

	return ret;
}

const char *backend_get_part_fs_type(const char *name)
{
	struct part_info *part_entry;

	if (backend_do_init() != BE_SUCCESS)
		return NULL;

	/* Get partition info from board-specific partition table */
	part_entry = get_part_info(name);
	if (part_entry == NULL)
		return NULL;

	return part_entry->part_fs_type;
}

uint64_t backend_get_bdev_size_bytes(const char *name)
{
	if (backend_do_init() != BE_SUCCESS)
		return 0;

	struct bdev_info *bdev_entry;

	/* Get bdev info from board-specific bdev table */
	bdev_entry = get_bdev_info(name);
	if (bdev_entry == NULL)
		return 0;

	BlockDev *bdev = bdev_entry->bdev;
	uint64_t size = (uint64_t)bdev->block_count * bdev->block_size;

	return size;
}

uint64_t backend_get_bdev_size_blocks(const char *name)
{
	if (backend_do_init() != BE_SUCCESS)
		return 0;

	struct bdev_info *bdev_entry;

	/* Get bdev info from board-specific bdev table */
	bdev_entry = get_bdev_info(name);
	if (bdev_entry == NULL)
		return 0;

	BlockDev *bdev = bdev_entry->bdev;
	uint64_t size = (uint64_t)bdev->block_count;

	return size;
}

int fb_fill_part_list(const char *name, size_t base, size_t size)
{
	struct part_info *part_entry = get_part_info(name);

	if (part_entry == NULL)
		return -1;

	part_entry->base = base;
	part_entry->size = size;

	return 0;
}
