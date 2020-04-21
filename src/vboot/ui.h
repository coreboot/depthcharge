/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google Inc.
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
 * Functions to display UI in the firmware screen.
 *
 * Currently libpayload only allows drawing within the canvas, which is the
 * maximal square drawing area located in the center of the screen. For example,
 * in landscape mode, the canvas stretches vertically to the edges of the
 * screen, leaving non-drawing areas on the left and right. In this case, the
 * canvas height will be the same as the screen height, but the canvas width
 * will be less than the screen width.
 *
 * As a result, all the offsets (x and y coordinates) appearing in the drawing
 * functions are relative to the canvas, not the screen.
 */

#ifndef __VBOOT_UI_H__
#define __VBOOT_UI_H__

#include <libpayload.h>
#include <vb2_api.h>

#define _UI_PRINT(fmt, args...) printf("%s: " fmt, __func__, ##args)
#define UI_INFO(...) _UI_PRINT(__VA_ARGS__)
#define UI_WARN(...) _UI_PRINT(__VA_ARGS__)
#define UI_ERROR(...) _UI_PRINT(__VA_ARGS__)

/*
 * This is the base used to specify the size and the coordinate of the image.
 * For example, height = 40 means 4.0% of the canvas height.
 */
#define UI_SCALE				1000

/* Margins for all screens. Nothing should be drawn within the margin. */
#define UI_MARGIN_TOP				30
#define UI_MARGIN_BOTTOM			70
#define UI_MARGIN_H				50

/* For language item */
#define UI_LANG_BOX_HEIGHT			40
#define UI_LANG_MARGIN_BOTTOM			120

/* For screen icon */
#define UI_ICON_HEIGHT				45
#define UI_ICON_MARGIN_BOTTOM			50

/* For step icons */
#define UI_STEP_ICON_HEIGHT			28
#define UI_STEP_ICON_MARGIN_H			7
#define UI_STEP_ICON_SEPARATOR_WIDTH		60

/* For title and descriptions */
#define UI_TITLE_TEXT_HEIGHT			56
#define UI_TITLE_MARGIN_BOTTOM			20
#define UI_DESC_TEXT_HEIGHT			30
#define UI_DESC_TEXT_LINE_SPACING		5
#define UI_DESC_MARGIN_BOTTOM			50

/* For buttons */
#define UI_BUTTON_HEIGHT			40
#define UI_BUTTON_TEXT_HEIGHT			26
#define UI_BUTTON_TEXT_PADDING_H		40
#define UI_BUTTON_BORDER_THICKNESS		2
#define UI_BUTTON_BORDER_RADIUS			8
#define UI_BUTTON_MARGIN_BOTTOM			10

/* For footer */
#define UI_FOOTER_HEIGHT			108
#define UI_FOOTER_COL1_MARGIN_RIGHT		20
#define UI_FOOTER_COL2_PARA_SPACING		16
#define UI_FOOTER_COL2_MARGIN_RIGHT		40
#define UI_FOOTER_COL3_MARGIN_LEFT		40
#define UI_FOOTER_COL3_LINE_SPACING		10
#define UI_FOOTER_COL3_ICON_MARGIN_TOP		20
#define UI_FOOTER_COL3_ICON_HEIGHT		30
#define UI_FOOTER_COL3_ICON_SPACING		5

/*
 * UI_BOX_* constants define a large textbox taking up the width of the screen.
 * They are used for
 * (1) a fallback message in the case of a broken screen, and
 * (2) a main UI element for viewing a large body of text.
 */
#define UI_BOX_TEXT_HEIGHT			30
#define UI_BOX_MARGIN_V				275
#define UI_BOX_PADDING_H			20
#define UI_BOX_PADDING_V			15
#define UI_BOX_BORDER_THICKNESS			2
#define UI_BOX_BORDER_RADIUS			6

/* Indicate width or height is automatically set based on the other value */
#define UI_SIZE_AUTO				0
/*
 * Minimum size that is guaranteed to show up as at least 1 pixel on the screen,
 * provided that the canvas resolution is at least 500 (UI_SCALE / 2). Pixels
 * may be dropped on devices with screen resolution 640x480.
 */
#define UI_SIZE_MIN				2

static const struct rgb_color ui_color_bg		= { 0x20, 0x21, 0x24 };
static const struct rgb_color ui_color_fg		= { 0xcc, 0xcc, 0xcc };
static const struct rgb_color ui_color_button		= { 0x8a, 0xb4, 0xf8 };
static const struct rgb_color ui_color_border		= { 0x43, 0x44, 0x46 };

struct ui_bitmap {
	const void *data;
	size_t size;
};

struct ui_locale {
	const char *code;	/* Language code */
	int rtl;		/* Whether locale is right-to-left */
};

/******************************************************************************/
/* archive.c */

/*
 * Get locale information.
 *
 * This function will load the locale data from CBFS only on the first call.
 * Subsequent calls with the same locale_id are guaranteed to set an identical
 * pointer.
 *
 * @param locale_id	Locale id.
 * @param locale	Pointer to a ui_locale struct pointer to be set.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_locale_info(uint32_t locale_id,
			       struct ui_locale const **locale);

/*
 * Get generic (locale-independent) bitmap.
 *
 * @param image_name	Image file name.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_bitmap(const char *image_name, struct ui_bitmap *bitmap);

/*
 * Get locale-dependent bitmap.
 *
 * @param image_name	Image file name.
 * @param locale_code	Language code of the locale.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_localized_bitmap(const char *image_name,
				    const char *locale_code,
				    struct ui_bitmap *bitmap);

/* Character style. */
enum ui_char_style {
	UI_CHAR_STYLE_DEFAULT = 0,	/* foreground color */
	UI_CHAR_STYLE_DARK = 1,		/* darker color */
};

/*
 * Get character bitmap.
 *
 * @param c		Character.
 * @param style		Style of the character.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_char_bitmap(const char c, enum ui_char_style style,
			       struct ui_bitmap *bitmap);

/*
 * Get bitmap of step icon.
 *
 * @param step		Step number.
 * @param focused	1 for focused and 0 for non-focused.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_step_icon_bitmap(int step, int focused,
				    struct ui_bitmap *bitmap);

/*
 * Get bitmap of menu item.
 *
 * @param image_name	Non-focused image name.
 * @param locale_code	Language code of current locale.
 * @param focused	1 for focused and 0 for non-focused.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_menu_item_bitmap(const char *image_name,
				    const char *locale_code,
				    int focused, struct ui_bitmap *bitmap);

/******************************************************************************/
/* draw.c */

/*
 * Draw bitmap.
 *
 * @param bitmap	Bitmap to draw.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordicate of the top-left corner.
 * @param width		Width of the image.
 * @param height	Height of the image.
 * @param flags		Flags passed to draw_bitmap() in libpayload.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_bitmap(const struct ui_bitmap *bitmap,
			   int32_t x, int32_t y, int32_t width, int32_t height,
			   uint32_t flags, int reverse);

/*
 * Get bitmap width.
 *
 * @param bitmap	Pointer to the bitmap struct.
 * @param height	Height of the image.
 * @param width		Width of the image, calculated from the height to keep
 *			the aspect ratio. When height is zero, the original
 *			bitmap width will be returned.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_bitmap_width(const struct ui_bitmap *bitmap,
				int32_t height, int32_t *width);

/*
 * Get text width.
 *
 * @param text		Text.
 * @param height	Text height.
 * @param style		Style of the text.
 * @param width		Text width to be calculated.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_text_width(const char *text, int32_t height,
			      enum ui_char_style style, int32_t *width);

/*
 * Draw a line of text.
 *
 * @param text		Text to be drawn, which should contain only printable
 *			characters, including spaces, but excluding tabs.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordicate of the top-left corner.
 * @param height	Height of the text.
 * @param flags		Flags passed to draw_bitmap() in libpayload.
 * @param style		Style of the text.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_text(const char *text, int32_t x, int32_t y,
			 int32_t height, uint32_t flags,
			 enum ui_char_style style, int reverse);

/*
 * Draw a box with rounded corners.
 *
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param width		Width of the box.
 * @param height	Height of the box.
 * @param rgb		Color of the box.
 * @param thickness	Thickness of the border of the box.
 * @param radius	Radius of the rounded corners.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_rounded_box(int32_t x, int32_t y,
				int32_t width, int32_t height,
				const struct rgb_color *rgb,
				uint32_t thickness, uint32_t radius,
				int reverse);

/*
 * Draw a solid box.
 *
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param width		Width of the box.
 * @param height	Height of the box.
 * @param rgb		Color of the box.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_box(int32_t x, int32_t y,
			int32_t width, int32_t height,
			const struct rgb_color *rgb,
			int reverse);

/******************************************************************************/
/* layout.c */

/* UI state for display. */
struct ui_state {
	enum vb2_screen screen;
	const struct ui_locale *locale;
	uint32_t selected_item;
	uint32_t disabled_item_mask;
};

/* Icon type. */
enum ui_icon_type {
	UI_ICON_TYPE_NONE = 0,
	UI_ICON_TYPE_INFO,
	UI_ICON_TYPE_STEP,
};

/* List of image files */
struct ui_files {
	const char *const *files;
	size_t count;
};

struct ui_screen_info {
	/* Screen id */
	enum vb2_screen id;
	/* Icon type */
	enum ui_icon_type icon;
	/* Step icons */
	int step;
	int num_steps;
	/* File for screen title. Required if (draw == NULL). */
	const char *title;
	/* Files for screen descriptions */
	struct ui_files desc;
	/*
	 * Files for menu items on the screen, excluding language selection and
	 * advanced options.
	 */
	struct ui_files menu;
	/*
	 * Custom drawing function. When it is NULL, the default drawing
	 * function ui_draw_default() will be called instead.
	 */
	vb2_error_t (*draw)(const struct ui_screen_info *screen,
			    const struct ui_state *state,
			    const struct ui_state *prev_state);
	/* Fallback message */
	const char *mesg;
};

/*
 * Default drawing function.
 *
 * @param screen	Screen information such as title and descriptions.
 * @param state		UI state.
 * @param prev_state	Previous UI state.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_default(const struct ui_screen_info *screen,
			    const struct ui_state *state,
			    const struct ui_state *prev_state);

/******************************************************************************/
/* screens.c */

/*
 * Get UI descriptor of a screen.
 *
 * @param screen_id	Screen id.
 *
 * @return UI descriptor on success, NULL on error.
 */
const struct ui_screen_info *ui_get_screen_info(enum vb2_screen screen_id);

/******************************************************************************/
/* common.c */

/*
 * Display the UI state on the screen.
 *
 * When part of the screen remains unchanged, screen redrawing should be kept as
 * minimal as possible.
 *
 * @param state		Current UI state.
 * @param prev_state	Previous UI state, or NULL if previous menu drawing was
 *			unsuccessful or there's no previous state.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_display_screen(struct ui_state *state,
			      const struct ui_state *prev_state);

#endif /* __VBOOT_UI_H__ */