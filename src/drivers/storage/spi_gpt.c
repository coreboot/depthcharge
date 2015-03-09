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

#include <assert.h>
#include <libpayload.h>
#include <stdio.h>
#include <string.h>

/* Headers from vboot for GPT manipulation */
#include <gpt.h>
#include <gpt_misc.h>

#include "base/container_of.h"
#include "base/device_tree.h"
#include "drivers/flash/flash.h"
#include "drivers/storage/spi_gpt.h"
#include "vboot/stages.h"

/* Block size is arbitrarily chosen; any size would work once buffering
 * is implemented. Partitions have to be aligned to the size of an
 * erase block, but this is enforced by cgpt rather than encoded in
 * the on-device format. */
#define BLOCK_SHIFT 9
#define BLOCK_SIZE (1 << BLOCK_SHIFT);

static lba_t read_spi_gpt(struct BlockDevOps *me, lba_t start, lba_t count,
			  void *buffer)
{
	BlockDev *blockdev = container_of(me, BlockDev, ops);
	SpiGptDev *dev = container_of(blockdev, SpiGptDev, block_dev);

	uint64_t start_byte = start << BLOCK_SHIFT;
	uint64_t count_byte = count << BLOCK_SHIFT;

	if (start_byte + count_byte > dev->area.size) {
		printf(
		       "read out of bounds: start=0x%llx count=0x%llx spi size=0x%x",
		       start, count, dev->area.size);
		return -1;
	}

	void *flash_buffer = flash_read(start_byte + dev->area.offset,
					count_byte);
	if (!flash_buffer)
		return 0;
	memcpy(buffer, flash_buffer, count_byte);
	return count;
}

static lba_t write_spi_gpt(struct BlockDevOps *me, lba_t start, lba_t count,
			   const void *buffer)
{
	BlockDev *blockdev = container_of(me, BlockDev, ops);
	SpiGptDev *dev = container_of(blockdev, SpiGptDev, block_dev);

	uint64_t start_byte = start << BLOCK_SHIFT;
	uint64_t count_byte = count << BLOCK_SHIFT;

	if (start_byte + count_byte > dev->area.size) {
		printf(
		       "write out of bounds: start=0x%llx count=0x%llx spi size=0x%x",
		       start, count, dev->area.size);
		return -1;
	}

	return flash_rewrite(start_byte + dev->area.offset, count_byte, buffer)
			>> BLOCK_SHIFT;
}

static StreamOps *new_stream_spi_gpt(struct BlockDevOps *me, lba_t start,
				      lba_t count)
{
	BlockDev *blockdev = container_of(me, BlockDev, ops);
	SpiGptDev *dev = container_of(blockdev, SpiGptDev, block_dev);

	uint64_t start_byte = start << BLOCK_SHIFT;
	uint64_t count_byte = count << BLOCK_SHIFT;

	return dev->ctrlr->stream_ctrlr->open(dev->ctrlr->stream_ctrlr,
					      start_byte, count_byte);
}

static GptData *read_gpt(SpiGptDev *dev)
{
	GptData *data = xzalloc(sizeof(*data));
	data->sector_bytes = BLOCK_SIZE;
	data->streaming_drive_sectors = dev->block_dev.stream_block_count;
	data->gpt_drive_sectors = dev->block_dev.block_count;
	data->flags = GPT_FLAG_EXTERNAL;

	int ret = AllocAndReadGptData(&dev->block_dev, data);
	if (ret)
		return NULL;

	if (GPT_SUCCESS != GptInit(data)) {
		WriteAndFreeGptData(&dev->block_dev, data);
		return NULL;
	}
	return data;
}

static int spi_gpt_fixup(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	SpiGptCtrlr *ctrlr = container_of(fixup, SpiGptCtrlr, fixup);
	ctrlr->block_ctrlr.ops.update(&ctrlr->block_ctrlr.ops);
	SpiGptDev *dev = ctrlr->dev;

	/* If in recovery mode, don't add the GPT from SPI to the dt
	 * because this would violate the requirement that recovery
	 * doesn't read internal storage from RW. */
	if (vboot_in_recovery())
		return 0;

	uint32_t addrc, sizec;
	DeviceTreeNode *nand = dt_find_node_by_path(tree->root,
						    ctrlr->dt_path,
						    &addrc, &sizec, 0);
	if (!nand) {
		printf("device node not found at path %s!\n",
		       ctrlr->dt_path);
		return 0;
	}

	GptData *gpt = read_gpt(dev);
	if (!gpt) {
		printf("SPI GPT read failed!\n");
		return 0;
	}

	ListNode *prev_child = &nand->children;

	u64 total_size = dev->block_dev.stream_block_count << BLOCK_SHIFT;
	/* TODO(chromium:436265): If we use 4GB+ NAND, update to support
	 * two-word addresses for partitions on 32-bit architectures. */
	if (total_size >> (32 * addrc) || total_size >> (32 * sizec)) {
		printf(
		       "Multiple word addresses not supported, addrc=%d, sizec=%d",
		       addrc, sizec);
		return 1;
	}

	/* Partitions are from the GPT */
	GptHeader *header = (GptHeader *)gpt->primary_header;
	GptEntry *entries = (GptEntry *)gpt->primary_entries;
	GptEntry *e;
	int i;
	/* Empty partitions are inserted between used partitions in
	 * the device tree in order to preserve the partition numbering.
	 * The first loop calculates the maximum used partition
	 * to determine how long to continue placing empty partitions. */
	int max_idx = 0;
	for (i = header->number_of_entries - 1; i >= 0; i--) {
		e = entries + i;
		if (!IsUnusedEntry(e)) {
			max_idx = i;
			break;
		}
	}
	for (i = 0, e = entries; i <= max_idx; i++, e++) {
		DeviceTreeNode *partition = xzalloc(sizeof(*partition));
		u64 start, size;
		if (IsUnusedEntry(e)) {
			/* To make an empty partition, start has to be at
			 * the logical end of the device, since size = 0
			 * is interpreted by the kernel to mean the whole
			 * device. */
			partition->name = "blank";
			start = dev->block_dev.stream_block_count
				<< BLOCK_SHIFT;
			size = 0;
		} else {
			partition->name = utf16le_to_ascii(e->name,
							   ARRAY_SIZE(e->name));
			start = e->starting_lba << BLOCK_SHIFT;
			size = (e->ending_lba - e->starting_lba + 1)
					<< BLOCK_SHIFT;
		}
		dt_add_reg_prop(partition, &start, &size, 1, addrc, sizec);
		list_insert_after(&partition->list_node, prev_child);
		prev_child = &partition->list_node;
	}
	WriteAndFreeGptData(dev, gpt);

	return 0;
}

int update_spi_gpt(struct BlockDevCtrlrOps *me)
{
	BlockDevCtrlr *blockdevctrlr = container_of(me, BlockDevCtrlr, ops);
	SpiGptCtrlr *ctrlr = container_of(blockdevctrlr, SpiGptCtrlr,
					  block_ctrlr);

	SpiGptDev *dev = xzalloc(sizeof(*dev));
	dev->ctrlr = ctrlr;
	ctrlr->dev = dev;

	if (fmap_find_area(ctrlr->fmap_region, &dev->area))
		return 1;

	uint32_t sector_size = flash_sector_size();
	if (!sector_size || dev->area.size < sector_size * 2) {
		printf(
			"GPT on SPI requires two NOR erase blocks. fmap size=%d, erase block size=%d\n",
			dev->area.size, sector_size);
		return 1;
	}

	dev->block_dev.name = "virtual_spi_gpt";
	dev->block_dev.removable = 0;
	dev->block_dev.block_size = BLOCK_SIZE;
	dev->block_dev.block_count = dev->area.size >> BLOCK_SHIFT;
	dev->block_dev.stream_block_count =
		ctrlr->stream_ctrlr->size(ctrlr->stream_ctrlr) >> BLOCK_SHIFT;
	/* could fail if flash initialization fails */
	if (dev->block_dev.stream_block_count < 0)
		return 1;

	dev->block_dev.ops.read = read_spi_gpt;
	dev->block_dev.ops.write = write_spi_gpt;
	dev->block_dev.ops.new_stream = new_stream_spi_gpt;
	dev->block_dev.external_gpt = 1;
	list_insert_after(&dev->block_dev.list_node, &fixed_block_devices);

	ctrlr->block_ctrlr.need_update = 0;

	return 0;
}

SpiGptCtrlr *new_spi_gpt(const char *fmap_region, StreamCtrlr *stream_ctrlr,
			 const char *dt_path)
{
	SpiGptCtrlr *ctrlr = xzalloc(sizeof(*ctrlr));
	ctrlr->block_ctrlr.ops.update = update_spi_gpt;
	ctrlr->block_ctrlr.need_update = 1;
	ctrlr->fmap_region = fmap_region;
	ctrlr->stream_ctrlr = stream_ctrlr;
	if (dt_path) {
		ctrlr->dt_path = dt_path;
		ctrlr->fixup.fixup = spi_gpt_fixup;
		list_insert_after(&ctrlr->fixup.list_node, &device_tree_fixups);
	}
	return ctrlr;
}
