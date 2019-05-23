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

#include <assert.h>
#include <cbfs.h>
#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>
#include <vboot/screens.h>

#include "base/list.h"
#include "boot/payload.h"
#include "drivers/flash/cbfs.h"
#include "drivers/video/display.h"
#include "vboot/util/commonparams.h"

/*
 * This is the base used to specify the size and the coordinate of the image.
 * For example, height = 40 means 4.0% of the canvas (=drawing area) height.
 */
#define VB_SCALE		1000		/* 100.0% */
#define VB_SCALE_HALF		(VB_SCALE / 2)	/* 50.0% */

/* Height of the text image per line relative to the canvas size */
#define VB_TEXT_HEIGHT		36	/* 3.6% */

/* Chrome logo size and distance from the divider */
#define VB_LOGO_HEIGHT		39	/* 3.9% */
#define VB_LOGO_LIFTUP		0

/* Indicate width or height is automatically set based on the other value */
#define VB_SIZE_AUTO		0

/* Height of the icons relative to the canvas size */
#define VB_ICON_HEIGHT		169	/* 16.9% */

/* Height of InsertDevices, RemoveDevices */
#define VB_DEVICE_HEIGHT	371	/* 37.1% */

/* Vertical position and size of the dividers */
#define VB_DIVIDER_WIDTH	900	/* 90.0% -> 5% padding on each side */
#define VB_DIVIDER_V_OFFSET	160	/* 16.0% */

/* Positions of the text for the altfw menu */
#define VB_ALTFW_SEQ_LEFT	 160
#define VB_ALTFW_NAME_LEFT   200
#define VB_ALTFW_DESC_LEFT   400

/* Space between sections of text */
#define VB_PADDING		3	/* 0.3 % */

/* Downshift for vertical characters to match middle of text in Noto Sans */
#define VB_ARROW_V_OFF		3	/* 0.3 % */

/* draw_box() uses a different scale then we do, this helps with conversions */
#define VB_TO_CANVAS(offset)	(offset * CANVAS_SCALE / VB_SCALE)

#define RETURN_ON_ERROR(function_call) do {				\
		VbError_t rv = (function_call);				\
		if (rv)							\
			return rv;					\
	} while (0)

static char initialized = 0;
static int  prev_lang_page_num = -1;
static int  prev_selected_index = -1;
static struct directory *base_graphics;
static struct directory *font_graphics;
static struct cbfs_media *ro_cbfs;
static struct {
	/* current locale */
	uint32_t current;

	/* pointer to the localized graphics data and its locale */
	uint32_t archive_locale;
	struct directory *archive;

	/* number of supported language and codes: en, ja, ... */
	uint32_t count;
	char *codes[256];
} locale_data;

/* params structure for vboot draw functions */
struct params {
	uint32_t locale;
	uint32_t selected_index;
	uint32_t disabled_idx_mask;
	uint32_t redraw_base;
	const VbScreenData *data;
};

/* struct for passing around menu string arrays */
struct menu {
	const char *const *strings;
	uint32_t count;
};

/*
 * Load archive into RAM
 */
static VbError_t load_archive(const char *name, struct directory **dest)
{
	struct directory *dir;
	struct dentry *entry;
	size_t size;
	int i;

	printf("%s: loading %s\n", __func__, name);
	*dest = NULL;

	/* load archive from cbfs */
	dir = cbfs_get_file_content(ro_cbfs, name, CBFS_TYPE_RAW, &size);
	if (!dir || !size) {
		printf("%s: failed to load %s\n", __func__, name);
		return VBERROR_INVALID_BMPFV;
	}

	/* convert endianness of archive header */
	dir->count = le32toh(dir->count);
	dir->size = le32toh(dir->size);

	/* validate the total size */
	if (dir->size != size) {
		printf("%s: archive size does not match\n", __func__);
		return VBERROR_INVALID_BMPFV;
	}

	/* validate magic field */
	if (memcmp(dir->magic, CBAR_MAGIC, sizeof(CBAR_MAGIC))) {
		printf("%s: invalid archive magic\n", __func__);
		return VBERROR_INVALID_BMPFV;
	}

	/* validate count field */
	if (get_first_offset(dir) > dir->size) {
		printf("%s: invalid count\n", __func__);
		return VBERROR_INVALID_BMPFV;
	}

	/* convert endianness of file headers */
	entry = get_first_dentry(dir);
	for (i = 0; i < dir->count; i++) {
		entry[i].offset = le32toh(entry[i].offset);
		entry[i].size = le32toh(entry[i].size);
	}

	*dest = dir;

	return VBERROR_SUCCESS;
}

static VbError_t load_localized_graphics(uint32_t locale)
{
	char str[256];

	/* check whether we've already loaded the archive for this locale */
	if (locale_data.archive) {
		if (locale_data.archive_locale == locale)
			return VBERROR_SUCCESS;
		/* No need to keep more than one locale graphics at a time */
		free(locale_data.archive);
	}

	/* compose archive name using the language code */
	snprintf(str, sizeof(str), "locale_%s.bin", locale_data.codes[locale]);
	RETURN_ON_ERROR(load_archive(str, &locale_data.archive));

	/* Remember what's cached */
	locale_data.archive_locale = locale;

	return VBERROR_SUCCESS;
}

static struct dentry *find_file_in_archive(const struct directory *dir,
					   const char *name)
{
	struct dentry *entry;
	uintptr_t start;
	int i;

	if (!dir) {
		printf("%s: archive not loaded\n", __func__);
		return NULL;
	}

	/* calculate start of the file content section */
	start = get_first_offset(dir);
	entry = get_first_dentry(dir);
	for (i = 0; i < dir->count; i++) {
		if (strncmp(entry[i].name, name, NAME_LENGTH))
			continue;
		/* validate offset & size */
		if (entry[i].offset < start
				|| entry[i].offset + entry[i].size > dir->size
				|| entry[i].offset > dir->size
				|| entry[i].size > dir->size) {
			printf("%s: '%s' has invalid offset or size\n",
			       __func__, name);
			return NULL;
		}
		return &entry[i];
	}

	printf("%s: file '%s' not found\n", __func__, name);

	return NULL;
}

/*
 * Find and draw image in archive
 */
static VbError_t draw(struct directory *dir, const char *image_name,
		      int32_t x, int32_t y, int32_t width, int32_t height,
		      uint32_t flags)
{
	struct dentry *file;
	void *bitmap;

	file = find_file_in_archive(dir, image_name);
	if (!file)
		return VBERROR_NO_IMAGE_PRESENT;
	bitmap = (uint8_t *)dir + file->offset;

	struct scale pos = {
		.x = { .n = x, .d = VB_SCALE, },
		.y = { .n = y, .d = VB_SCALE, },
	};
	struct scale dim = {
		.x = { .n = width, .d = VB_SCALE, },
		.y = { .n = height, .d = VB_SCALE, },
	};

	if (get_bitmap_dimension(bitmap, file->size, &dim))
		return VBERROR_UNKNOWN;

	if ((int64_t)dim.x.n * VB_SCALE <= (int64_t)dim.x.d * VB_DIVIDER_WIDTH)
		return draw_bitmap((uint8_t *)dir + file->offset, file->size,
				   &pos, &dim, flags);

	/*
	 * If we get here the image is too wide, so fit it to the content width.
	 * This only works if it is horizontally centered (x == VB_SCALE_HALF
	 * and flags & PIVOT_H_CENTER), but that applies to our current stuff
	 * which might be too wide (locale-dependent strings). Only exception is
	 * the "For help" footer, which was already fitted in its own function.
	 */
	printf("vbgfx: '%s' too wide, fitting to content width\n", image_name);
	dim.x.n = VB_DIVIDER_WIDTH;
	dim.x.d = VB_SCALE;
	dim.y.n = VB_SIZE_AUTO;
	dim.y.d = VB_SCALE;
	return draw_bitmap((uint8_t *)dir + file->offset, file->size,
			   &pos, &dim, flags);
}

static VbError_t draw_image(const char *image_name,
			    int32_t x, int32_t y, int32_t width, int32_t height,
			    uint32_t pivot)
{
	return draw(base_graphics, image_name, x, y, width, height, pivot);
}

static VbError_t draw_image_locale(const char *image_name, uint32_t locale,
				   int32_t x, int32_t y, int32_t w, int32_t h,
				   uint32_t flags)
{
	RETURN_ON_ERROR(load_localized_graphics(locale));
	return draw(locale_data.archive, image_name, x, y, w, h, flags);
}

static VbError_t get_image_size(struct directory *dir, const char *image_name,
				int32_t *width, int32_t *height)
{
	struct dentry *file;
	VbError_t rv;

	file = find_file_in_archive(dir, image_name);
	if (!file)
		return VBERROR_NO_IMAGE_PRESENT;

	struct scale dim = {
		.x = { .n = *width, .d = VB_SCALE, },
		.y = { .n = *height, .d = VB_SCALE, },
	};

	rv = get_bitmap_dimension((uint8_t *)dir + file->offset,
				  file->size, &dim);
	if (rv)
		return VBERROR_UNKNOWN;

	*width = dim.x.n * VB_SCALE / dim.x.d;
	*height = dim.y.n * VB_SCALE / dim.y.d;

	return VBERROR_SUCCESS;
}

static VbError_t get_image_size_locale(const char *image_name, uint32_t locale,
				       int32_t *width, int32_t *height)
{
	RETURN_ON_ERROR(load_localized_graphics(locale));
	return get_image_size(locale_data.archive, image_name, width, height);
}

static int draw_icon(const char *image_name)
{
	return draw_image(image_name,
			  VB_SCALE_HALF, VB_SCALE_HALF,
			  VB_SIZE_AUTO, VB_ICON_HEIGHT,
			  PIVOT_H_CENTER|PIVOT_V_BOTTOM);
}

static const char *get_char_image_file(int height, const char c, int32_t *width)
{
	int ret;
	static char filename[16];  // result becomes invalid after next call
	const char pattern[] = "idx%03d_%02x.bmp";

	snprintf(filename, sizeof(filename), pattern, c, c);
	*width = VB_SIZE_AUTO;
	ret = get_image_size(font_graphics, filename, width, &height);
	if (ret != VBERROR_SUCCESS) {
		snprintf(filename, sizeof(filename), pattern, '?', '?');
		*width = VB_SIZE_AUTO;
		ret = get_image_size(font_graphics, filename, width, &height);
		if (ret != VBERROR_SUCCESS)
			return NULL;
		printf("ERROR: Trying to display unprintable char: %#.2x\n", c);
	}

	return filename;
}

static int get_text_width(int32_t height, const char *text, int32_t *width)
{
	const char *char_filename;
	*width = 0;
	while (*text) {
		int char_width;
		char_filename = get_char_image_file(height, *text, &char_width);
		if (!char_filename)
			return VBERROR_NO_IMAGE_PRESENT;
		*width += char_width;
		text++;
	}
	return VBERROR_SUCCESS;
}

static int draw_text(const char *text, int32_t x, int32_t y,
		     int32_t height, uint32_t pivot)
{
	int32_t char_width;
	const char *char_filename;

	if (pivot & PIVOT_H_CENTER) {
		char_width = VB_SIZE_AUTO;
		RETURN_ON_ERROR(get_text_width(height, text, &char_width));

		pivot &= ~(PIVOT_H_CENTER);
		pivot |= PIVOT_H_LEFT;
		x -= char_width / 2;
	}

	while (*text) {
		char_filename = get_char_image_file(height, *text, &char_width);
		if (!char_filename)
			return VBERROR_NO_IMAGE_PRESENT;
		RETURN_ON_ERROR(draw(font_graphics, char_filename,
				     x, y, VB_SIZE_AUTO, height, pivot));
		x += char_width;
		text++;
	}
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_footer(uint32_t locale)
{
	int32_t x, y, w1, h1, w2, h2, w3, h3;
	int32_t total;

	/*
	 * Draw help URL line: 'For help visit http://.../'. It consists of
	 * three parts: [for_help_left.bmp][URL][for_help_right.bmp].
	 * Since the widths vary, we need to get the widths first then calculate
	 * the horizontal positions of the images.
	 */
	w1 = VB_SIZE_AUTO;
	h1 = VB_TEXT_HEIGHT;
	/* Expected to fail in locales which don't have left part */
	get_image_size_locale("for_help_left.bmp", locale, &w1, &h1);

	w2 = VB_SIZE_AUTO;
	h2 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size(base_graphics, "Url.bmp", &w2, &h2));

	w3 = VB_SIZE_AUTO;
	h3 = VB_TEXT_HEIGHT;
	/* Expected to fail in locales which don't have right part */
	get_image_size_locale("for_help_right.bmp", locale, &w3, &h3);

	total = w1 + VB_PADDING + w2 + VB_PADDING + w3;
	y = VB_SCALE - VB_DIVIDER_V_OFFSET;
	if (VB_DIVIDER_WIDTH - total >= 0) {
		/* Calculate position to centralize the images combined */
		x = (VB_SCALE - total) / 2;
		/* Expected to fail in locales which don't have left part */
		draw_image_locale("for_help_left.bmp", locale,
				  x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
				  PIVOT_H_LEFT|PIVOT_V_TOP);
		x += w1 + VB_PADDING;
		RETURN_ON_ERROR(draw_image("Url.bmp",
					   x, y,
					   VB_SIZE_AUTO, VB_TEXT_HEIGHT,
					   PIVOT_H_LEFT|PIVOT_V_TOP));
		x += w2 + VB_PADDING;
		/* Expected to fail in locales which don't have right part */
		draw_image_locale("for_help_right.bmp", locale,
				  x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
				  PIVOT_H_LEFT|PIVOT_V_TOP);
	} else {
		int32_t pad;
		/* images are too wide. need to fit them to content width */
		printf("%s: help line overflowed. fit it to content width\n",
		       __func__);
		x = (VB_SCALE - VB_DIVIDER_WIDTH) / 2;
		/* Shrink all images */
		w1 = VB_DIVIDER_WIDTH * w1 / total;
		w2 = VB_DIVIDER_WIDTH * w2 / total;
		w3 = VB_DIVIDER_WIDTH * w3 / total;
		pad = VB_DIVIDER_WIDTH * VB_PADDING / total;

		/* Render using width as a base */
		draw_image_locale("for_help_left.bmp", locale,
				  x, y, w1, VB_SIZE_AUTO,
				  PIVOT_H_LEFT|PIVOT_V_TOP);
		x += w1 + pad;
		RETURN_ON_ERROR(draw_image("Url.bmp",
					   x, y, w2, VB_SIZE_AUTO,
					   PIVOT_H_LEFT|PIVOT_V_TOP));
		x += w2 + pad;
		draw_image_locale("for_help_right.bmp", locale,
				  x, y, w3, VB_SIZE_AUTO,
				  PIVOT_H_LEFT|PIVOT_V_TOP);
	}

	/*
	 * Draw model line: 'Model XYZ'. It consists of two parts: 'Model',
	 * which is locale dependent, and 'XYZ', a model name. Model name
	 * consists of individual font images: 'X' 'Y' 'Z'.
	 */
	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = sizeof(hwid);
	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size))
		strcpy(hwid, "NOT FOUND");

	w1 = VB_SIZE_AUTO;
	h1 = VB_TEXT_HEIGHT;
	get_image_size_locale("model_left.bmp", locale, &w1, &h1);
	w1 += VB_PADDING;

	w2 = VB_SIZE_AUTO;
	h2 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_text_width(h2, hwid, &w2));
	w2 += VB_PADDING;

	w3 = VB_SIZE_AUTO;
	h3 = VB_TEXT_HEIGHT;
	get_image_size_locale("model_right.bmp", locale, &w3, &h3);

	/* Calculate horizontal position to centralize the combined images. */
	/*
	 * No clever way to redraw the combined images when they overflow but
	 * luckily there is plenty of space for just 'model' + model name.
	 */
	x = (VB_SCALE - w1 - w2 - w3) / 2;
	y += VB_TEXT_HEIGHT;
	draw_image_locale("model_left.bmp", locale,
			  x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			  PIVOT_H_LEFT|PIVOT_V_TOP);
	x += w1;
	RETURN_ON_ERROR(draw_text(hwid, x, y, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	x += w2;
	draw_image_locale("model_right.bmp", locale,
			  x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			  PIVOT_H_LEFT|PIVOT_V_TOP);

	return VBERROR_SUCCESS;
}

/*
 * Draws the language section at the top right corner. The language text image
 * is placed in the middle surrounded by arrows on each side.
 */
static VbError_t vboot_draw_language(uint32_t locale)
{
	int32_t w, h, x;

	/*
	 * Right arrow starts from the right edge of the divider, which is
	 * positioned horizontally in the center.
	 */
	x = VB_SCALE_HALF + VB_DIVIDER_WIDTH / 2;

	/* Draw right arrow */
	if (!CONFIG(DETACHABLE_UI)) {
		w = VB_SIZE_AUTO;
		h = VB_TEXT_HEIGHT;
		RETURN_ON_ERROR(draw_image("arrow_right.bmp", x,
					   VB_DIVIDER_V_OFFSET + VB_ARROW_V_OFF,
					   w, h, PIVOT_H_RIGHT|PIVOT_V_BOTTOM));
		RETURN_ON_ERROR(get_image_size(base_graphics, "arrow_right.bmp",
					       &w, &h));
		x -= w + VB_PADDING;
	}

	/* Draw language name */
	w = VB_SIZE_AUTO;
	h = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(draw_image_locale("language.bmp", locale,
					  x, VB_DIVIDER_V_OFFSET, w, h,
					  PIVOT_H_RIGHT|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(get_image_size_locale("language.bmp", locale, &w, &h));

	if (!CONFIG(DETACHABLE_UI)) {
		x -= w + VB_PADDING;

		/* Draw left arrow */
		w = VB_SIZE_AUTO;
		h = VB_TEXT_HEIGHT;
		RETURN_ON_ERROR(draw_image("arrow_left.bmp", x,
					   VB_DIVIDER_V_OFFSET + VB_ARROW_V_OFF,
					   w, h, PIVOT_H_RIGHT|PIVOT_V_BOTTOM));
	}

	return VBERROR_SUCCESS;
}

static VbError_t draw_base_screen(uint32_t locale, int show_language)
{

	if (clear_screen(&color_white))
		return VBERROR_UNKNOWN;
	RETURN_ON_ERROR(draw_image("chrome_logo.bmp",
			(VB_SCALE - VB_DIVIDER_WIDTH)/2,
			VB_DIVIDER_V_OFFSET - VB_LOGO_LIFTUP,
			VB_SIZE_AUTO, VB_LOGO_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));

	if (show_language)
		RETURN_ON_ERROR(vboot_draw_language(locale));

	RETURN_ON_ERROR(draw_image("divider_top.bmp",
			VB_SCALE_HALF, VB_DIVIDER_V_OFFSET,
			VB_DIVIDER_WIDTH, VB_SIZE_AUTO,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image("divider_btm.bmp",
			VB_SCALE_HALF, VB_SCALE - VB_DIVIDER_V_OFFSET,
			VB_DIVIDER_WIDTH, VB_SIZE_AUTO,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));

	RETURN_ON_ERROR(vboot_draw_footer(locale));

	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_base_screen(struct params *p)
{
	return draw_base_screen(p->locale, 1);
}

static VbError_t vboot_draw_base_screen_without_language(struct params *p)
{
	return draw_base_screen(p->locale, 0);
}

static VbError_t vboot_draw_blank(struct params *p)
{
	clear_screen(&color_black);
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_menu(struct params *p, const struct menu *m)
{
	int i = 0;
	int yoffset;
	uint32_t flags;

	/* find starting point y offset */
	yoffset = 0 - m->count/2;
	for (i = 0; i < m->count; i++) {
		if ((p->disabled_idx_mask & (1 << i)) != 0)
			continue;
		flags = PIVOT_H_CENTER|PIVOT_V_TOP;
		if (p->selected_index == i)
			flags |= INVERT_COLORS;
		RETURN_ON_ERROR(draw_image_locale(m->strings[i], p->locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT * yoffset,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			flags));
		if (!strncmp(m->strings[i], "lang.bmp", NAME_LENGTH)) {
			int32_t w = VB_SIZE_AUTO, h = VB_TEXT_HEIGHT;
			RETURN_ON_ERROR(get_image_size_locale(m->strings[i],
				p->locale, &w, &h));
			RETURN_ON_ERROR(draw_image("globe.bmp",
				VB_SCALE_HALF + w / 2,
				VB_SCALE_HALF + VB_TEXT_HEIGHT * yoffset,
				VB_SIZE_AUTO, VB_TEXT_HEIGHT,
				PIVOT_H_LEFT | PIVOT_V_TOP));
		}
		yoffset++;
	}

	RETURN_ON_ERROR(draw_image_locale("navigate.bmp", p->locale,
			VB_SCALE_HALF,
			VB_SCALE - VB_DIVIDER_V_OFFSET - VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));

	return VBERROR_SUCCESS;
}

/*
 * String arrays with bmp file names for detachable Menus
*/
static const char *const dev_warning_menu_files[] = {
	"dev_option.bmp", /* Developer Options */
	"debug_info.bmp", /* Show Debug Info */
	"enable_ver.bmp", /* Enable Root Verification */
	"power_off.bmp",  /* Power Off */
	"lang.bmp",       /* Language */
};

static const char *const dev_menu_files[] = {
	"boot_network.bmp", /* Boot Network Image */
	"boot_legacy.bmp",  /* Boot Legacy BIOS */
	"boot_usb.bmp",     /* Boot USB Image */
	"boot_dev.bmp",     /* Boot Developer Image */
	"cancel.bmp",       /* Cancel */
	"power_off.bmp",    /* Power Off */
	"lang.bmp",         /* Language */
};

static const char *const rec_to_dev_files[] = {
	"confirm_dev.bmp", /* Confirm enabling developer mode */
	"cancel.bmp",      /* Cancel */
	"power_off.bmp",   /* Power Off */
	"lang.bmp",        /* Language */
};

static const char *const dev_to_norm_files[] = {
	"confirm_ver.bmp", /* Confirm Enabling Verified Boot */
	"cancel.bmp",      /* Cancel */
	"power_off.bmp",   /* Power Off */
	"lang.bmp",        /* Language */
};

static const char *const options_files[] = {
	"debug_info.bmp",  /* Show Debug Info */
	"cancel.bmp",      /* Cancel */
	"power_off.bmp",   /* Power Off */
	"lang.bmp",        /* Language */
};

static VbError_t vboot_draw_developer_warning(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_icon("VerificationOff.bmp"));
	RETURN_ON_ERROR(draw_image_locale("verif_off.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image_locale("devmode.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT * 2,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_developer_warning_menu(struct params *p)
{
	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_image_locale("enable_hint.bmp", p->locale,
			VB_SCALE_HALF, VB_DIVIDER_V_OFFSET + VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	const struct menu m = { dev_warning_menu_files,
				ARRAY_SIZE(dev_warning_menu_files) };
	return vboot_draw_menu(p, &m);
}

static VbError_t vboot_draw_developer_menu(struct params *p)
{
	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));
	const struct menu m = { dev_menu_files, ARRAY_SIZE(dev_menu_files) };
	return vboot_draw_menu(p, &m);
}

static VbError_t vboot_draw_launch_diag(struct params *p)
{
	if (!IS_ENABLED(CONFIG_DIAGNOSTIC_UI))
		return VBERROR_SUCCESS;

	RETURN_ON_ERROR(draw_image_locale("launch_diag.bmp", p->locale,
			VB_SCALE_HALF,
			VB_SCALE - VB_DIVIDER_V_OFFSET - VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 3 / 4,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_no_good(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_image_locale("yuck.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF - VB_DEVICE_HEIGHT / 2,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image("BadDevices.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_ICON_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	RETURN_ON_ERROR(vboot_draw_launch_diag(p));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_insert(struct params *p)
{
	const int32_t h = VB_DEVICE_HEIGHT;
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_image_locale("insert.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF - h/2,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image("InsertDevices.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF, VB_SIZE_AUTO, h,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	RETURN_ON_ERROR(vboot_draw_launch_diag(p));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_to_dev(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_image_locale("todev.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 4,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_to_dev_menu(struct params *p)
{
	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_image_locale("disable_warn.bmp", p->locale,
			VB_SCALE_HALF, VB_DIVIDER_V_OFFSET + VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	const struct menu m = { rec_to_dev_files,
				ARRAY_SIZE(rec_to_dev_files) };
	return vboot_draw_menu(p, &m);
}

static VbError_t vboot_draw_developer_to_norm(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_icon("VerificationOff.bmp"));
	RETURN_ON_ERROR(draw_image_locale("verif_off.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image_locale("tonorm.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT * 2,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 4,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_developer_to_norm_menu(struct params *p)
{
	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_image_locale("confirm_hint.bmp", p->locale,
			VB_SCALE_HALF, VB_DIVIDER_V_OFFSET + VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	const struct menu m = { dev_to_norm_files,
				ARRAY_SIZE(dev_to_norm_files) };
	return vboot_draw_menu(p, &m);
}

static VbError_t vboot_draw_wait(struct params *p)
{
	/*
	 * Currently, language cannot be changed while EC software sync is
	 * taking place because keyboard is disabled.
	 */
	RETURN_ON_ERROR(vboot_draw_base_screen_without_language(p));
	RETURN_ON_ERROR(draw_image_locale("update.bmp", p->locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_to_norm_confirmed(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_icon("VerificationOn.bmp"));
	RETURN_ON_ERROR(draw_image_locale("verif_on.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image_locale("reboot_erase.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT * 2,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_os_broken(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	RETURN_ON_ERROR(draw_icon("Warning.bmp"));
	RETURN_ON_ERROR(draw_image_locale("os_broken.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(vboot_draw_launch_diag(p));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_languages_menu(struct params *p)
{
	int i = 0;

	/*
	 * There are too many languages to fit onto a page.  Let's try to list
	 * about 15 at a time. Since the explanatory text needs to fit on the
	 * bottom, center the list two entries higher than the screen center.
	 */
	const int lang_per_page = 15;
	const int yoffset_start = 0 - lang_per_page/2 - 2;
	int yoffset = yoffset_start;
	int selected_index = p->selected_index % locale_data.count;
	locale_data.current = selected_index;

	int page_num = selected_index / lang_per_page;
	int page_start_index = lang_per_page * page_num;
	int total_pages = locale_data.count / lang_per_page;
	if (locale_data.count % lang_per_page > 0)
		total_pages++;

	/*
	 * redraw screen if we cross a page boundary
	 * or if we're instructed to do so (because of screen change)
	 */
	if (prev_lang_page_num != page_num || p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));

	/* Print out page #s (1/5, 2/5, etc.) */
	char page_count[6];
	snprintf(page_count, sizeof(page_count), "%d/%d", page_num + 1,
		 total_pages);
	/* draw_text() cannot pivot center, so must fudge x-coord a little. */
	RETURN_ON_ERROR(draw_text(page_count, VB_SCALE_HALF - 20,
			VB_DIVIDER_V_OFFSET, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));

	/*
	 * Check if we can just redraw some entries (staying on the
	 * same page) instead of the whole page because opening the
	 * archives for each language slows things down.
	 */
	int num_lang_to_draw = lang_per_page;
	int start_index = page_start_index;
	if (prev_lang_page_num == page_num && !p->redraw_base) {
		/* Redraw selected index and previously selected index */
		num_lang_to_draw = 2;
		start_index = MIN(prev_selected_index, selected_index);
		/* previous index invalid. */
		if (prev_selected_index == -1) {
			start_index = selected_index;
			num_lang_to_draw = 1;
		}
		yoffset = yoffset_start + (start_index - page_start_index);
	}

	uint32_t flags;
	for (i = start_index;
	     i < start_index + num_lang_to_draw && i < locale_data.count;
	     i++, yoffset++) {
		flags = PIVOT_H_CENTER|PIVOT_V_TOP;
		if (selected_index == i)
			flags |= INVERT_COLORS;
		RETURN_ON_ERROR(draw_image_locale("language.bmp", i,
				VB_SCALE_HALF,
				VB_SCALE_HALF + VB_TEXT_HEIGHT * yoffset,
				VB_SIZE_AUTO, VB_TEXT_HEIGHT,
				flags));
	}
	prev_lang_page_num = page_num;
	prev_selected_index = selected_index;

	RETURN_ON_ERROR(draw_image_locale("navigate.bmp", p->locale,
			VB_SCALE_HALF,
			VB_SCALE - VB_DIVIDER_V_OFFSET - VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));

	return VBERROR_SUCCESS;
}

VbError_t vboot_print_string(char *str)
{
	// Left align with divider, begin right under it.
	int left = (VB_SCALE - VB_DIVIDER_WIDTH) / 2;
	int top = VB_DIVIDER_V_OFFSET + 10;

	char *line = str;
	int lines = 0;

	while ((line = strchr(line, '\n'))) {
		lines++;
		line++;		// skip over '\n' to keep counting
	}

	int max_height = VB_TEXT_HEIGHT;
	if (lines * max_height > VB_SCALE - top * 2)
		max_height = (VB_SCALE - top * 2) / lines;

	struct rect box = {
		{ .x = VB_TO_CANVAS(left), .y = VB_TO_CANVAS(top) },
		{ .width = VB_TO_CANVAS(VB_DIVIDER_WIDTH),
		  .height = VB_TO_CANVAS(lines * max_height) },
	};
	draw_box(&box, &color_white);

	while ((line = strsep(&str, "\n"))) {
		int height = max_height;
		int width;

		RETURN_ON_ERROR(get_text_width(height, line, &width));
		if (width > VB_DIVIDER_WIDTH)
			height = height * VB_DIVIDER_WIDTH / width;

		RETURN_ON_ERROR(draw_text(line, left, top, height,
					  PIVOT_H_LEFT|PIVOT_V_TOP));
		top += height;
	}

	return VBERROR_SUCCESS;
}

static VbError_t draw_altfw_text(int menutop, int linenum, int seqnum,
			  const char *name, const char *desc)
{
	int top = menutop + VB_TEXT_HEIGHT * linenum;

	if (seqnum != -1) {
		char seq[2] = {'0' + seqnum, '\0'};

		RETURN_ON_ERROR(draw_text(seq,
				VB_ALTFW_SEQ_LEFT,
				top,
				VB_TEXT_HEIGHT,
				PIVOT_H_LEFT|PIVOT_V_TOP));
	}

	RETURN_ON_ERROR(draw_text(name,
			VB_ALTFW_NAME_LEFT,
			top,
			VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_text(desc,
			VB_ALTFW_DESC_LEFT,
			top,
			VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));

	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_altfw_pick(struct params *p)
{
	ListNode *head;

	head = payload_get_altfw_list();
	RETURN_ON_ERROR(vboot_draw_base_screen(p));
	if (head) {
		struct altfw_info *node;
		int pos = 2;
		int node_cnt = 0;
		int top;

		list_for_each(node, *head, list_node) {
			if (!node->seqnum)
				continue;
			node_cnt++;
		}

		top = VB_SCALE_HALF - (node_cnt + 2) * VB_TEXT_HEIGHT / 2;

		RETURN_ON_ERROR(draw_image_locale("select_altfw.bmp", p->locale,
			VB_SCALE_HALF, top,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
		list_for_each(node, *head, list_node) {
			if (!node->seqnum)
				continue;
			RETURN_ON_ERROR(draw_altfw_text(top,
					pos,
					node->seqnum,
					node->name,
					node->desc));
			pos++;
		}
	}

	return VBERROR_SUCCESS;
}

#if CONFIG(DIAGNOSTIC_UI)
static VbError_t vboot_draw_confirm_diag(struct params *p)
{
	uint32_t locale = p->locale;
	RETURN_ON_ERROR(vboot_draw_base_screen(p));

	RETURN_ON_ERROR(draw_image_locale("diag_confirm.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 3,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}
#endif

static VbError_t vboot_draw_options_menu(struct params *p)
{
	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));
	const struct menu m = { options_files,
				ARRAY_SIZE(options_files) };
	return vboot_draw_menu(p, &m);
}

static VbError_t vboot_draw_altfw_menu(struct params *p)
{
	struct altfw_info *node;
	char str[256];
	ListNode *head;
	int top = VB_SCALE_HALF - VB_TEXT_HEIGHT / 2;
	uint32_t flags;

	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));

	head = payload_get_altfw_list();

	int i = 0;
	if (head) {
		int node_cnt = 0;

		list_for_each(node, *head, list_node) {
			if (!node->seqnum)
				continue;
			if ((p->disabled_idx_mask &
			    (1 << (node->seqnum - 1))) != 0)
				continue;
			node_cnt++;
		}

		top = VB_SCALE_HALF - (node_cnt + 1) * VB_TEXT_HEIGHT / 2;

		list_for_each(node, *head, list_node) {
			if (!node->seqnum)
				continue;
			if ((p->disabled_idx_mask &
			    (1 << (node->seqnum - 1))) != 0)
				continue;

			flags = PIVOT_H_CENTER|PIVOT_V_TOP;
			if (p->selected_index == node->seqnum - 1)
				flags |= INVERT_COLORS;

			snprintf(str, sizeof(str), " %s - %s ",
					node->name, node->desc);

			RETURN_ON_ERROR(draw_text(str,
					VB_SCALE_HALF,
					top + VB_TEXT_HEIGHT * i,
					VB_TEXT_HEIGHT,
					flags));
			i++;
		}
	}
	flags = PIVOT_H_CENTER|PIVOT_V_TOP;
	if (p->selected_index == 9)
		flags |= INVERT_COLORS;
	RETURN_ON_ERROR(draw_image_locale("cancel.bmp", p->locale,
			VB_SCALE_HALF, top + VB_TEXT_HEIGHT * i,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			flags));

	RETURN_ON_ERROR(draw_image_locale("navigate.bmp", p->locale,
			VB_SCALE_HALF,
			VB_SCALE - VB_DIVIDER_V_OFFSET - VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));

	return 0;
}

#if CONFIG_VENDOR_DATA_LENGTH > 0
static VbError_t vboot_draw_vendor_data_prompt(struct params *p,
					       const char *string) {
	if (p->redraw_base)
		RETURN_ON_ERROR(vboot_draw_base_screen(p));

	RETURN_ON_ERROR(draw_image_locale(string, p->locale,
			VB_SCALE_HALF,
			VB_SCALE_HALF - VB_TEXT_HEIGHT / 2,
			VB_SIZE_AUTO,
			VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_CENTER));

	RETURN_ON_ERROR(draw_text(p->data->vendor_data.input_text,
			VB_SCALE_HALF,
			VB_SCALE_HALF + VB_TEXT_HEIGHT / 2,
			VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return 0;
}

static VbError_t vboot_draw_set_vendor_data(struct params *p)
{
	return vboot_draw_vendor_data_prompt(p, "set_vendor_data.bmp");
}

static VbError_t vboot_draw_confirm_vendor_data(struct params *p)
{
	return vboot_draw_vendor_data_prompt(p, "conf_vendor_data.bmp");
}
#endif // CONFIG_VENDOR_DATA_LENGTH > 0

/* we may export this in the future for the board customization */
struct vboot_ui_descriptor {
	uint32_t id;				/* VB_SCREEN_* */
	VbError_t (*draw)(struct params *p);	/* draw function */
	const char *mesg;			/* fallback message */
};

static const struct vboot_ui_descriptor vboot_screens[] = {
	{
		.id = VB_SCREEN_BLANK,
		.draw = vboot_draw_blank,
		.mesg = NULL,
	},
	{
		.id = VB_SCREEN_DEVELOPER_WARNING,
		.draw = vboot_draw_developer_warning,
		.mesg = "OS verification is OFF\n"
			"Press SPACE to re-enable.\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_NO_GOOD,
		.draw = vboot_draw_recovery_no_good,
		.mesg = "The device you inserted does not contain Chrome OS.\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_INSERT,
		.draw = vboot_draw_recovery_insert,
		.mesg = "Chrome OS is missing or damaged.\n"
			"Please insert a recovery USB stick or SD card.\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_TO_DEV,
		.draw = vboot_draw_recovery_to_dev,
		.mesg = "To turn OS verificaion OFF, press ENTER.\n"
			"Your system will reboot and local data will be cleared.\n"
			"To go back, press ESC.\n",
	},
	{
		.id = VB_SCREEN_DEVELOPER_TO_NORM,
		.draw = vboot_draw_developer_to_norm,
		.mesg = "OS verification is OFF\n"
			"Press ENTER to confirm you wish to turn OS verification on.\n"
			"Your system will reboot and local data will be cleared.\n"
			"To go back, press ESC.\n",
	},
	{
		.id = VB_SCREEN_WAIT,
		.draw = vboot_draw_wait,
		.mesg = "Your system is applying a critical update.\n"
			"Please do not turn off.\n",
	},
	{
		.id = VB_SCREEN_TO_NORM_CONFIRMED,
		.draw = vboot_draw_to_norm_confirmed,
		.mesg = "OS verification is ON\n"
			"Your system will reboot and local data will be cleared.\n",
	},
	{
		.id = VB_SCREEN_OS_BROKEN,
		.draw = vboot_draw_os_broken,
		.mesg = "Chrome OS may be broken.\n"
			"Remove media and initiate recovery.\n",
	},
	{
		.id = VB_SCREEN_DEVELOPER_WARNING_MENU,
		.draw = vboot_draw_developer_warning_menu,
		.mesg = "Developer Warning Menu\n",
	},
	{
		.id = VB_SCREEN_DEVELOPER_MENU,
		.draw = vboot_draw_developer_menu,
		.mesg = "Developer Menu\n",
	},
	{
		.id = VB_SCREEN_RECOVERY_TO_DEV_MENU,
		.draw = vboot_draw_recovery_to_dev_menu,
		.mesg = "Recovery to Dev Menu\n",
	},
	{
		.id = VB_SCREEN_DEVELOPER_TO_NORM_MENU,
		.draw = vboot_draw_developer_to_norm_menu,
		.mesg = "Developer to Norm Menu",
	},
	{
		.id = VB_SCREEN_LANGUAGES_MENU,
		.draw = vboot_draw_languages_menu,
		.mesg = "Languages Menu",
	},
	{
		.id = VB_SCREEN_OPTIONS_MENU,
		.draw = vboot_draw_options_menu,
		.mesg = "Options Menu",
	},
	{
		.id = VB_SCREEN_ALT_FW_PICK,
		.draw = vboot_draw_altfw_pick,
		.mesg = "Alternative Firmware Menu",
	},
	{
		.id = VB_SCREEN_ALT_FW_MENU,
		.draw = vboot_draw_altfw_menu,
		.mesg = "Alternative Firmware Menu",
	},
#if CONFIG_VENDOR_DATA_LENGTH > 0
	{
		.id = VB_SCREEN_SET_VENDOR_DATA,
		.draw = vboot_draw_set_vendor_data,
		.mesg = "Set Vendor Data",
	},
	{
		.id = VB_SCREEN_CONFIRM_VENDOR_DATA,
		.draw = vboot_draw_confirm_vendor_data,
		.mesg = "Confirm Vendor Data",
	},
#endif
#if CONFIG(DIAGNOSTIC_UI)
	{
		.id = VB_SCREEN_CONFIRM_DIAG,
		.draw = vboot_draw_confirm_diag,
		.mesg = "To run diagnostics tap the power button.\n"
			"To go back, press esc.\n",
	},
#endif
};

static const struct vboot_ui_descriptor *get_ui_descriptor(uint32_t id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(vboot_screens); i++) {
		if (vboot_screens[i].id == id)
			return &vboot_screens[i];
	}
	return NULL;
}

static void print_fallback_message(const struct vboot_ui_descriptor *desc)
{
	char msg[256];

	if (!desc->mesg)
		return;

	strncpy(msg, desc->mesg, sizeof(msg) - 1);
	msg[sizeof(msg) - 1] = '\0';
	vboot_print_string(msg);
}

static VbError_t draw_ui(uint32_t screen_type, struct params *p)
{
	VbError_t rv = VBERROR_UNKNOWN;
	const struct vboot_ui_descriptor *desc;

	desc = get_ui_descriptor(screen_type);
	if (!desc) {
		printf("Not a valid screen type: 0x%x\n", screen_type);
		return VBERROR_INVALID_SCREEN_INDEX;
	}

	if (p->locale >= locale_data.count) {
		printf("Unsupported locale (%d)\n", p->locale);
		print_fallback_message(desc);
		return VBERROR_INVALID_PARAMETER;
	}

	/* if no drawing function is registered, fallback msg will be printed */
	if (desc->draw) {
		rv = desc->draw(p);
		if (rv)
			printf("Drawing failed (0x%x)\n", rv);
	}
	if (rv) {
		print_fallback_message(desc);
		return VBERROR_SCREEN_DRAW;
	}

	return VBERROR_SUCCESS;
}

static void vboot_init_locale(void)
{
	char *locales, *loc_start, *loc;
	size_t size;

	locale_data.count = 0;

	/* Load locale list from cbfs */
	locales = cbfs_get_file_content(ro_cbfs, "locales",
					CBFS_TYPE_RAW, &size);
	if (!locales || !size) {
		printf("%s: locale list not found\n", __func__);
		return;
	}

	/* Copy the file and null-terminate it */
	loc_start = malloc(size + 1);
	if (!loc_start) {
		printf("%s: out of memory\n", __func__);
		free(locales);
		return;
	}
	memcpy(loc_start, locales, size);
	loc_start[size] = '\0';

	/* Parse the list */
	printf("%s: Supported locales:", __func__);
	loc = loc_start;
	while (loc - loc_start < size
			&& locale_data.count < ARRAY_SIZE(locale_data.codes)) {
		char *lang = strsep(&loc, "\n");
		if (!lang || !strlen(lang))
			break;
		printf(" %s,", lang);
		locale_data.codes[locale_data.count] = lang;
		locale_data.count++;
	}
	free(locales);

	printf(" (%d locales)\n", locale_data.count);
}

static VbError_t vboot_init_screen(void)
{
	if (ro_cbfs == NULL) {
		ro_cbfs = cbfs_ro_media();
		if (ro_cbfs == NULL) {
			printf("No RO CBFS found.\n");
			return VBERROR_UNKNOWN;
		}
	}

	/* Make sure framebuffer is initialized before turning display on. */
	clear_screen(&color_white);
	if (display_init())
		return VBERROR_UNKNOWN;

	/* create a list of supported locales */
	vboot_init_locale();

	/* load generic (location-free) graphics data. ignore errors.
	 * fallback screens will be drawn for missing data */
	load_archive("vbgfx.bin", &base_graphics);

	/* load font graphics */
	load_archive("font.bin", &font_graphics);

	/* reset localized graphics. we defer loading it. */
	locale_data.archive = NULL;

	initialized = 1;

	return VBERROR_SUCCESS;
}

int vboot_draw_screen(uint32_t screen, uint32_t locale,
		      const VbScreenData *data)
{

	printf("%s: screen=0x%x locale=%d\n", __func__, screen, locale);

	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(VB_SCREEN_BLANK == screen ? 0 : 1);

	/* TODO: draw only locale dependent part if current_screen == screen */
	/* setting selected_index value to 0xFFFFFFFF invalidates the field */
	struct params p = { locale, 0xFFFFFFFF, 0, 1, data };
	RETURN_ON_ERROR(draw_ui(screen, &p));

	locale_data.current = locale;

	return VBERROR_SUCCESS;
}

int vboot_draw_ui(uint32_t screen, uint32_t locale,
		  uint32_t selected_index, uint32_t disabled_idx_mask,
		  uint32_t redraw_base)
{
	printf("%s: screen=0x%x locale=%d, selected_index=%d,"
	       "disabled_idx_mask=0x%x\n",
	       __func__, screen, locale, selected_index, disabled_idx_mask);

	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(screen == VB_SCREEN_BLANK ? 0 : 1);

	struct params p = { locale, selected_index,
			    disabled_idx_mask, redraw_base, NULL };
	return draw_ui(screen, &p);
}

int vboot_get_locale_count(void)
{
	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}
	return locale_data.count;
}
