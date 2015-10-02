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
#include <vboot_api.h>
#include <vboot/screens.h>
#include "drivers/video/display.h"

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

#define RETURN_ON_ERROR(function_call) do {				\
		VbError_t rv = (function_call);				\
		if (rv)							\
			return rv;					\
	} while (0)

static char initialized = 0;
static uint32_t locale_count;
static char *supported_locales[256];

static VbError_t draw_image(const char *image_name,
			    int32_t x, int32_t y, int32_t width, int32_t height,
			    char pivot)
{
	uint8_t *image;
	size_t size;
	VbError_t rv = VBERROR_SUCCESS;

	image = cbfs_get_file_content(CBFS_DEFAULT_MEDIA, image_name,
				      CBFS_TYPE_RAW, &size);
	if (!image)
		return VBERROR_NO_IMAGE_PRESENT;

	struct scale pos = {
		.x = { .n = x, .d = VB_SCALE, },
		.y = { .n = y, .d = VB_SCALE, },
	};
	struct scale dim = {
		.x = { .n = width, .d = VB_SCALE, },
		.y = { .n = height, .d = VB_SCALE, },
	};

	rv = draw_bitmap(image, size, &pos, pivot, &dim);
	if (rv)
		rv = VBERROR_UNKNOWN;
	free(image);

	return rv;
}

static int draw_image_locale(const char *image_name, uint32_t locale,
			     int32_t x, int32_t y,
			     int32_t width, int32_t height, char pivot)
{
	char str[256];
	snprintf(str, sizeof(str), "locale/%s/%s",
		 supported_locales[locale], image_name);
	return draw_image(str, x, y, width, height, pivot);
}

static VbError_t get_image_size(const char *image_name,
				int32_t *width, int32_t *height)
{
	uint8_t *image;
	size_t size;
	VbError_t rv;

	image = cbfs_get_file_content(CBFS_DEFAULT_MEDIA, image_name,
				      CBFS_TYPE_RAW, &size);
	if (!image)
		return VBERROR_NO_IMAGE_PRESENT;

	struct scale dim = {
		.x = { .n = *width, .d = VB_SCALE, },
		.y = { .n = *height, .d = VB_SCALE, },
	};

	rv = get_bitmap_dimension(image, size, &dim);
	free(image);
	if (rv)
		return VBERROR_UNKNOWN;

	*width = dim.x.n * VB_SCALE / dim.x.d;
	*height = dim.y.n * VB_SCALE / dim.y.d;

	return VBERROR_SUCCESS;
}

static VbError_t get_image_size_locale(const char *image_name, uint32_t locale,
				       int32_t *width, int32_t *height)
{
	char str[256];
	snprintf(str, sizeof(str), "locale/%s/%s",
		 supported_locales[locale], image_name);
	return get_image_size(str, width, height);
}

static int draw_icon(const char *image_name)
{
	return draw_image(image_name,
			  VB_SCALE_HALF, VB_SCALE_HALF,
			  VB_SIZE_AUTO, VB_ICON_HEIGHT,
			  PIVOT_H_CENTER|PIVOT_V_BOTTOM);
}

static VbError_t vboot_draw_footer(uint32_t locale)
{
	int32_t x, y, w1, h1, w2, h2;

	/*
	 * Draw help URL line: 'For help visit http://.../'. It consits of
	 * two parts: 'For help visit', which is locale dependent, and a URL.
	 * Since the widths vary, we need to get the widths first then calculate
	 * the horizontal positions of the images.
	 */
	w1 = 0;
	h1 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size_locale("for_help_left.bmp", locale,
					      &w1, &h1));
	w2 = 0;
	h2 = VB_TEXT_HEIGHT;
	RETURN_ON_ERROR(get_image_size("Url.bmp", &w2, &h2));

	/* Calculate horizontal position to centralize the combined images */
	x = (VB_SCALE - w1 - w2) / 2;
	y = VB_SCALE - VB_DIVIDER_V_OFFSET;
	RETURN_ON_ERROR(draw_image_locale("for_help_left.bmp", locale,
			x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	x += w1;
	RETURN_ON_ERROR(draw_image("Url.bmp",
			x, y, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));

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
	/*
	 * Language section is at the top right corner. The language text image
	 * is placed in the middle using the center as a pivot. Then, arrows
	 * are placed on each side using PIVOT_H_RIGHT and PIVOT_H_LEFT. This
	 * way, we can keep the different language images all in the middle.
	 *
	 * TODO: Get the width of the projected image and use it to determine
	 * horizontal positions relative to the right edge of the divider.
	 */
	RETURN_ON_ERROR(draw_image("arrow_left.bmp",
			770, VB_DIVIDER_V_OFFSET, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_RIGHT|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image_locale("language.bmp", locale,
			820, VB_DIVIDER_V_OFFSET, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_CENTER|PIVOT_V_BOTTOM));
	RETURN_ON_ERROR(draw_image("arrow_right.bmp",
			870, VB_DIVIDER_V_OFFSET, VB_SIZE_AUTO, VB_TEXT_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_BOTTOM));
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
	/*
	 * TODO: We need a mechanism to let boards customize these. For example,
	 * some boards have only USB.
	 */
	RETURN_ON_ERROR(draw_image("BadSD.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_ICON_HEIGHT,
			PIVOT_H_RIGHT|PIVOT_V_TOP));
	RETURN_ON_ERROR(draw_image("BadUSB.bmp",
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_ICON_HEIGHT,
			PIVOT_H_LEFT|PIVOT_V_TOP));
	return VBERROR_SUCCESS;
}

static VbError_t vboot_draw_recovery_insert(uint32_t locale)
{
	RETURN_ON_ERROR(vboot_draw_base_screen(locale));
	RETURN_ON_ERROR(draw_icon("Warning.bmp"));
	RETURN_ON_ERROR(draw_image_locale("insert.bmp", locale,
			VB_SCALE_HALF, VB_SCALE_HALF,
			VB_SIZE_AUTO, VB_TEXT_HEIGHT * 2,
			PIVOT_H_CENTER|PIVOT_V_TOP));
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
		.draw = NULL,
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
	if (clear_screen(&white))
		return;
	if (desc->mesg) {
		unsigned int rows, cols;
		video_get_rows_cols(&rows, &cols);
		video_console_set_cursor(0, rows/2);
		video_printf(0, 15, VIDEO_PRINTF_ALIGN_CENTER, desc->mesg);
	}
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

	if (locale >= locale_count) {
		printf("Unsupported locale (%d)\n", locale);
		print_fallback_message(desc);
		return VBERROR_INVALID_PARAMETER;
	}

	/* if no drawing function is registered, fallback msg will be printed */
	if (desc->draw)
		rv = desc->draw(locale);
	if (rv)
		print_fallback_message(desc);

	return rv;
}

static void vboot_init_locale(void)
{
	char *locales, *loc_start, *loc;
	size_t size;

	locale_count = 0;

	/* Load locale list from cbfs */
	locales = cbfs_get_file_content(CBFS_DEFAULT_MEDIA, "locales",
					CBFS_TYPE_RAW, &size);
	if (!locales || !size) {
		printf("%s: locale list not found", __func__);
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
			&& locale_count < ARRAY_SIZE(supported_locales)) {
		char *lang = strsep(&loc, "\n");
		if (!lang || !strlen(lang))
			break;
		printf(" %s,", lang);
		supported_locales[locale_count] = lang;
		locale_count++;
	}
	free(locales);
	printf(" (%d locales)\n", locale_count);
}

static VbError_t vboot_init_screen(void)
{
	vboot_init_locale();
	if (display_init())
		return VBERROR_UNKNOWN;

	/* initialize video console */
	video_init();
	video_console_clear();
	video_console_cursor_enable(0);
	initialized = 1;

	return VBERROR_SUCCESS;
}

int vboot_draw_screen(uint32_t screen, uint32_t locale)
{
	static uint32_t current_screen = VB_SCREEN_BLANK;
	static uint32_t current_locale = 0;

	printf("%s: screen=0x%x locale=%d\n", __func__, screen, locale);

	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}

	/* If requested screen is the same as the current one, we're done. */
	if (screen == current_screen && locale == current_locale)
		return VBERROR_SUCCESS;

	/* If the screen is blank, turn off the backlight; else turn it on. */
	backlight_update(VB_SCREEN_BLANK == screen ? 0 : 1);

	/* TODO: draw only locale dependent part if current_screen == screen */
	RETURN_ON_ERROR(draw_screen(screen, locale));

	current_screen = screen;
	current_locale = locale;

	return VBERROR_SUCCESS;
}

int vboot_get_locale_count(void)
{
	if (!initialized) {
		if (vboot_init_screen())
			return VBERROR_UNKNOWN;
	}
	return locale_count;
}
