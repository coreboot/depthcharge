// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <string.h>
#include <vb2_api.h>

#include "vboot/ui.h"

vb2_error_t ui_get_bitmap(const char *image_name, const char *locale_code,
			  int focused, struct ui_bitmap *bitmap)
{
	char file[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const char *file_ext;
	const char *suffix = focused ? "_focus" : "";
	const size_t image_name_len = strlen(image_name);

	if (image_name_len + strlen(suffix) >= sizeof(file)) {
		UI_ERROR("Image name %s too long\n", image_name);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	file_ext = strrchr(image_name, '.');
	if (file_ext)
		snprintf(file, sizeof(file), "%.*s%s%s",
			 (int)(file_ext - image_name), image_name, suffix,
			 file_ext);
	else
		snprintf(file, sizeof(file), "%s%s", image_name, suffix);

	return ui_load_bitmap(
		locale_code ? UI_ARCHIVE_LOCALIZED : UI_ARCHIVE_GENERIC,
		file, locale_code, bitmap);
}

vb2_error_t ui_get_language_name_bitmap(const char *locale_code,
					struct ui_bitmap *bitmap)
{
	char file[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const char pattern[] = "language_%s.bmp";

	snprintf(file, sizeof(file), pattern, locale_code);
	return ui_get_bitmap(file, NULL, 0, bitmap);
}

vb2_error_t ui_get_char_bitmap(const char c, struct ui_bitmap *bitmap)
{
	char file[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const char pattern[] = "idx%03d_%02x.bmp";

	snprintf(file, sizeof(file), pattern, c, c);
	return ui_load_bitmap(UI_ARCHIVE_FONT, file, NULL, bitmap);
}

vb2_error_t ui_get_step_icon_bitmap(int step, int focused,
				    struct ui_bitmap *bitmap)
{
	char file[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const char *pattern = focused ? "ic_%d-done.bmp" : "ic_%d.bmp";

	snprintf(file, sizeof(file), pattern, step);
	return ui_get_bitmap(file, NULL, 0, bitmap);
}
