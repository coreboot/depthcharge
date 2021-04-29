/*
 * Copyright 2021 The Fuchsia Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "boot/commandline.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"
#include "boot/zbi/driver-config.h"
#include "boot/zbi/pixelformat.h"
#include "boot/zbi/zbi.h"
#include "boot/zbi/libzbi.h"
#include "boot/zbi.h"

// Convert RGB mask size into Zircon pixel format
static int zbi_framebuffer_format(struct cb_framebuffer *fb)
{
	if (fb->red_mask_size == 8 && fb->green_mask_size == 8 &&
	    fb->blue_mask_size == 8)
		return ZX_PIXEL_FORMAT_RGB_x888;

	if (fb->red_mask_size == 3 && fb->green_mask_size == 3 &&
	    fb->blue_mask_size == 2)
		return ZX_PIXEL_FORMAT_RGB_332;

	if (fb->red_mask_size == 5 && fb->green_mask_size == 6 &&
	    fb->blue_mask_size == 5)
		return ZX_PIXEL_FORMAT_RGB_565;

	if (fb->red_mask_size == 2 && fb->green_mask_size == 2 &&
	    fb->blue_mask_size == 2)
		return ZX_PIXEL_FORMAT_RGB_2220;

	return ZX_PIXEL_FORMAT_NONE;
}

struct zbi_find_cmdline_ctx {
	void *cmdline_payload;
	uint32_t cmdline_size;
};
static zbi_result_t zbi_find_cmdline(zbi_header_t *hdr, void *payload, void *cookie)
{
	struct zbi_find_cmdline_ctx *ctx = (struct zbi_find_cmdline_ctx*) cookie;
	if (hdr->type != ZBI_TYPE_CMDLINE)
		return ZBI_RESULT_OK;

	if (ctx->cmdline_payload != NULL) {
		printf("%s: found more than one cmdline item\n", __func__);
		return ZBI_RESULT_TOO_BIG;
	}
	// Mark the ZBI_TYPE_CMDLINE as discarded.
	hdr->type = ZBI_TYPE_DISCARD;
	ctx->cmdline_size = hdr->length;
	ctx->cmdline_payload = payload;
	return ZBI_RESULT_OK;
}

int zbi_fill_boot_info(struct boot_info *bi)
{
	// Find the ZBI cmdline so we can do string substitution on it.
	// We assume that the ZBI only has one ZBI_TYPE_CMDLINE entry that will
	// need substitution, and we ignore all the others.
	zbi_header_t *image = bi->ramdisk_addr;
	zbi_result_t result = zbi_check(image, NULL);
	if (result != ZBI_RESULT_OK) {
		// This is not a fatal error.
		return 0;
	}

	struct zbi_find_cmdline_ctx ctx = {
		.cmdline_payload = NULL,
		.cmdline_size = 0,
	};
	result = zbi_for_each(image, zbi_find_cmdline, &ctx);
	if (result != ZBI_RESULT_OK) {
		printf("%s: will only modify first cmdline.\n", __func__);
	}
	bi->cmd_line = ctx.cmdline_payload;

	return 0;
}

int zbi_prepare(struct boot_info *bi)
{
	zbi_header_t *image = bi->ramdisk_addr;
	zbi_result_t result = zbi_check(image, NULL);
	if (result != ZBI_RESULT_OK) {
		printf("%s: invalid ZBI at %p: %d\n", __func__, image, result);
		return -1;
	}

	size_t max_length = image->length + CrosParamSize;

	if (lib_sysinfo.cb_serial && CONFIG(ARCH_X86)) {
		struct cb_serial *serial = phys_to_virt(lib_sysinfo.cb_serial);
		if (serial->type == CB_SERIAL_TYPE_IO_MAPPED) {
			dcfg_simple_pio_t uart = {
				.base = serial->baseaddr
			};
			result = zbi_create_entry_with_payload(
				image, max_length, ZBI_TYPE_KERNEL_DRIVER,
				KDRV_I8250_PIO_UART, 0, &uart, sizeof(uart));
		} else if (serial->type == CB_SERIAL_TYPE_MEMORY_MAPPED) {
			dcfg_simple_t uart = {
				.mmio_phys = serial->baseaddr,
			};
			result = zbi_create_entry_with_payload(
				image, max_length, ZBI_TYPE_KERNEL_DRIVER,
				KDRV_I8250_MMIO_UART, 0, &uart, sizeof(uart));
		} else {
			result = ZBI_RESULT_ERROR;
		}
		if (result != ZBI_RESULT_OK)
			printf("%s: Failed to add uart data: %d\n", __func__,
			       result);
		else
			printf("%s: Added %s uart base 0x%lx\n", __func__,
			       (serial->type == CB_SERIAL_TYPE_MEMORY_MAPPED
					? "mmio"
					: "io-port"),
			       (unsigned long)serial->baseaddr);
	}

	// Add framebuffer descriptor
	if (lib_sysinfo.framebuffer.physical_address != 0) {
		struct cb_framebuffer *fb = &lib_sysinfo.framebuffer;
		zbi_swfb_t swfb = {
			.base = fb->physical_address,
			.width = fb->x_resolution,
			.height = fb->y_resolution,
			.stride = fb->x_resolution,
			.format = zbi_framebuffer_format(fb)
		};
		result = zbi_create_entry_with_payload(image, max_length,
						       ZBI_TYPE_FRAMEBUFFER, 0,
						       0, &swfb, sizeof(swfb));
		if (result != ZBI_RESULT_OK)
			printf("%s: Failed to add framebuffer: %d\n", __func__,
			       result);
		else
			printf("%s: Added framebuffer base 0x%lx format %d\n",
			       __func__, (unsigned long)swfb.base, swfb.format);
	}

	// Add nvram descriptor if a ramoops buffer is present
	if (lib_sysinfo.ramoops_buffer && lib_sysinfo.ramoops_buffer_size) {
		zbi_nvram_t lastlog = {
			.base = lib_sysinfo.ramoops_buffer,
			.length = lib_sysinfo.ramoops_buffer_size,
		};
		result = zbi_create_entry_with_payload(
			image, max_length, ZBI_TYPE_NVRAM, 0, 0, &lastlog,
			sizeof(lastlog));
		if (result != ZBI_RESULT_OK)
			printf("%s: Failed to add lastlog: %d\n", __func__,
			       result);
		else
			printf("%s: Added lastlog base 0x%lx size %lu\n",
			       __func__, (unsigned long)lastlog.base,
			       (unsigned long)lastlog.length);
	}

	// Put the cmdline into the ZBI.
	if (bi->cmd_line) {
		result = zbi_create_entry_with_payload(image, max_length, ZBI_TYPE_CMDLINE, 0, 0, bi->cmd_line, strlen(bi->cmd_line) + 1);
		if (result != ZBI_RESULT_OK) {
			printf("%s: Failed to add new cmdline: %d\n", __func__, result);
		} else {
			printf("%s: Set ZBI cmdline to %s\n", __func__, bi->cmd_line);
			// Don't add it to the multiboot header.
			bi->cmd_line = NULL;
		}
	}

	return 0;
}


