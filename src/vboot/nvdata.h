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

#ifndef __VBOOT_NVDATA_H__
#define __VBOOT_NVDATA_H__

#include <vb2_api.h>

void nvdata_write_field_DO_NOT_USE(uint32_t field, uint32_t val);

vb2_error_t nvdata_read(struct vb2_context *ctx);
vb2_error_t nvdata_write(struct vb2_context *ctx);

vb2_error_t nvdata_cmos_read(uint8_t *buf);
vb2_error_t nvdata_disk_read(uint8_t *buf);
vb2_error_t nvdata_flash_read(uint8_t *buf);
vb2_error_t nvdata_fake_read(uint8_t *buf);

vb2_error_t nvdata_cmos_write(const uint8_t *buf);
vb2_error_t nvdata_disk_write(const uint8_t *buf);
vb2_error_t nvdata_flash_write(const uint8_t *buf);
vb2_error_t nvdata_fake_write(const uint8_t *buf);

#endif /* __VBOOT_NVDATA_H__ */
