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
#include <cbfs.h>
#include <gbb_header.h>
#include <vboot_api.h>
#include <vboot/screens.h>
#include "base/graphics.h"
#include "drivers/flash/cbfs.h"
#include "drivers/video/display.h"
#include "vboot/util/commonparams.h"

/*
 * This is the base used to specify the size and the coordinate of the image.
 * For example, height = 40 means 4.0% of the canvas (=drawing area) height.
 */
#define VB_SCALE	1000
#define VB_SCALE_HALF	(VB_SCALE / 2)	/* 50.0% */

/* Height of the text image per line relative to the canvas size */
#define VB_TEXT_HEIGHT	40		/* 4.0% */

/* Indicate width or height is automatically set based on the other value */
#define VB_SIZE_AUTO	0

/* Height of the icons relative to the canvas size */
#define VB_ICON_HEIGHT	160

/* Vertical position and size of the dividers */
#define VB_DIVIDER_WIDTH	800	/* 80.0% */
#define VB_DIVIDER_V_OFFSET	160	/* 16.0% */

/* Space between 'MODEL' and a model name */
#define VB_MODEL_PADDING	10	/* 1.0 % */

#define RETURN_ON_ERROR(function_call) do {				\
		VbError_t rv = (function_call);				\
		if (rv)							\
			return rv;					\
	} while (0)

static char initialized = 0;
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
		      char pivot)
{
	struct dentry *file;

	file = find_file_in_archive(dir, image_name);
	if (!file)
		return VBERROR_NO_IMAGE_PRESENT;

	struct scale pos = {
		.x = { .n = x, .d = VB_SCALE, },
		.y = { .n = y, .d = VB_SCALE, },
	};
	struct scale dim = {
		.x = { .n = width, .d = VB_SCALE, },
		.y = { .n = height, .d = VB_SCALE, },
	};

	return draw_bitmap((uint8_t *)dir + file->offset, file->size,
			   &pos, pivot, &dim);
}

static VbError_t draw_image(const char *image_name,
			    int32_t x, int32_t y, int32_t width, int32_t height,
			    char pivot)
{
	return draw(base_graphics, image_name, x, y, width, height, pivot);
}

static VbError_t draw_image_locale(const char *image_name, uint32_t locale,
				   int32_t x, int32_t y,
				   int32_t width, int32_t height, char pivot)
{
	VbError_t rv;
	RETURN_ON_ERROR(load_localized_graphics(locale));
	rv = draw(locale_data.archive, image_name, x, y, width, height, pivot);
	if (rv == CBGFX_ERROR_BOUNDARY && width == VB_SIZE_AUTO) {
		printf("%s: '%s' overflowed. fit it to canvas width\n",
		       __func__, image_name);
		rv = draw(locale_data.archive, image_name,
			  x, y, VB_SCALE, VB_SIZE_AUTO, pivot);
	}
	return rv;
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

static int draw_text(const char *text, int32_t x, int32_t y,
		     int32_t height, char pivot)
{
	int32_t w, h;
	char str[256];
	while (*text) {
		sprintf(str, "idx%03d_%02x.bmp", *text, *text);
		w = 0;
		h = height;
		RETURN_ON_ERROR(get_image_size(font_graphics, str, &w, &h));
		RETURN_ON_ERROR(draw(font_graphics, str,
				     x, y, VB_SIZE_AUTO, height, pivot));
		x += w;
		text++;
	}
	return VBERROR_SUCCESS;
}

static int get_text_width(const char *text, int32_t *width, int32_t *height)
{
	int32_t w, h;
	char str[256];
	while (*text) {
		sprintf(str, "idx%03d_%02x.bmp", *text, *text);
		w = 0;
		h = *height;
		RETURN_ON_ERROR(get_image_size(font_graphics, str, &w, &h));
		*width += w;
		text++;
	}
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_footer(uint32_t locale)
{
	char *hwid = NULL;
	int32_t x, y, w1, h1, w2, h2, w3, h3;

	/*
	 * Draw help URL line: 'For help visit http://.../'. It consits of
	 * three parts: [for_help_left.bmp][URL][for_help_right.bmp].
	 * Since the widths vary, we need to get the widths first then calculate
	 * the horizontal positions of the images.
	 */
	w1 = 0;
	h1 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size_locale("for_help_left.bmp", locale,
					      &w1, &h1));
	w2 = 0;
	h2 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size(base_graphics, "Url.bmp", &w2, &h2));

	w3 = 0;
	h3 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size_locale("for_help_right.bmp", locale,
					      &w3, &h3));

	/* Calculate horizontal position to centralize the combined images */
	x = (VB_SCALE - w1 - w2 - w3) / 2;
	y = VB_SCALE - VB_DIVIDER_V_OFFSET;
	if (x < 0) {
		/* images are too wide. need to fit them to canvas width */
		int32_t total;

		printf("%s: help line overflowed. fit it to canvas width\n",
		       __func__);
		total = w1 + w2 + w3;
		x = 0;
		w1 = VB_SCALE * w1 / total;
		w2 = VB_SCALE * w2 / total;
		w3 = VB_SCALE * w3 / total;
	}
	RETURN_ON_ERROR(draw_image_locale("for_help_left.bmp", locale,
			x, y, w1, VB_SIZE_AUTO,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	x += w1;
	RETURN_ON_ERROR(draw_image("Url.bmp",
			x, y, w2, VB_SIZE_AUTO,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	x += w2;
	RETURN_ON_ERROR(draw_image_locale("for_help_right.bmp", locale,
			x, y, w3, VB_SIZE_AUTO,
			PIVOT_H_LEFT|PIVOT_V_TOP));

	/*
	 * Draw model line: 'Model XYZ'. It consists of two parts: 'Model',
	 * which is locale dependent, and 'XYZ', a model name. Model name
	 * consists of individual font images: 'X' 'Y' 'Z'.
	 */
	if (is_cparams_initialized()) {
		GoogleBinaryBlockHeader *gbb = cparams.gbb_data;
		if (gbb)
			hwid = (char *)((uintptr_t)gbb + gbb->hwid_offset);
	}
	if (!hwid)
		hwid = "NOT FOUND";

	w1 = 0;
	h1 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size_locale("model_left.bmp", locale,
					      &w1, &h1));
	w1 += VB_MODEL_PADDING;

	w2 = 0;
	h2 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_text_width(hwid, &w2, &h2));
	w2 += VB_MODEL_PADDING;

	w3 = 0;
	h3 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size_locale("model_right.bmp", locale,
					      &w3, &h3));

	/* Calculate horizontal position to centralize the combined images. */
	/*
	 * No clever way to redraw the combined images when they overflow but
	 * luckily there is plenty of space for just 'model' + model name.
	 */
	x = (VB_SCALE - w1 - w2 - w3) / 2;
	y += VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(draw_image_locale("model_left.bmp", locale,
			x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	x += w1;
	RETURN_ON_ERROR(draw_text(hwid, x, y, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	x += w2;
	RETURN_ON_ERROR(draw_image_locale("model_right.bmp", locale,
			x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));

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

	/* Draw the right arrow */
	w = 0;
	h = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size(base_graphics, "arrow_right.bmp",
				       &w, &h));
	x -= w;
	RETURN_ON_ERROR(draw_image("arrow_right.bmp",
			x, VB_DIVIDER_V_OFFSET, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));

	/* Draw the language name */
	w = 0;
	h = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size_locale("language.bmp", locale, &w, &h));
	x -= w;
	RETURN_ON_ERROR(draw_image_locale("language.bmp", locale,
			x, VB_DIVIDER_V_OFFSET, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));

	/* Draw the left arrow */
	w = 0;
	h = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size(base_graphics, "arrow_left.bmp",
				       &w, &h));
	x -= w;
	RETURN_ON_ERROR(draw_image("arrow_left.bmp",
			x, VB_DIVIDER_V_OFFSET, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));

	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_base_screen(uint32_t locale)
{
	const struct rgb_color white = { 0xff, 0xff, 0xff };

	if (clear_screen(&white))
		return VBERROR_UNKNOWN;
	RETURN_ON_ERROR(draw_image("chrome_logo.bmp",
			(VB_SCALE - VB_DIVIDER_WIDTH)/2,
			/* '-10' to keep the logo lifted up from the divider */
			VB_DIVIDER_V_OFFSET - 10,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));

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

static VbError_t vboot_draw_blank(uint32_t locale)
{
	video_console_clear();
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_developer_warning(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_icon("VerificationOff.bmp"));
	RETURN_ON_ERROR(draw_image_locale("verif_off.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image_locale("devmode.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_remove(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_image_locale("remove.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF - VB_ICON_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image("RemoveDevices.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_ICON_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_no_good(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_image_locale("yuck.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image("BadDevices.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_ICON_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_insert(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_image_locale("insert.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF - VB_ICON_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image("InsertDevices.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_ICON_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_to_dev(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_image_locale("todev.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 4,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_developer_to_norm(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_icon("VerificationOff.bmp"));
	RETURN_ON_ERROR(draw_image_locale("verif_off.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image_locale("tonorm.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 3,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_wait(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_image_locale("update.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 3,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_to_norm_confirmed(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_icon("VerificationOn.bmp"));
	RETURN_ON_ERROR(draw_image_locale("verif_on.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image_locale("reboot_erase.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF + VB_TEXT_HEIGHT,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_os_broken(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_image_locale("os_broken.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_CENTER));
	return VBERROR_SUCCESS;
}

/* we may export this in the future for the board customization */
struct vboot_screen_descriptor {
	uint32_t id;				/* VB_SCREEN_* */
	VbError_t (*draw)(uint32_t locale);	/* draw function */
	const char *mesg;			/* fallback message */
};

static const struct vboot_screen_descriptor vboot_screens[] = {
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
		.id = VB_SCREEN_RECOVERY_REMOVE,
		.draw = vboot_draw_recovery_remove,
		.mesg = "Please remove all external devices to begin recovery\n",
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
};

static const struct vboot_screen_descriptor *get_screen_descriptor(uint32_t id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(vboot_screens); i++) {
		if (vboot_screens[i].id == id)
			return &vboot_screens[i];
	}
	return NULL;
}

static void print_fallback_message(const struct vboot_screen_descriptor *desc)
{
	const struct rgb_color white = { 0xff, 0xff, 0xff };

	if (desc->mesg)
		graphics_print_single_text_block(desc->mesg, &white, 0, 15,
						 VIDEO_PRINTF_ALIGN_CENTER);
	else
		clear_screen(&white);
}

static VbError_t draw_screen(uint32_t screen_type, uint32_t locale)
{
	VbError_t rv = VBERROR_UNKNOWN;
	const struct vboot_screen_descriptor *desc;

	desc = get_screen_descriptor(screen_type);
	if (!desc) {
		printf("Not a valid screen type: 0x%x\n", screen_type);
		return VBERROR_INVALID_SCREEN_INDEX;
	}

	if (locale >= locale_data.count) {
		printf("Unsupported locale (%d)\n", locale);
		print_fallback_message(desc);
		return VBERROR_INVALID_PARAMETER;
	}

	/* if no drawing function is registered, fallback msg will be printed */
	if (desc->draw) {
		rv = desc->draw(locale);
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

	if (graphics_init())
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

int vboot_draw_screen(uint32_t screen, uint32_t locale)
{
	static uint32_t current_screen = VB_SCREEN_BLANK;

	printf("%s: screen=0x%x locale=%d\n", __func__, screen, locale);

	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}

	/* If requested screen is the same as the current one, we're done. */
	if (screen == current_screen && locale == locale_data.current)
		return VBERROR_SUCCESS;

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(VB_SCREEN_BLANK == screen ? 0 : 1);

	/* TODO: draw only locale dependent part if current_screen == screen */
	RETURN_ON_ERROR(draw_screen(screen, locale));

	current_screen = screen;
	locale_data.current = locale;

	return VBERROR_SUCCESS;
}

int vboot_get_locale_count(void)
{
	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}
	return locale_data.count;
}
