// SPDX-License-Identifier: GPL-2.0
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
 */

#include <cbfs.h>
#include <libpayload.h>
#include <string.h>
#include <vb2_api.h>

#include "drivers/flash/cbfs.h"
#include "vboot/ui.h"

#define UI_LOCALE_CODE_MAX_LEN 8
#define UI_CBFS_FILENAME_MAX_LEN 256
#define UI_BITMAP_FILENAME_MAX_LEN 32

static struct cbfs_media *get_ro_cbfs(void)
{
	static struct cbfs_media *cached_ro_cbfs;
	if (cached_ro_cbfs == NULL)
		cached_ro_cbfs = cbfs_ro_media();

	return cached_ro_cbfs;
}

struct locale_data {
	/* Number of supported languages and codes: en, ja, ... */
	uint32_t count;
	struct ui_locale locales[256];
};

static const struct locale_data *get_locale_data(void)
{
	static int cache_initialized = 0;
	static struct locale_data cached_locales;
	struct cbfs_media *ro_cbfs;
	char *locales, *loc;
	size_t size;

	if (cache_initialized)
		return &cached_locales;

	ro_cbfs = get_ro_cbfs();
	if (!ro_cbfs) {
		UI_ERROR("No RO CBFS found\n");
		return NULL;
	}

	cached_locales.count = 0;

	/* Load locale list from cbfs */
	locales = cbfs_get_file_content(ro_cbfs, "locales",
					CBFS_TYPE_RAW, &size);
	if (!locales || !size) {
		UI_ERROR("locale list not found\n");
		return NULL;
	}

	/* Copy the file and null-terminate it */
	locales = realloc(locales, size + 1);
	if (!locales) {
		UI_ERROR("Out of memory\n");
		free(locales);
		return NULL;
	}
	locales[size] = '\0';

	/* Parse the list */
	UI_INFO("Supported locales:");
	loc = locales;
	while (loc - locales < size &&
	       cached_locales.count < ARRAY_SIZE(cached_locales.locales)) {
		/* Each line is of format "code,right-to-left" */
		char *line;
		const char *code, *rtl;
		struct ui_locale *info;
		line = strsep(&loc, "\n");
		if (!line || !strlen(line))
			break;
		code = strsep(&line, ",");
		if (!code || !strlen(code)) {
			UI_WARN("Unable to parse code from line: %s\n", line);
			continue;
		} else if (strlen(code) > UI_LOCALE_CODE_MAX_LEN) {
			UI_WARN("Locale code %s longer than %d, skipping\n",
				code, UI_LOCALE_CODE_MAX_LEN);
			continue;
		}
		rtl = strsep(&line, ",");
		if (!rtl || !strlen(rtl)) {
			UI_WARN("Unable to parse rtl from line: %s\n", line);
			continue;
		}
		printf(" %s", code);
		info = &cached_locales.locales[cached_locales.count];
		info->code = code;
		if (!strcmp(rtl, "1")) {
			info->rtl = 1;
			printf("(rtl)");
		} else {
			info->rtl = 0;
		}
		cached_locales.count++;
	}

	printf(" (%d locales)\n", cached_locales.count);

	if (cached_locales.count == 0) {
		UI_ERROR("No locale found\n");
		free(locales);
		return NULL;
	}

	cache_initialized = 1;
	return &cached_locales;
}

vb2_error_t ui_get_locale_info(uint32_t locale_id,
			       struct ui_locale const **locale)
{
	const struct locale_data *locale_data = get_locale_data();

	if (!locale_data)
		return VB2_ERROR_UI_INVALID_ARCHIVE;

	if (locale_id >= locale_data->count) {
		UI_ERROR("Unsupported locale %u\n", locale_id);
		return VB2_ERROR_UI_INVALID_LOCALE;
	}

	*locale = &locale_data->locales[locale_id];
	return VB2_SUCCESS;
}

static vb2_error_t load_archive(const char *name, struct directory **dest)
{
	struct cbfs_media *ro_cbfs;
	struct directory *dir;
	struct dentry *entry;
	size_t size;
	int i;

	UI_INFO("Loading %s\n", name);
	*dest = NULL;

	ro_cbfs = get_ro_cbfs();
	if (!ro_cbfs) {
		UI_ERROR("No RO CBFS found\n");
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	/* Load archive from cbfs */
	dir = cbfs_get_file_content(ro_cbfs, name, CBFS_TYPE_RAW, &size);
	if (!dir || !size) {
		UI_ERROR("Failed to load %s (dir: %p, size: %zu)\n",
			 name, dir, size);
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	/* Convert endianness of archive header */
	dir->count = le32toh(dir->count);
	dir->size = le32toh(dir->size);

	/* Validate the total size */
	if (dir->size != size) {
		UI_ERROR("Archive size does not match\n");
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	/* Validate magic field */
	if (memcmp(dir->magic, CBAR_MAGIC, sizeof(CBAR_MAGIC))) {
		UI_ERROR("Invalid archive magic\n");
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	/* Validate count field */
	if (get_first_offset(dir) > dir->size) {
		UI_ERROR("Invalid count\n");
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	/* Convert endianness of file headers */
	entry = get_first_dentry(dir);
	for (i = 0; i < dir->count; i++) {
		entry[i].offset = le32toh(entry[i].offset);
		entry[i].size = le32toh(entry[i].size);
	}

	*dest = dir;

	return VB2_SUCCESS;
}

/* Load generic (locale-independent) graphics. */
static vb2_error_t get_graphic_archive(struct directory **dest) {
	static struct directory *cache;
	if (!cache)
		VB2_TRY(load_archive("vbgfx.bin", &cache));

	*dest = cache;
	return VB2_SUCCESS;
}

/* Load locale-dependent graphics. */
static vb2_error_t get_localized_graphic_archive(const char *locale_code,
						 struct directory **dest) {
	static struct directory *cache;
	static char cached_code[UI_LOCALE_CODE_MAX_LEN + 1];
	char name[UI_CBFS_FILENAME_MAX_LEN + 1];

	if (cache) {
		if (!strncmp(cached_code, locale_code, sizeof(cached_code))) {
			*dest = cache;
			return VB2_SUCCESS;
		}
		/* No need to keep more than one locale graphics at a time */
		free(cache);
		cache = NULL;
	}

	snprintf(name, sizeof(name), "locale_%s.bin", locale_code);
	VB2_TRY(load_archive(name, &cache));
	strncpy(cached_code, locale_code, sizeof(cached_code) - 1);
	*dest = cache;

	return VB2_SUCCESS;
}

/* Load font graphics. */
static vb2_error_t get_font_archive(struct directory **dest) {
	static struct directory *cache;
	if (!cache)
		VB2_TRY(load_archive("font.bin", &cache));

	*dest = cache;
	return VB2_SUCCESS;
}

static vb2_error_t find_bitmap_in_archive(const struct directory *dir,
					  const char *name,
					  struct ui_bitmap *bitmap)
{
	struct dentry *entry;
	uintptr_t start;
	int i;

	/* Calculate start of the file content section */
	start = get_first_offset(dir);
	entry = get_first_dentry(dir);
	for (i = 0; i < dir->count; i++) {
		if (strncmp(entry[i].name, name, NAME_LENGTH))
			continue;
		/* Validate offset & size */
		if (entry[i].offset < start ||
		    entry[i].offset + entry[i].size > dir->size ||
		    entry[i].offset > dir->size ||
		    entry[i].size > dir->size) {
			UI_ERROR("Invalid offset or size for '%s'\n", name);
			return VB2_ERROR_UI_INVALID_ARCHIVE;
		}

		bitmap->data = (uint8_t *)dir + entry[i].offset;
		bitmap->size = entry[i].size;
		return VB2_SUCCESS;
	}

	UI_ERROR("File '%s' not found\n",  name);
	return VB2_ERROR_UI_MISSING_IMAGE;
}

vb2_error_t ui_get_bitmap(const char *image_name, struct ui_bitmap *bitmap)
{
	struct directory *dir;
	VB2_TRY(get_graphic_archive(&dir));
	return find_bitmap_in_archive(dir, image_name, bitmap);
}

vb2_error_t ui_get_localized_bitmap(const char *image_name,
				    const char *locale_code,
				    struct ui_bitmap *bitmap)
{
	struct directory *dir;
	VB2_TRY(get_localized_graphic_archive(locale_code, &dir));
	return find_bitmap_in_archive(dir, image_name, bitmap);
}

vb2_error_t ui_get_char_bitmap(const char c, enum ui_char_style style,
			       struct ui_bitmap *bitmap)
{
	static char image_name[32];
	const char pattern_default[] = "idx%03d_%02x.bmp";
	const char pattern_dark[] = "idx%03d_%02x-dark.bmp";
	const char *pattern;
	struct directory *dir;

	VB2_TRY(get_font_archive(&dir));

	/* Compose file name */
	pattern = (style == UI_CHAR_STYLE_DARK) ?
		pattern_dark : pattern_default;
	snprintf(image_name, sizeof(image_name), pattern, c, c);
	return find_bitmap_in_archive(dir, image_name, bitmap);
}

vb2_error_t ui_get_step_icon_bitmap(int step, int focused,
				    struct ui_bitmap *bitmap)
{
	char filename[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const char *pattern = focused ? "ic_%d-done.bmp" : "ic_%d.bmp";
	snprintf(filename, sizeof(filename), pattern, step);
	return ui_get_bitmap(filename, bitmap);
}

vb2_error_t ui_get_menu_item_bitmap(const char *image_name,
				    const char *locale_code,
				    int focused, struct ui_bitmap *bitmap)
{
	int used;
	char file[UI_BITMAP_FILENAME_MAX_LEN + 1];
	const char *file_ext;
	const char *suffix = "_focus";
	const size_t image_name_len = strlen(image_name);

	if (image_name_len + strlen(suffix) >= sizeof(file)) {
		UI_ERROR("Image name %s too long\n", image_name);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	file_ext = strrchr(image_name, '.');
	if (file_ext)
		used = file_ext - image_name;
	else
		used = image_name_len;
	strncpy(file, image_name, used);

	if (focused)
		used += snprintf(file + used, sizeof(file) - used, suffix);
	snprintf(file + used, sizeof(file) - used, file_ext);

	return ui_get_localized_bitmap(file, locale_code, bitmap);
}