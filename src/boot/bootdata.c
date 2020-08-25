/*
 * Copyright (C) 2017 The Fuchsia Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    - Neither the name of Google Inc. nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libpayload.h>
#include <stdlib.h>

#include "boot/bootdata.h"
#include "vboot/boot_policy.h"
#include "vboot/boot.h"

static int add_bootdata(void **ptr, size_t *avail, bootdata_t *bd, void *data)
{
	size_t len, offset;
	bootextra_t extra = {
		.magic = BOOTITEM_MAGIC,
		.crc32 = BOOTITEM_NO_CRC32,
	};

	if (!ptr || !avail || !bd || !data)
		return -1;

	len = BOOTDATA_ALIGN(bd->length);
	if ((sizeof(bootdata_t) + sizeof(bootextra_t) + len) > *avail) {
		printf("%s: no room for bootdata type=0x%08x size=%u\n",
		       __func__, bd->type, bd->length);
		return -1;
	}

	// Copy bootdata header into place
	memcpy(*ptr, bd, sizeof(*bd));
	offset = sizeof(*bd);
	// Copy bootextra header into place
	if (bd->flags & BOOTDATA_FLAG_EXTRA) {
		memcpy(*ptr + offset, &extra, sizeof(extra));
		offset += sizeof(extra);
	}
	// Copy data into place after headers
	memcpy(*ptr + offset, data, bd->length);
	// Clear alignment bytes at the end if necessary
	if (len > bd->length)
		memset(*ptr + offset + bd->length, 0, len - bd->length);

	len += offset;
	*ptr += len;
	*avail -= len;

	return 0;
}

// Convert RGB mask size into MX pixel format
static int bootdata_framebuffer_format(struct cb_framebuffer *fb)
{
	if (fb->red_mask_size == 8 &&
	    fb->green_mask_size == 8 &&
	    fb->blue_mask_size == 8)
		return MX_PIXEL_FORMAT_RGB_x888;

	if (fb->red_mask_size == 3 &&
	    fb->green_mask_size == 3 &&
	    fb->blue_mask_size == 2)
		return MX_PIXEL_FORMAT_RGB_332;

	if (fb->red_mask_size == 5 &&
	    fb->green_mask_size == 6 &&
	    fb->blue_mask_size == 5)
		return MX_PIXEL_FORMAT_RGB_565;

	if (fb->red_mask_size == 2 &&
	    fb->green_mask_size == 2 &&
	    fb->blue_mask_size == 2)
		return MX_PIXEL_FORMAT_RGB_2220;

	return MX_PIXEL_FORMAT_NONE;
}

int bootdata_prepare(struct boot_info *bi)
{
	bootdata_t *image = bi->ramdisk_addr;
	bootextra_t extra = {
		.magic = BOOTITEM_MAGIC,
		.crc32 = BOOTITEM_NO_CRC32,
	};
	bootdata_t hdr;
	void *bptr;
	size_t blen;

	if (!image) {
		printf("%s: bootdata image missing\n", __func__);
		return -1;
	}
	if (image->type != BOOTDATA_CONTAINER ||
	    image->extra != BOOTDATA_MAGIC) {
		printf("%s: invalid bootdata header at %p\n", __func__, image);
		return -1;
	}

	// Allocate space for bootdata at the start of ramdisk
	memmove(bi->ramdisk_addr + CrosParamSize, bi->ramdisk_addr,
		bi->ramdisk_size);
	image = bi->ramdisk_addr + CrosParamSize;
	bi->ramdisk_size += CrosParamSize;

	// Prepare bootdata region
	bptr = bi->ramdisk_addr;
	blen = CrosParamSize;
	memset(bptr, 0, blen);

	// Add container to describe bootdata + ramdisk
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = BOOTDATA_CONTAINER;
	hdr.length = image->length + CrosParamSize;
	hdr.extra = BOOTDATA_MAGIC;
	if (image->flags & BOOTDATA_FLAG_EXTRA)
		hdr.flags = BOOTDATA_FLAG_EXTRA;
	memcpy(bptr, &hdr, sizeof(hdr));
	bptr += sizeof(hdr);
	blen -= sizeof(hdr);

	// Add extra header
	if (hdr.flags & BOOTDATA_FLAG_EXTRA) {
		memcpy(bptr, &extra, sizeof(extra));
		bptr += sizeof(extra);
		blen -= sizeof(extra);
	}

	// Add serial console descriptor
	if (lib_sysinfo.cb_serial) {
		struct cb_serial *serial = phys_to_virt(lib_sysinfo.cb_serial);
		bootdata_uart_t uart = {
			.type = serial->type,
			.base = serial->baseaddr,
		};
		hdr.type = BOOTDATA_DEBUG_UART;
		hdr.length = sizeof(uart);
		hdr.extra = 0;
		if (add_bootdata(&bptr, &blen, &hdr, &uart))
			printf("%s: Failed to add uart data\n", __func__);
		else
			printf("%s: Added uart type %u base 0x%lx\n",
			       __func__, uart.type, (unsigned long)uart.base);
	}

	// Add framebuffer descriptor
	if (lib_sysinfo.framebuffer.physical_address != 0) {
		struct cb_framebuffer *fb = &lib_sysinfo.framebuffer;
		bootdata_swfb_t swfb = {
			.phys_base = fb->physical_address,
			.width = fb->x_resolution,
			.height = fb->y_resolution,
			.stride = fb->x_resolution,
			.format = bootdata_framebuffer_format(fb)
		};
		hdr.type = BOOTDATA_FRAMEBUFFER;
		hdr.length = sizeof(swfb);
		hdr.extra = 0;
		if (add_bootdata(&bptr, &blen, &hdr, &swfb))
			printf("%s: Failed to add framebuffer\n", __func__);
		else
			printf("%s: Added framebuffer base 0x%lx format %d\n",
			       __func__, (unsigned long)swfb.phys_base,
			       swfb.format);
	}

	// Add lastlog descriptor if a ramoops buffer is present
	if (lib_sysinfo.ramoops_buffer && lib_sysinfo.ramoops_buffer_size) {
		bootdata_lastlog_nvram_t lastlog = {
			.base = lib_sysinfo.ramoops_buffer,
			.length = lib_sysinfo.ramoops_buffer_size,
		};
		hdr.type = BOOTDATA_LASTLOG_NVRAM;
		hdr.length = sizeof(lastlog);
		hdr.extra = 0;
		if (add_bootdata(&bptr, &blen, &hdr, &lastlog))
			printf("%s: Failed to add lastlog\n", __func__);
		else
			printf("%s: Added lastlog base 0x%lx size %lu\n",
			       __func__, (unsigned long)lastlog.base,
			       (unsigned long)lastlog.length);
	}

	// Ignore remaining space before the ramdisk
	if ((blen < (sizeof(bootdata_t) + sizeof(bootextra_t))) || (blen & 7)) {
		printf("%s: invalid bootdata length %zd\n", __func__, blen);
		return -1;
	}
	hdr.type = BOOTDATA_IGNORE;
	hdr.length = blen;
	hdr.extra = 0;
	memcpy(bptr, &hdr, sizeof(hdr));

	// Add extra header
	if (hdr.flags & BOOTDATA_FLAG_EXTRA) {
		bptr += sizeof(hdr);
		memcpy(bptr, &extra, sizeof(extra));
	}

	return 0;
}
