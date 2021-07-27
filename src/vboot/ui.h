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

/* Maximum lengths */
#define UI_LOCALE_CODE_MAX_LEN 8
#define UI_CBFS_FILENAME_MAX_LEN 256
#define UI_BITMAP_FILENAME_MAX_LEN 32

/*
 * This is the base used to specify the size and the coordinate of the image.
 * For example, height = 40 means 4.0% of the canvas height.
 */
#define UI_SCALE				1000

/* Margins for all screens. Nothing should be drawn within the margin. */
#define UI_MARGIN_TOP				30
#define UI_MARGIN_BOTTOM			50
#define UI_MARGIN_H				50

/* For language dropdown header */
#define UI_LANG_BOX_HEIGHT			40
#define UI_LANG_ICON_MARGIN_H			15
#define UI_LANG_ICON_GLOBE_SIZE			20
#define UI_LANG_TEXT_WIDTH			240
#define UI_LANG_TEXT_HEIGHT			24
#define UI_LANG_ICON_ARROW_SIZE			24
#define UI_LANG_BORDER_THICKNESS		3
#define UI_LANG_BORDER_RADIUS			8
#define UI_LANG_MARGIN_BOTTOM			110

/* For language dropdown menu content */
#define UI_LANG_MENU_MARGIN_TOP			15
#define UI_LANG_MENU_BOX_HEIGHT			48
#define UI_LANG_MENU_TEXT_HEIGHT		26
#define UI_LANG_MENU_BORDER_THICKNESS		2
#define UI_LANG_MENU_SCROLLBAR_MARGIN_RIGHT	2
#define UI_LANG_MENU_SCROLLBAR_WIDTH		10
#define UI_LANG_MENU_SCROLLBAR_CORNER_RADIUS	2

/* For screen icon */
#define UI_ICON_HEIGHT				45
#define UI_ICON_MARGIN_BOTTOM			58

/* For step icons */
#define UI_STEP_ICON_HEIGHT			28
#define UI_STEP_ICON_MARGIN_H			7
#define UI_STEP_ICON_SEPARATOR_WIDTH		60

/* For title and descriptions */
#define UI_TITLE_TEXT_HEIGHT			42
#define UI_TITLE_MARGIN_BOTTOM			30
#define UI_DESC_TEXT_HEIGHT			24
#define UI_DESC_TEXT_LINE_SPACING		12
#define UI_DESC_MARGIN_BOTTOM			44

/* For custom screens */
#define UI_REC_QR_SIZE				228
#define UI_REC_QR_MARGIN_H			24

/* For primary buttons */
#define UI_BUTTON_HEIGHT			40
#define UI_BUTTON_TEXT_HEIGHT			20
#define UI_BUTTON_TEXT_PADDING_H		40
#define UI_BUTTON_BORDER_THICKNESS		2
#define UI_BUTTON_FOCUS_RING_THICKNESS		3
#define UI_BUTTON_BORDER_RADIUS			8
#define UI_BUTTON_MARGIN_V			10
#define UI_BUTTON_HELP_TEXT_MARGIN_L		30

/* For secondary (link) buttons */
#define UI_LINK_TEXT_PADDING_LEFT		16
#define UI_LINK_ICON_SIZE			24
#define UI_LINK_ICON_MARGIN_R			20
#define UI_LINK_ARROW_SIZE			20
#define UI_LINK_ARROW_MARGIN_H			15
#define UI_LINK_BORDER_THICKNESS		3

/* For footer */
#define UI_FOOTER_MARGIN_TOP			30
#define UI_FOOTER_HEIGHT			128
#define UI_FOOTER_TEXT_HEIGHT			20
#define UI_FOOTER_COL1_MARGIN_RIGHT		20
#define UI_FOOTER_COL2_LINE_SPACING		4
#define UI_FOOTER_COL2_MARGIN_RIGHT		40
#define UI_FOOTER_COL3_MARGIN_LEFT		40
#define UI_FOOTER_COL3_SPACING_MIN		5
#define UI_FOOTER_COL3_PARA_SPACING		48
#define UI_FOOTER_COL3_ICON_HEIGHT		30
#define UI_FOOTER_COL3_ICON_SPACING		5

/* For error box */
#define UI_ERROR_BOX_RADIUS                     12
#define UI_ERROR_BOX_HEIGHT                     280
#define UI_ERROR_BOX_WIDTH                      500
#define UI_ERROR_BOX_PADDING_H                  30
#define UI_ERROR_BOX_PADDING_V                  30
#define UI_ERROR_BOX_SECTION_SPACING            20
#define UI_ERROR_BOX_ICON_HEIGHT                40

/*
 * UI_BOX_* constants define a large textbox taking up the width of the screen.
 * They are used for
 * (1) a fallback message in the case of a broken screen, and
 * (2) a main UI element for viewing a large body of text.
 */
#define UI_BOX_TEXT_HEIGHT			20
#define UI_BOX_TEXT_LINE_SPACING		2
#define UI_BOX_MARGIN_V				275
#define UI_BOX_PADDING_H			17
#define UI_BOX_PADDING_V			15
#define UI_BOX_BORDER_THICKNESS			2
#define UI_BOX_BORDER_RADIUS			6

/* For fallback colored stripes */
#define UI_FALLBACK_STRIPE_HEIGHT		10

/* Indicate width or height is automatically set based on the other value */
#define UI_SIZE_AUTO				0
/*
 * Minimum size that is guaranteed to show up as at least 1 pixel on the screen,
 * provided that the canvas resolution is at least 500 (UI_SCALE / 2). Pixels
 * may be dropped on devices with screen resolution 640x480.
 */
#define UI_SIZE_MIN				2

static const struct rgb_color ui_color_bg		= { 0x20, 0x21, 0x24 };
static const struct rgb_color ui_color_fg		= { 0xe8, 0xea, 0xed };
static const struct rgb_color ui_color_footer_fg	= { 0x9a, 0xa0, 0xa6 };
static const struct rgb_color ui_color_lang_header_bg	= { 0x16, 0x17, 0x19 };
static const struct rgb_color ui_color_lang_header_border
	= { 0x52, 0x68, 0x8a };
static const struct rgb_color ui_color_lang_menu_bg	= { 0x2d, 0x2e, 0x30 };
static const struct rgb_color ui_color_lang_menu_border	= { 0x49, 0x57, 0x70 };
static const struct rgb_color ui_color_lang_scrollbar	= { 0x6c, 0x6d, 0x6e };
static const struct rgb_color ui_color_button		= { 0x8a, 0xb4, 0xf8 };
static const struct rgb_color ui_color_button_disabled_bg
	= { 0x3c, 0x40, 0x43 };
static const struct rgb_color ui_color_button_disabled_fg
	= { 0x9a, 0xa0, 0xa6 };
static const struct rgb_color ui_color_button_border	= { 0x4c, 0x4d, 0x4f };
static const struct rgb_color ui_color_button_focus_ring
	= { 0x4a, 0x5b, 0x78 };
static const struct rgb_color ui_color_button_help_fg	= { 0xf2, 0x8b, 0x82 };
static const struct rgb_color ui_color_link_bg		= { 0x2a, 0x2f, 0x39 };
static const struct rgb_color ui_color_link_border	= { 0x4a, 0x5b, 0x78 };
static const struct rgb_color ui_color_border		= { 0x3f, 0x40, 0x42 };
static const struct rgb_color ui_color_error_box	= { 0x20, 0x21, 0x24 };
static const struct rgb_color ui_color_black            = { 0x00, 0x00, 0x00 };

struct ui_bitmap {
	char name[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const void *data;
	size_t size;
};

struct ui_locale {
	const char *code;	/* Language code */
	int rtl;		/* Whether locale is right-to-left */
};

/* Forward declarations. */
struct ui_screen_info;
struct ui_log_info;

/* UI state for display. */
struct ui_state {
	const struct ui_screen_info *screen;
	const struct ui_locale *locale;
	uint32_t selected_item;
	uint32_t disabled_item_mask;
	uint32_t hidden_item_mask;
	int timer_disabled;
	const struct ui_log_info *log;
	int32_t current_page;
	enum vb2_ui_error error_code;
};

/* For displaying error messages. */
struct ui_error_item {
	/* Error message body */
	const char *body;
};

/* Icon type. */
enum ui_icon_type {
	/* No reserved space for any icon. */
	UI_ICON_TYPE_NONE = 0,
	UI_ICON_TYPE_INFO,
	UI_ICON_TYPE_ERROR,
	UI_ICON_TYPE_DEV_MODE,
	UI_ICON_TYPE_RESTART,
	UI_ICON_TYPE_STEP,
};

/* List of description files. */
struct ui_desc {
	size_t count;
	const char *const *files;
};

/* Menu item type. */
enum ui_menu_item_type {
	/* Primary button. */
	UI_MENU_ITEM_TYPE_PRIMARY = 0,
	/* Secondary button. */
	UI_MENU_ITEM_TYPE_SECONDARY,
	/* Language selection. */
	UI_MENU_ITEM_TYPE_LANGUAGE,
};

enum ui_menu_item_flag {
	/* No arrow; valid for UI_MENU_ITEM_TYPE_SECONDARY only. */
	UI_MENU_ITEM_FLAG_NO_ARROW		= 1 << 0,
	/* Button may disappear/reappear, so include in button width
	 * calculation; valid for UI_MENU_ITEM_TYPE_PRIMARY only. */
	UI_MENU_ITEM_FLAG_TRANSIENT		= 1 << 1,
};

/* Menu item. */
struct ui_menu_item {
	/* Pre-generated bitmap containing button text. */
	const char *file;
	/* Button text, drawn with monospace font if 'file' is not specified. */
	const char *text;
	/* If UI_MENU_ITEM_TYPE_LANGUAGE, the 'file' field will be ignored. */
	enum ui_menu_item_type type;
	/* Icon file for UI_MENU_ITEM_TYPE_SECONDARY only. */
	const char *icon_file;
	/*
	 * Bitmap file of the help text displayed next to the button.
	 * This field is only for disabled buttons of type
	 * UI_MENU_ITEM_TYPE_PRIMARY.
	 */
	const char *disabled_help_text_file;
	/* Flags are defined in enum ui_menu_item_flag. */
	uint8_t flags;
};

/* List of menu items. */
struct ui_menu {
	size_t num_items;
	/* Only the first item allowed to be UI_MENU_ITEM_TYPE_LANGUAGE. */
	const struct ui_menu_item *items;
};

struct ui_screen_info {
	/* Screen id */
	enum vb2_screen id;
	/* Icon type */
	enum ui_icon_type icon;
	/*
	 * Current step number; valid only if icon is UI_ICON_TYPE_STEP. A
	 * negative value indicates an error in the abs(step)-th step.
	 */
	int step;
	/* Total number of steps; valid only if icon is UI_ICON_TYPE_STEP */
	int num_steps;
	/* File for screen title. */
	const char *title;
	/* Files for screen descriptions. */
	struct ui_desc desc;
	/* Menu items. */
	struct ui_menu menu;
	/* Absence of footer */
	int no_footer;
	/*
	 * Custom drawing function. When it is NULL, the default drawing
	 * function ui_draw_default() will be called instead.
	 */
	vb2_error_t (*draw)(const struct ui_state *state,
			    const struct ui_state *prev_state);
	/* Custom description drawing function */
	vb2_error_t (*draw_desc)(const struct ui_state *state,
				 const struct ui_state *prev_state,
				 int32_t *y);
	/* Fallback message */
	const char *mesg;
};

/* Log string and its pages information. */
struct ui_log_info {
	/* Full log content. */
	const char *str;
	/* Total number of pages. */
	uint32_t page_count;
	/*
	 * Array of (page_count + 1) pointers. For i < page_count, page_start[i]
	 * is the start position of the i-th page. page_start[page_count] is the
	 * position of the '\0' character at the end of the log string.
	 */
	const char **page_start;
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
 * Get the number of supported locales.
 *
 * Returns the number of locales available in CBFS.
 *
 * @returns Number of locales.  0 if none or on error.
 */
uint32_t ui_get_locale_count(void);

/*
 * Get bitmap.
 *
 * @param image_name	Image file name.
 * @param locale_code	Language code of current locale, or NULL for
 *			locale-independent image.
 * @param focused	1 for focused and 0 for non-focused.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_bitmap(const char *image_name, const char *locale_code,
			  int focused, struct ui_bitmap *bitmap);

/*
 * Get bitmap of language name.
 *
 * @param locale_code	Language code of locale.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_language_name_bitmap(const char *locale_code,
					struct ui_bitmap *bitmap);

/*
 * Get old bitmap of language name. This is needed to work with RO version with
 * old language name bitmaps. See b/176942478.
 *
 * @param locale_code	Language code of locale.
 * @param for_header	1 for dropdown header and 0 for dropdown menu content.
 * @param focused	1 for focused and 0 for non-focused.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_language_name_old_bitmap(const char *locale_code,
					    int for_header, int focused,
					    struct ui_bitmap *bitmap);

/*
 * Get character bitmap.
 *
 * @param c		Character.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_char_bitmap(const char c, struct ui_bitmap *bitmap);

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

/******************************************************************************/
/* draw.c */

/*
 * Draw bitmap.
 *
 * @param bitmap	Bitmap to draw.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
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
 * Draw bitmap with color mappings.
 *
 * @param bitmap	Bitmap to draw.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param width		Width of the image.
 * @param height	Height of the image.
 * @param bg_color	Background color, which is passed to set_color_map() in
 *			libpayload.
 * @param fg_color	Foreground color passed to set_color_map().
 * @param flags		Flags passed to draw_bitmap() in libpayload.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_mapped_bitmap(const struct ui_bitmap *bitmap,
				  int32_t x, int32_t y,
				  int32_t width, int32_t height,
				  const struct rgb_color *bg_color,
				  const struct rgb_color *fg_color,
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
 * Get the number of lines in a bitmap containing text.
 *
 * @param bitmap	Pointer to the bitmap struct.
 *
 * @return A strictly positive number of lines.  If bitmap does not contain
 *         text or is invalid, a value of 1 is returned.
 */
uint32_t ui_get_bitmap_num_lines(const struct ui_bitmap *bitmap);

/*
 * Get text width.
 *
 * @param text		Text.
 * @param height	Text height.
 * @param width		Text width to be calculated.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_text_width(const char *text, int32_t height, int32_t *width);

/*
 * Draw a line of text.
 *
 * @param text		Text to be drawn, which should contain only printable
 *			characters, including spaces, but excluding tabs.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param height	Height of the text.
 * @param bg_color	Background color, which is passed to set_color_map() in
 *			libpayload.
 * @param fg_color	Foreground color passed to set_color_map().
 * @param flags		Flags passed to draw_bitmap() in libpayload.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_text(const char *text,
			 int32_t x, int32_t y, int32_t height,
			 const struct rgb_color *bg_color,
			 const struct rgb_color *fg_color,
			 uint32_t flags, int reverse);

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

/*
 * Draw a horizontal line segment.
 *
 * @param x		x-coordinate of the left endpoint.
 * @param y		y-coordinate of the line.
 * @param length	Length of the line.
 * @param thickness	Thickness of the line.
 * @param rgb		Color of the line.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_h_line(int32_t x, int32_t y,
			   int32_t length, int32_t thickness,
			   const struct rgb_color *rgb);

/******************************************************************************/
/* layout.c */

/*
 * Draw language dropdown header.
 *
 * @param locale	Locale of which name to be drawn.
 * @param state		UI state.
 * @param focused	1 for focused and 0 for non-focused.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_language_header(const struct ui_locale *locale,
				    const struct ui_state *state, int focused);

/*
 * Get button width, based on the longest text of all the visible buttons.
 *
 * Menu items specified in hidden_item_mask are ignored.
 *
 * @param menu			Menu items.
 * @param state			UI state.
 * @param button_width		Button width to be calculated.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_get_button_width(const struct ui_menu *menu,
				const struct ui_state *state,
				int32_t *button_width);

/*
 * Draw a button with image.
 *
 * Menu item should specify either .file or .text for button text.
 *
 * @param item		Menu item.
 * @param locale_code	Language code of current locale.
 * @param x		x-coordinate of the top-left corner.
 * @param y		y-coordinate of the top-left corner.
 * @param width		Width of the button.
 * @param height	Height of the button.
 * @param reverse	Whether to reverse the x-coordinate relative to the
 *			canvas.
 * @param focused	1 for focused and 0 for non-focused.
 * @param disabled	1 for disabled style and 0 for normal style.
 * @param clear_help	1 to request for clearing the disabled help text area.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_button(const struct ui_menu_item *item,
			   const char *locale_code,
			   int32_t x, int32_t y,
			   int32_t width, int32_t height,
			   int reverse, int focused, int disabled,
			   int clear_help);

/*
 * Draw screen descriptions.
 *
 * @param desc		List of description files.
 * @param state		UI state.
 * @param y		Starting y-coordinate of the descriptions. On return,
 *			the value will be the ending coordinate, excluding the
 *			margin below the descriptions.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_desc(const struct ui_desc *desc,
			 const struct ui_state *state,
			 int32_t *y);

/*
 * Draw a rounded textbox with multi-line text.
 *
 * The printed text is guaranteed to fit on the screen by adjusting the height
 * of the box, and by resizing individual lines horizontally to fit. The box
 * will take up the full width of the canvas.
 *
 * @param str		Texts to be printed, which may contain line breaks.
 * @param y		Starting y-coordinate of the box. On return, the value
 *			will be the ending coordinate, excluding the margin
 *			below the box.
 * @param min_lines	Minimum number of lines the textbox should contain. Pad
 *			with empty lines if str does not reach this limit.
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_textbox(const char *str, int32_t *y, int32_t min_lines);

/*
 * Get the dimensions of the log textbox.
 *
 * The log textbox can fit lines_per_page * chars_per_line characters.
 *
 * @param lines_per_page	On return, the value will be maximum number of
 *				lines per page.
 * @param chars_per_line	On return, the value will be maximum number of
 *				characters per line.
 *
 * @return VB2_SUCCESS on success, no-zero on error.
 */
vb2_error_t ui_get_log_textbox_dimensions(uint32_t *lines_per_page,
					  uint32_t *chars_per_line);

/*
 * Draw a textbox for displaying the log screen.
 *
 * @param str		The full log string, which may contain line breaks.
 * @param y		Starting y-coordinate of the box. On return, the value
 *			will be the ending coordinate, excluding the margin
 *			below the box.
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_log_textbox(const char *str, int32_t *y);

/*
 * Draw primary and secondary buttons; ignore the language dropdown header.
 *
 * @param menu			Menu items.
 * @param state			UI state.
 * @param prev_state		Previous UI state.
 * @param y			Starting y-coordinate of the descriptions.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_menu_items(const struct ui_menu *menu,
			       const struct ui_state *state,
			       const struct ui_state *prev_state,
			       int32_t y);

/*
 * Default drawing function.
 *
 * @param state		UI state.
 * @param prev_state	Previous UI state.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_default(const struct ui_state *state,
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
/* log.c */

/*
 * Initialize log info struct with a string.
 *
 * @param log			Log info struct to be initialized.
 * @param str			The full log string.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_log_init(struct ui_log_info *log, const char *str);

/*
 * Retrieve the content of specified page.
 * The caller owns the string and should call free() when finished with it.
 *
 * @param log		Log info.
 * @param page		Page number.
 *
 * @return The pointer to the page content, NULL on error.
 */
char *ui_log_get_page_content(const struct ui_log_info *log, uint32_t page);

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
