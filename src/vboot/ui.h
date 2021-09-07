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
#define UI_ERROR_BOX_RADIUS			12
#define UI_ERROR_BOX_HEIGHT			280
#define UI_ERROR_BOX_WIDTH			500
#define UI_ERROR_BOX_PADDING			30
#define UI_ERROR_BOX_SECTION_SPACING		20
#define UI_ERROR_BOX_ICON_HEIGHT		40
#define UI_ERROR_BOX_TEXT_HEIGHT		24
#define UI_ERROR_BOX_TEXT_LINE_SPACING		12

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

/* Time-related constants */
#define UI_KEY_DELAY_MS 20  /* Delay between key scans in UI loops */
#define DEV_DELAY_SHORT_MS (2 * MSECS_PER_SEC)		/* 2 seconds */
#define DEV_DELAY_NORMAL_MS (30 * MSECS_PER_SEC)	/* 30 seconds */
#define DEV_DELAY_BEEP1_MS (20 * MSECS_PER_SEC)		/* 20 seconds */
#define DEV_DELAY_BEEP2_MS (20 * MSECS_PER_SEC + 500)	/* 20.5 seconds */

/* Key code for CTRL + letter */
#define UI_KEY_CTRL(letter) (letter & 0x1f)
/* Key code for fn keys */
#define UI_KEY_F(num) (num + 0x108)

/* Pre-defined key for UI action functions */
#define UI_KEY_INTERNET_RECOVERY	UI_KEY_CTRL('R')
#define UI_KEY_REC_TO_DEV		UI_KEY_CTRL('D')
/* S for secure mode (normal mode) */
#define UI_KEY_DEV_TO_NORM		UI_KEY_CTRL('S')
/* D for internal Disk */
#define UI_KEY_DEV_BOOT_INTERNAL	UI_KEY_CTRL('D')
/* U for USB disk */
#define UI_KEY_DEV_BOOT_EXTERNAL	UI_KEY_CTRL('U')
/* L for aLtfw (formerly Legacy) */
#define UI_KEY_DEV_BOOT_ALTFW		UI_KEY_CTRL('L')

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
static const struct rgb_color ui_color_black		= { 0x00, 0x00, 0x00 };

/*
 * Flags for additional information.
 * TODO(semenzato): consider adding flags for modifiers instead of
 * making up some of the key codes below.
 */
enum ui_key_flag {
	UI_KEY_FLAG_TRUSTED_KEYBOARD = 1 << 0,
};

/* Key codes for required non-printable-ASCII characters. */
enum ui_key_code {
	UI_KEY_ENTER = '\r',
	UI_KEY_ESC = 0x1b,
	UI_KEY_BACKSPACE = 0x8,
	UI_KEY_UP = 0x100,
	UI_KEY_DOWN = 0x101,
	UI_KEY_LEFT = 0x102,
	UI_KEY_RIGHT = 0x103,
	UI_KEY_CTRL_ENTER = 0x104,
};

/*
 * WARNING!!! Before updating the codes in enum ui_button_code, ensure that the
 * code does not overlap the values in ui_key_code unless the button action is
 * the same as key action.
 */
enum ui_button_code {
	/* Volume up/down short press match the values in 8042 driver. */
	UI_BUTTON_VOL_UP_SHORT_PRESS = 0x62,
	UI_BUTTON_VOL_DOWN_SHORT_PRESS = 0x63,
	/* Random values used below. */
	UI_BUTTON_POWER_SHORT_PRESS = 0x90,
	UI_BUTTON_VOL_UP_LONG_PRESS = 0x91,
	UI_BUTTON_VOL_DOWN_LONG_PRESS = 0x92,
	UI_BUTTON_VOL_UP_DOWN_COMBO_PRESS = 0x93,
};

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
struct ui_context;

struct ui_state {
	const struct ui_screen_info *screen;
	const struct ui_locale *locale;
	uint32_t selected_item;
	uint32_t disabled_item_mask;
	uint32_t hidden_item_mask;
	int timer_disabled;
	const struct ui_log_info *log;
	uint32_t page_count;
	uint32_t current_page;
	enum vb2_ui_error error_code;

	/* For minidiag test screens. */
	int test_finished;  /* Do not update screen if the content is done */

	struct ui_state *prev;
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
	/*
	 * Item name for printing to console, required for all menu items.
	 * When 'file' is not specified, the string will be used to draw the
	 * button text with monospace font.
	 */
	const char *name;
	/* Pre-generated bitmap containing button text. */
	const char *file;
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
	/* Target screen */
	enum vb2_screen target;
	/* Action function takes precedence over target screen if non-NULL. */
	vb2_error_t (*action)(struct ui_context *ui);
};

/* List of menu items. */
struct ui_menu {
	size_t num_items;
	/* Only the first item allowed to be UI_MENU_ITEM_TYPE_LANGUAGE. */
	const struct ui_menu_item *items;
};

/* States of power button. */
enum ui_power_button {
	UI_POWER_BUTTON_HELD_SINCE_BOOT = 0,
	UI_POWER_BUTTON_RELEASED,
	UI_POWER_BUTTON_PRESSED,  /* Must have been previously released */
};

struct ui_context {
	struct vb2_context *ctx;
	struct ui_state *state;
	uint32_t locale_id;
	uint32_t key;
	int key_trusted;

	/* For check_shutdown_request. */
	enum ui_power_button power_button;

	/* For developer mode. */
	int disable_timer;
	uint32_t start_time_ms;
	int beep_count;

	/* For manual recovery. */
	vb2_error_t recovery_rv;

	/* For to_dev transition flow. */
	int physical_presence_button_pressed;

	/* For language selection screen. */
	struct ui_menu language_menu;

	/* For bootloader selection screen. */
	struct ui_menu bootloader_menu;

	/* For error beep sound. */
	int error_beep;

	/* For displaying error messages. */
	enum vb2_ui_error error_code;

	/* Force calling ui_display for refreshing the screen. This flag
	   will be reset after done. */
	int force_display;
};

struct ui_screen_info {
	/* Screen id */
	enum vb2_screen id;
	/* Screen name for printing to console only */
	const char *name;
	/* Icon type */
	enum ui_icon_type icon;
	/*
	 * Current step number; valid only if icon is UI_ICON_TYPE_STEP. A
	 * negative value indicates an error in the abs(step)-th step.
	 */
	int step;
	/* Total number of steps; valid only if icon is UI_ICON_TYPE_STEP */
	int num_steps;
	/* File for screen title; required with ui_draw_default(). */
	const char *title;
	/* Files for screen descriptions. */
	struct ui_desc desc;
	/* Menu items. */
	struct ui_menu menu;
	/* Absence of footer */
	int no_footer;
	/*
	 * Init function runs once when changing to the screen which is not in
	 * the history stack.
	 */
	vb2_error_t (*init)(struct ui_context *ui);
	/*
	 * Re-init function runs once when changing to the screen which is
	 * already in the history stack, for example, when going back to the
	 * screen. Exactly one of init() and reinit() will be called.
	 */
	vb2_error_t (*reinit)(struct ui_context *ui);
	/* Action function runs repeatedly while on the screen. */
	vb2_error_t (*action)(struct ui_context *ui);
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
	/*
	 * Custom function for getting menu items. If non-null, field 'menu'
	 * will be ignored.
	 */
	const struct ui_menu *(*get_menu)(struct ui_context *ui);
	/*
	 * Indices of menu items;
	 * used by log_page_* functions in ui/screens.c.
	 */
	uint32_t page_up_item;
	uint32_t page_down_item;
	uint32_t back_item;
	uint32_t cancel_item;
};

/* Log string and its pages information. */
struct ui_log_info {
	/* Full log content. */
	const char *str;
	/* Maximum number of lines per page. */
	uint32_t lines_per_page;
	/* Maximum number of characters per line.*/
	uint32_t chars_per_line;
	/* Total number of pages. */
	uint32_t page_count;
	/*
	 * Array of (page_count + 1) pointers. For i < page_count, page_start[i]
	 * is the start position of the i-th page. page_start[page_count] is the
	 * position of the '\0' character at the end of the log string.
	 */
	const char **page_start;
};

/* Log info for ui_display */
extern struct ui_log_info global_ui_log_info;

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

enum ui_archive_type {
	UI_ARCHIVE_GENERIC,
	UI_ARCHIVE_LOCALIZED,
	UI_ARCHIVE_FONT,
};

/*
 * Load bitmap from archive.
 *
 * @param type		Archive type. See enum ui_archive_type.
 * @param file		Bitmap file name.
 * @param locale_code	Language code of locale, only for UI_ARCHIVE_LOCALIZED.
 * @param bitmap	Bitmap struct to be filled.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */

vb2_error_t ui_load_bitmap(enum ui_archive_type type, const char *file,
			   const char *locale_code, struct ui_bitmap *bitmap);

/******************************************************************************/
/* bitmap.c */

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
 * @param screen		Screen to display the log.
 * @param locale_code		Language code of locale.
 * @param lines_per_page	On return, the value will be maximum number of
 *				lines per page.
 * @param chars_per_line	On return, the value will be maximum number of
 *				characters per line.
 *
 * @return VB2_SUCCESS on success, no-zero on error.
 */
vb2_error_t ui_get_log_textbox_dimensions(enum vb2_screen screen,
					  const char *locale_code,
					  uint32_t *lines_per_page,
					  uint32_t *chars_per_line);

/*
 * Draw a textbox for displaying the log screen.
 *
 * @param str		The full log string, which may contain line breaks.
 * @param state		UI state.
 * @param y		Starting y-coordinate of the box. On return, the value
 *			will be the ending coordinate, excluding the margin
 *			below the box.
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_draw_log_textbox(const char *str, const struct ui_state *state,
				int32_t *y);

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

/* Expose these boot action functions for recovery_action. */
vb2_error_t ui_recovery_mode_boot_minios_action(struct ui_context *ui);

/* Expose these boot action functions for developer_action. */
vb2_error_t ui_developer_mode_boot_internal_action(struct ui_context *ui);
vb2_error_t ui_developer_mode_boot_external_action(struct ui_context *ui);
vb2_error_t ui_developer_mode_boot_altfw_action(struct ui_context *ui);

/******************************************************************************/
/* log.c */

/*
 * Initialize log info struct with a string.
 *
 * @param screen	Screen to display the log.
 * @param locale_code	Language code of locale.
 * @param str		The full log string.
 * @param log		Log info struct to be initialized.
 *
 * @return VB2_SUCCESS on success, non-zero on error.
 */
vb2_error_t ui_log_init(enum vb2_screen screen, const char *locale_code,
			const char *str, struct ui_log_info *log);

/*
 * Retrieve the content of specified page.
 *
 * The log must be have been initialized by ui_log_init(). The caller owns the
 * string and should call free() when finished with it.
 *
 * @param log		Log info.
 * @param page		Page number.
 *
 * @return The pointer to the page content, NULL on error.
 */
char *ui_log_get_page_content(const struct ui_log_info *log, uint32_t page);

/******************************************************************************/
/* display.c */

/*
 * Display UI screen.
 *
 * @param screen		Screen to display.
 * @param locale_id		Id of current locale.
 * @param selected_item		Index of the selected menu item. If the screen
 *				doesn't have a menu, this value will be ignored.
 * @param disabled_item_mask	Mask for disabled menu items. Bit (1 << idx)
 *				indicates whether item 'idx' is disabled.
 *				A disabled menu item is visible and selectable,
 *				with a different button style.
 * @param hidden_item_mask	Mask for hidden menu items. Bit (1 << idx)
 *				indicates whether item 'idx' is hidden.
 *				A hidden menu item is neither visible nor
 *				selectable.
 * @param timer_disabled	Whether timer is disabled or not. Some screen
 *				descriptions will depend on this value.
 * @param current_page		Current page number for a log screen. If the
 *				screen doesn't show logs, this value will be
 *				ignored.
 * @param error_code		Error code if an error occurred.
 * @return VB2_SUCCESS, or error code on error.
 */

vb2_error_t ui_display(enum vb2_screen screen, uint32_t locale_id,
		       uint32_t selected_item, uint32_t disabled_item_mask,
		       uint32_t hidden_item_mask, int timer_disabled,
		       uint32_t current_page, enum vb2_ui_error error_code);

/******************************************************************************/
/* input.c */

/*
 * Read the next keypress from the keyboard buffer.
 *
 * Returns the keypress, or zero if no keypress is pending or error.
 *
 * The following keys must be returned as ASCII character codes:
 *    0x08          Backspace
 *    0x09          Tab
 *    0x0D          Enter (carriage return)
 *    0x01 - 0x1A   Ctrl+A - Ctrl+Z (yes, those alias with backspace/tab/enter)
 *    0x1B          Esc (UI_KEY_ESC)
 *    0x20          Space
 *    0x30 - 0x39   '0' - '9'
 *    0x60 - 0x7A   'a' - 'z'
 *
 * Some extended keys must also be supported; see the UI_KEY_* defines above.
 *
 * Keys ('/') or key-chords (Fn+Q) not defined above may be handled in any of
 * the following ways:
 *    1. Filter (don't report anything if one of these keys is pressed).
 *    2. Report as ASCII (if a well-defined ASCII value exists for the key).
 *    3. Report as any other value in the range 0x200 - 0x2FF.
 * It is not permitted to report a key as a multi-byte code (for example,
 * sending an arrow key as the sequence of keys '\x1b', '[', '1', 'A').
 */
uint32_t ui_keyboard_read(uint32_t *flags_ptr);

int ui_is_lid_open(void);

int ui_is_power_pressed(void);

/******************************************************************************/
/* loop.c */

/*
 * The entry of main UI loop.
 *
 * @param ctx			Vboot2 context.
 * @param root_screen_id	Root screen id.
 * @param global_action		The entry of action function.
 */
vb2_error_t ui_loop(struct vb2_context *ctx, enum vb2_screen root_screen_id,
		    vb2_error_t (*global_action)(struct ui_context *ui));

/******************************************************************************/
/* menu.c */

const struct ui_menu *ui_get_menu(struct ui_context *ui);

vb2_error_t ui_menu_prev(struct ui_context *ui);

vb2_error_t ui_menu_next(struct ui_context *ui);

vb2_error_t ui_menu_select(struct ui_context *ui);

/******************************************************************************/
/* navigation.c */

/*
 * NOTE: This function never returns VB2_SUCCUSS by design. Instead,
 * VB2_REQUEST_UI_CONTINUE is returned on success for the following
 * reason.
 *
 * The screen would have been changed when this function returns.
 * Therefore, any call to this function should return immediately afterwards.
 * Otherwise, code underneath it might operate under the assumption that
 * the screen has not been changed (see b/181087237). The same rule also
 * applies to any indirect call to this function, but that would include
 * many UI functions (action, init, reinit functions).
 *
 * Instead of checking whether the screen has been changed after each
 * call, we rely on VB2_REQUEST_UI_CONTINUE to ensure that all direct
 * and indirect calls to this function will immediately return all the
 * way back to ui_loop(), where VB2_REQUEST_UI_CONTINUE is ignored to
 * stay in the loop (see CL:2714502). This solution also allows us to
 * utilize the VB2_TRY macro as much as possible.
 */
vb2_error_t ui_screen_change(struct ui_context *ui, enum vb2_screen id);

/*
 * NOTE: This function never returns VB2_SUCCUSS by design. See the
 * comments before ui_screen_change().
 */
vb2_error_t ui_screen_back(struct ui_context *ui);

#endif /* __VBOOT_UI_H__ */
