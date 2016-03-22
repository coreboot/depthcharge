/*
 * Copyright 2013 Google Inc.
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

#ifndef __DRIVERS_FLASH_FLASH_H__
#define __DRIVERS_FLASH_FLASH_H__

#include <stdint.h>

typedef struct FlashOps
{
	/* Return a pointer to the read data in the flash driver cache. */
	void *(*read)(struct FlashOps *me, uint32_t offset, uint32_t size);
	/* Return the number of successfully written bytes */
	int (*write)(struct FlashOps *me, const void *buffer,
		     uint32_t offset, uint32_t size);
	/* Return the number of successfully erased bytes.
	 * Offset and size must be erase_size-aligned. */
	int (*erase)(struct FlashOps *me, uint32_t offset, uint32_t size);
	/* Check if soft write-protect bit is set. */
	int (*is_wp_enabled)(struct FlashOps *me);
	int (*write_status)(struct FlashOps *me, uint8_t status);
	/* Reads status and returns -1 on error, status reg value on success. */
	int (*read_status)(struct FlashOps *me);
	/* Granularity and alignment of erases */
	uint32_t sector_size;
	/* Total number of sectors present */
	uint32_t sector_count;
} FlashOps;

/* Functions operating on flash_ops */
void flash_set_ops(FlashOps *ops);
void *flash_read(uint32_t offset, uint32_t size);
int flash_write(uint32_t offset, uint32_t size, const void *buffer);
int flash_erase(uint32_t offset, uint32_t size);
uint32_t flash_sector_size(void);
int flash_rewrite(uint32_t start, uint32_t length, const void *buffer);
int flash_write_status(uint8_t status);
int flash_read_status(void);
int flash_is_wp_enabled(void);

/* Functions operating on passed in ops */
void *flash_read_ops(FlashOps *ops, uint32_t offset, uint32_t size);
int flash_write_ops(FlashOps *ops, uint32_t offset, uint32_t size,
		    const void *buffer);
int flash_erase_ops(FlashOps *ops, uint32_t offset, uint32_t size);
int flash_rewrite_ops(FlashOps *ops, uint32_t start, uint32_t length,
		      const void *buffer);


#endif /* __DRIVERS_FLASH_FLASH_H__ */
