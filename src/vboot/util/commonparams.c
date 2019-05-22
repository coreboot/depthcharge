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
 */

#define NEED_VB20_INTERNALS  /* Peeking into vb2_gbb_header */

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_struct.h>

#include "config.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

/* TODO(kitching): cparams is only used to contain GBB data.  Migrate to use
 * vboot2 data structures instead, and remove cparams. */
VbCommonParams cparams CPARAMS;
static int cparams_initialized = 0;

static void *gbb_copy_in(uint32_t gbb_offset, uint32_t offset, uint32_t size)
{
	uint8_t *gbb_copy = cparams.gbb_data;

	if (offset > cparams.gbb_size || offset + size > cparams.gbb_size) {
		printf("GBB component not inside the GBB.\n");
		return NULL;
	}

	void *data;
	data = flash_read(gbb_offset + offset, size);
	if (!data)
		return NULL;
	memcpy(gbb_copy + offset, data, size);
	return gbb_copy + offset;
}

static void gbb_copy_out(uint32_t gbb_offset, uint32_t offset, uint32_t size)
{
	uint8_t *gbb_copy = cparams.gbb_data;

	if ((offset > cparams.gbb_size) ||
	    ((offset + size) > cparams.gbb_size)) {
		printf("GBB component not inside the GBB\n");
		return;
	}

	flash_rewrite(gbb_offset + offset, size, gbb_copy + offset);
	return;
}

static int cparams_init(void)
{
	// Initialize cparams.
	memset(&cparams, 0, sizeof(cparams));

	FmapArea area;
	if (fmap_find_area("GBB", &area)) {
		printf("Couldn't find the GBB.\n");
		return 1;
	}

	if (area.size > CONFIG_GBB_COPY_SIZE) {
		printf("Not enough room for a copy of the GBB.\n");
		return 1;
	}

	cparams.gbb_size = area.size;
	cparams.gbb_data = &_gbb_copy_start;
	memset(cparams.gbb_data, 0, cparams.gbb_size);

	uint32_t offset = area.offset;

	struct vb2_gbb_header *header =
		gbb_copy_in(offset, 0, sizeof(struct vb2_gbb_header));
	if (!header)
		return 1;
	printf("The GBB signature is at %p and is: ", header->signature);
	for (int i = 0; i < VB2_GBB_SIGNATURE_SIZE; i++)
		printf(" %02x", header->signature[i]);
	printf("\n");

	if (memcmp(header->signature, VB2_GBB_SIGNATURE,
		   VB2_GBB_SIGNATURE_SIZE)) {
		printf("Bad signature on GBB.\n");
		return 1;
	}

	if (!gbb_copy_in(offset, header->hwid_offset, header->hwid_size))
		return 1;

	if (!gbb_copy_in(offset, header->rootkey_offset, header->rootkey_size))
		return 1;

	if (!gbb_copy_in(offset, header->recovery_key_offset,
			 header->recovery_key_size))
		return 1;

	cparams_initialized = 1;
	return 0;
}

int gbb_clear_flags(void)
{
	/* If WP is enabled, cannot write to RO-GBB. */
	if (flash_is_wp_enabled() != 0)
		return 1;

	die_if(!cparams_initialized, "cparams not yet initialized\n");

	FmapArea area;
	if (fmap_find_area("GBB", &area)) {
		printf("Couldn't find the GBB.\n");
		return 1;
	}

	struct vb2_gbb_header *header = cparams.gbb_data;
	header->flags = 0;
	gbb_copy_out(area.offset, 0, sizeof(*header));
	return 0;
}

uint32_t gbb_get_flags(void)
{
	struct vb2_gbb_header *header;

	die_if(!cparams_initialized, "cparams not yet initialized\n");

	header = cparams.gbb_data;
	return header->flags;
}

void gbb_get_hwid(char **hwid, uint32_t *size)
{
	struct vb2_gbb_header *gbb = cparams.gbb_data;
	*hwid = (char *)((uintptr_t)gbb + gbb->hwid_offset);
	*size = gbb->hwid_size;
}

static int vboot_verify_handoff(void)
{
	if (lib_sysinfo.vboot_handoff == NULL) {
		printf("vboot handoff pointer is NULL\n");
		return 1;
	}

	if (lib_sysinfo.vboot_handoff_size != sizeof(struct vboot_handoff)) {
		printf("Unexpected vboot handoff size: %d\n",
		       lib_sysinfo.vboot_handoff_size);
		return 1;
	}

	return 0;
}

static int vboot_update_shared_data(void)
{
	VbSharedDataHeader *vb_sd;
	int vb_sd_size;

	if (find_common_params((void **)&vb_sd, &vb_sd_size)) {
		printf("Unable to access VBSD\n");
		return 1;
	}

	/*
	 * If the lid is closed, kernel selection should not count down the
	 * boot tries for updates, since the OS will shut down before it can
	 * register success.
	 */
	if (!flag_fetch(FLAG_LIDSW))
		vb_sd->flags |= VBSD_NOFAIL_BOOT;

	if (flag_fetch(FLAG_WPSW))
		vb_sd->flags |= VBSD_BOOT_FIRMWARE_WP_ENABLED;

	if (CONFIG(EC_SOFTWARE_SYNC))
		vb_sd->flags |= VBSD_EC_SOFTWARE_SYNC;

	if (CONFIG(EC_SLOW_UPDATE))
		vb_sd->flags |= VBSD_EC_SLOW_UPDATE;

	if (CONFIG(EC_EFS))
		vb_sd->flags |= VBSD_EC_EFS;

	if (!CONFIG(PHYSICAL_REC_SWITCH))
		vb_sd->flags |= VBSD_BOOT_REC_SWITCH_VIRTUAL;

	return 0;
}

static int vboot_update_vbinit_flags(void)
{
	struct vboot_handoff *vboot_handoff = lib_sysinfo.vboot_handoff;
	/* VbInit was already called in coreboot, so we need to update the
	 * vboot internal flags ourself. */
	return vboot_do_init_out_flags(vboot_handoff->init_params.out_flags);
}

static int set_cparams_shared_data(void)
{
	void *blob;
	int size;
	if (find_common_params(&blob, &size))
		return 1;

	cparams.shared_data_blob = blob;
	cparams.shared_data_size = size;

	return 0;
}

int common_params_init(void)
{
	// Initialize cparams, GBB size/data.
	if (cparams_init())
		return 1;

	// Verify incoming vboot_handoff.
	if (vboot_verify_handoff())
		return 1;

	// Modify VbSharedDataHeader contents from vboot_handoff struct.
	if (vboot_update_shared_data())
		return 1;

	// Retrieve data from VbInit flags.
	if (vboot_update_vbinit_flags())
		return 1;

	// Set cparams.shared_data fields.
	if (set_cparams_shared_data())
		return 1;

	return 0;
}

int find_common_params(void **blob, int *size)
{
	struct vboot_handoff *vboot_handoff = lib_sysinfo.vboot_handoff;
	*blob = &vboot_handoff->shared_data[0];
	*size = ARRAY_SIZE(vboot_handoff->shared_data);
	return 0;
}

struct vb2_context *vboot_get_context(void)
{
	static struct vb2_context ctx;

	if (ctx.workbuf)
		return &ctx;

	die_if(lib_sysinfo.vboot_workbuf == NULL,
	       "vboot workbuf pointer is NULL\n");

	ctx.workbuf_size = VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE;
	die_if(lib_sysinfo.vboot_workbuf_size > ctx.workbuf_size,
	       "previous workbuf too big\n");

	/* Import the firmware verification workbuf, which includes
	 * vb2_shared_data. */
	ctx.workbuf = xmemalign(VB2_WORKBUF_ALIGN,
				VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE);
	memcpy(ctx.workbuf, lib_sysinfo.vboot_workbuf,
	       lib_sysinfo.vboot_workbuf_size);
	ctx.workbuf_used = lib_sysinfo.vboot_workbuf_size;

	return &ctx;
}
