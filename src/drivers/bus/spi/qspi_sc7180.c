/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2018-2020, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/barrier.h>
#include <arch/cache.h>
#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/bus/spi/qspi_sc7180.h"
#include "drivers/gpio/sc7180.h"
#include <assert.h>

#define DMA_BYTES_PER_WORD 4
#define DMA_CHAIN_DONE	0x80000000
#define CACHE_LINE_SIZE	64
#define CHUNK	0xFFC0

typedef enum {
	SDR_1BIT = 1,
	SDR_2BIT = 2,
	SDR_4BIT = 3,
	DDR_1BIT = 5,
	DDR_2BIT = 6,
	DDR_4BIT = 7,
} QspiMode;

enum {
	CS_DEASSERT,
	CS_ASSERT
};

enum {
	MASTER_READ = 0,
	MASTER_WRITE = 1,
};

static Sc7180QspiDescriptor *allocate_descriptor(Sc7180Qspi *qspi_bus)
{
	Sc7180QspiDescriptor *next;

	next = dma_malloc(sizeof(*next));
	next->data_address = 0;
	next->next_descriptor = 0;
	next->direction = MASTER_READ;
	next->multi_io_mode = 0;
	next->reserved1 = 0;
	/*
	 * QSPI controller doesn't support transfer starts with read segment.
	 * So to support read transfers that are not preceded by write, set
	 * transfer fragment bit = 1
	 */
	next->fragment = 1;
	next->reserved2 = 0;
	next->length = 0;
	next->bounce_src = 0;
	next->bounce_dst = 0;
	next->bounce_length = 0;

	if (qspi_bus->last_descriptor)
		qspi_bus->last_descriptor->next_descriptor =
					(uint32_t)(uintptr_t)next;
	else
		qspi_bus->first_descriptor = next;

	qspi_bus->last_descriptor = next;

	return next;
}

static void dma_transfer_chain(Sc7180Qspi *qspi_bus,
				Sc7180QspiDescriptor *chain)
{
	Sc7180QspiRegs *sc7180_qspi = qspi_bus->qspi_base;
	uint32_t mstr_int_status;

	write32(&sc7180_qspi->mstr_int_sts, 0xFFFFFFFF);
	write32(&sc7180_qspi->next_dma_desc_addr, (uint32_t)(uintptr_t)chain);

	while (1) {
		mstr_int_status = read32(&sc7180_qspi->mstr_int_sts);
		if (mstr_int_status & DMA_CHAIN_DONE)
			break;
	}
}

static void queue_direct_data(Sc7180Qspi *qspi_bus, uint8_t *data,
			uint32_t data_bytes, QspiMode data_mode, bool write)
{
	Sc7180QspiDescriptor *desc;

	desc = allocate_descriptor(qspi_bus);
	desc->direction = write;
	desc->multi_io_mode = data_mode;
	desc->data_address = (uint32_t)(uintptr_t)data;
	desc->length = data_bytes;

	if (write)
		dcache_clean_by_mva(data, data_bytes);
	else
		dcache_invalidate_by_mva(data, data_bytes);
}

static void queue_bounce_data(Sc7180Qspi *qspi_bus, uint8_t *data,
			uint32_t data_bytes, QspiMode data_mode, bool write)
{
	Sc7180QspiDescriptor *desc;
	uint8_t *ptr;

	desc = allocate_descriptor(qspi_bus);
	desc->direction = write;
	desc->multi_io_mode = data_mode;
	ptr = dma_memalign(CACHE_LINE_SIZE,
				ALIGN_UP(data_bytes, DMA_BYTES_PER_WORD));
	desc->data_address = (uint32_t)(uintptr_t)ptr;

	if (write)
		memcpy(ptr, data, data_bytes);
	else {
		desc->bounce_src = (uint32_t)(uintptr_t)ptr;
		desc->bounce_dst = (uint32_t)(uintptr_t)data;
	}

	/* Here bounce_length is dummy for 'write' case, only to
	 * differentiate direct_data from bounce_data to free
	 * data_address in flush_chain
	 */
	desc->bounce_length = data_bytes;
	desc->length = data_bytes;
}

static void flush_chain(Sc7180Qspi *qspi_bus)
{
	Sc7180QspiDescriptor *desc = qspi_bus->first_descriptor;
	uint8_t *data_addr = (void *)(uintptr_t)desc->data_address;
	Sc7180QspiDescriptor *curr_desc;
	uint8_t *src;
	uint8_t *dst;

	dma_transfer_chain(qspi_bus, desc);

	while (desc) {
		if (desc->direction == MASTER_READ) {
			if (desc->bounce_length == 0)
				dcache_invalidate_by_mva(data_addr,
							desc->length);
			else {
				src = (void *)(uintptr_t)desc->bounce_src;
				dst = (void *)(uintptr_t)desc->bounce_dst;
				memcpy(dst, src, desc->bounce_length);
				free(src);
			}
		} else {
			if (desc->bounce_length)
				free(data_addr);
		}
		curr_desc = desc;
		desc = (void *)(uintptr_t)desc->next_descriptor;
		free(curr_desc);
	}
	qspi_bus->first_descriptor = qspi_bus->last_descriptor = NULL;
}

static void queue_data(Sc7180Qspi *qspi_bus, uint8_t *data, uint32_t data_bytes,
	QspiMode data_mode, bool write)
{
	uint8_t *aligned_ptr;
	uint8_t *epilog_ptr;
	uint32_t prolog_bytes, aligned_bytes, epilog_bytes;

	if (data_bytes == 0)
		return;

	aligned_ptr = (uint8_t *)ALIGN_UP((uintptr_t)data, CACHE_LINE_SIZE);

	prolog_bytes = MIN(data_bytes, aligned_ptr - data);
	aligned_bytes = ALIGN_DOWN(data_bytes - prolog_bytes, CACHE_LINE_SIZE);
	epilog_bytes = data_bytes - prolog_bytes - aligned_bytes;

	epilog_ptr = data + prolog_bytes + aligned_bytes;

	if (prolog_bytes)
		queue_bounce_data(qspi_bus, data,
					prolog_bytes, data_mode, write);
	if (aligned_bytes)
		queue_direct_data(qspi_bus, aligned_ptr,
					aligned_bytes, data_mode, write);
	if (epilog_bytes)
		queue_bounce_data(qspi_bus, epilog_ptr,
					epilog_bytes, data_mode, write);
}

static void chip_assert(int value)
{
	static Sc7180GpioCfg *gpio_cfg = NULL;

	if (!gpio_cfg)
		gpio_cfg = new_sc7180_gpio_output(GPIO(68));

	gpio_cfg->ops.set(&gpio_cfg->ops, value);
}

int spi_start(SpiOps *me)
{
	chip_assert(CS_DEASSERT);
	return 0;
}

int spi_stop(SpiOps *me)
{
	chip_assert(CS_ASSERT);
	return 0;
}

int spi_xfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	QspiMode mode = SDR_1BIT;
	uint8_t *data = (uint8_t *)(out ? out : in);
	Sc7180Qspi *qspi_bus = container_of(me, Sc7180Qspi, ops);

	/* Full duplex transfer is not supported */
	if (in && out)
		return -1;

	/* Chunk data to align controller's 64K transfer limit */
	while (size) {
		queue_data(qspi_bus, data, MIN(size, CHUNK), mode, !!out);
		flush_chain(qspi_bus);

		if (size < CHUNK)
			break;

		size -= CHUNK;
		data += CHUNK;
	}
	return 0;
}

Sc7180Qspi *new_sc7180_qspi(uintptr_t base)
{
	Sc7180Qspi *qspi_bus = xzalloc(sizeof(*qspi_bus));

	qspi_bus->ops.start = &spi_start;
	qspi_bus->ops.stop  = &spi_stop;
	qspi_bus->ops.transfer = &spi_xfer;
	qspi_bus->qspi_base = (Sc7180QspiRegs *)base;
	return qspi_bus;
}
