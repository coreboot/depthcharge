// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <stdio.h>
#include <assert.h>

#include "io.h"
#include "vboot/ui.h"

#define PATH_MAX_LEN 1000

vb2_error_t ui_get_locale_info(uint32_t locale_id,
			       struct ui_locale const **locale)
{
	static const struct ui_locale locale_en = {
		.code = "en",
		.rtl = 0,
	};
	*locale = &locale_en;
	return VB2_SUCCESS;
}

uint32_t ui_get_locale_count(void)
{
	return 1;
}

static vb2_error_t load_bitmap(const char *path, struct ui_bitmap *bitmap)
{
	size_t read_size;
	unsigned char *buffer;

	bitmap->data = NULL;
	bitmap->size = get_file_size(path);
	if (bitmap->size == 0) {
		UI_ERROR("Error getting file size: %s\n", path);
		return VB2_ERROR_UNKNOWN;
	}

	/* There will be memory leak, but we don't care */
	buffer = xmalloc(bitmap->size);
	read_size = read_file(buffer, bitmap->size, path);
	if (read_size != bitmap->size) {
		UI_ERROR("Read size %zu != file size %zu\n",
			 read_size, bitmap->size);
		return VB2_ERROR_UNKNOWN;
	}

	bitmap->data = buffer;
	return VB2_SUCCESS;
}

vb2_error_t ui_load_bitmap(enum ui_archive_type type, const char *file,
			   const char *locale_code, struct ui_bitmap *bitmap)
{
	char path[PATH_MAX_LEN + 1];

	switch (type) {
	case UI_ARCHIVE_GENERIC:
		snprintf(path, sizeof(path), "%s/%s",
			 __BMP_PATH__, file);
		return load_bitmap(path, bitmap);
	case UI_ARCHIVE_LOCALIZED:
		snprintf(path, sizeof(path), "%s/locale/ro/%s/%s",
			 __BMP_PATH__, locale_code, file);
		return load_bitmap(path, bitmap);
	case UI_ARCHIVE_FONT:
		snprintf(path, sizeof(path), "%s/glyph/%s",
			 __BMP_PATH__, file);
		return load_bitmap(path, bitmap);
	default:
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	return VB2_SUCCESS;
}
