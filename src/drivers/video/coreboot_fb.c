/*
 * Copyright 2012 Google Inc.
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
#include <stdint.h>
#include <sysinfo.h>

#include "base/bitmap.h"
#include "drivers/video/coreboot_fb.h"

static inline void dc_corebootfb_draw_pixel(uint32_t x, uint32_t y,
					    uint32_t red, uint32_t green,
					    uint32_t blue,
					    struct cb_framebuffer *fbinfo,
					    unsigned char *fbaddr)
{
	const uint32_t xres = fbinfo->x_resolution;
	const int bpp = fbinfo->bits_per_pixel;

	uint32_t color = 0;
	color |= (red >> (8 - fbinfo->red_mask_size))
		<< fbinfo->red_mask_pos;
	color |= (green >> (8 - fbinfo->green_mask_size))
		<< fbinfo->green_mask_pos;
	color |= (blue >> (8 - fbinfo->blue_mask_size))
		<< fbinfo->blue_mask_pos;

	uint8_t *pixel = fbaddr + (x + y * xres) * bpp / 8;
	for (int i = 0; i < bpp / 8; i++)
		pixel[i] = (color >> (i * 8));
}

static int dc_corebootfb_draw_bitmap_v2(uint32_t x, uint32_t y,
					void *bitmap,
					struct cb_framebuffer *fbinfo,
					unsigned char *fbaddr)
{
	printf("Bitmap version 2 not supported.\n");
	return -1;
}

static int dc_corebootfb_draw_bitmap_v3(uint32_t x, uint32_t y,
					void *bitmap,
					struct cb_framebuffer *fbinfo,
					unsigned char *fbaddr)
{
	BitmapFileHeader *file_header_ptr = (BitmapFileHeader *)bitmap;
	uint32_t bitmap_offset;
	memcpy(&bitmap_offset, &file_header_ptr->bitmap_offset,
		sizeof(bitmap_offset));
	BitmapHeaderV3 header;
	memcpy(&header, (uint8_t *)bitmap + sizeof(BitmapFileHeader),
		sizeof(header));
	int bpp = header.bits_per_pixel;

	// Check for things we don't support.
	if (header.compression != 0 || bpp >= 16) {
		printf("Non-palette bitmaps are not supported.\n");
		return -1;
	}
	if (bpp != 1 && bpp != 4 && bpp != 8) {
		printf("Unsupported bits per pixel.\n");
		return -1;
	}

	uintptr_t palette_offset =
		sizeof(BitmapFileHeader) + sizeof(BitmapHeaderV3);
	int palette_size = bitmap_offset - palette_offset;
	BitmapPaletteElementV3 *palette = xmalloc(palette_size);
	memcpy(palette, (uint8_t *)bitmap + palette_offset, palette_size);

	int32_t width = header.width, height = header.height;
	int extra = width % 4;
	const int32_t padding = extra ? (4 - extra) : 0;
	int32_t ystep = -1;
	if (height < 0) {
		height = -height;
		ystep = -ystep;
	} else {
		y += height - 1;
	}
	uint8_t *cur_data = (uint8_t *)bitmap + bitmap_offset;
	uint32_t x_offset = 0, y_offset = 0;
	int bit = 0;
	// Loop over all the pixels in the image.
	for (uint32_t pixel = 0; pixel < width * height; pixel++) {
		int index = 0;
		// Extract the index value for this pixel.
		if (bpp >= 8) {
			// For pixels one byte or larger, glue them together
			// one byte at a time. Pixels are big endian.
			for (int i = 0; i < bpp / 8; i++) {
				index <<= 8;
				index |= *cur_data++;
			}
		} else {
			// For pixels smaller than a byte, extract some bits
			// from the right byte. Bytes are big endian.
			const uint8_t mask = (1 << bpp) - 1;
			bit += bpp;
			index = (*cur_data >> (8 - bit)) & mask;
			if (bit == 8) {
				bit = 0;
				cur_data++;
			}
		}

		// Actually draw that pixel on the display.
		dc_corebootfb_draw_pixel(x + x_offset,
					 y + y_offset,
					 palette[index].red,
					 palette[index].green,
					 palette[index].blue,
					 fbinfo,
					 fbaddr);

		// Keep track of position.
		if (++x_offset == width) {
			x_offset = 0;
			y_offset += ystep;
			cur_data += padding;
		}
	}
	free(palette);
	return 0;
}

static int dc_corebootfb_draw_bitmap_v4(uint32_t x, uint32_t y,
					void *bitmap,
					struct cb_framebuffer *fbinfo,
					unsigned char *fbaddr)
{
	printf("Bitmap version 4 not supported.\n");
	return -1;
}

int dc_corebootfb_draw_bitmap(uint32_t x, uint32_t y, void *bitmap)
{
	BitmapFileHeader *file_header = (BitmapFileHeader *)bitmap;

	if (file_header->signature[0] != 'B' ||
	    file_header->signature[1] != 'M') {
		printf("Bitmap signature check failed.\n");
		return -1;
	}

	struct cb_framebuffer *fbinfo = lib_sysinfo.framebuffer;
	if (!fbinfo)
		return 0;
	unsigned char *fbaddr =
		(unsigned char *)(uintptr_t)(fbinfo->physical_address);
	if (!fbaddr)
		return -1;

	uint32_t header_size;
	memcpy(&header_size, file_header + 1, sizeof(header_size));
	switch (header_size) {
	case 12:
		return dc_corebootfb_draw_bitmap_v2(x, y, bitmap,
						    fbinfo, fbaddr);
	case 40:
		return dc_corebootfb_draw_bitmap_v3(x, y, bitmap,
						    fbinfo, fbaddr);
	case 108:
		return dc_corebootfb_draw_bitmap_v4(x, y, bitmap,
						    fbinfo, fbaddr);
	default:
		printf("Unrecognized bitmap format.\n");
		return -1;
	}
	return 0;
}
