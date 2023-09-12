// SPDX-License-Identifier: GPL-2.0

#include <cbfs.h>
#include <libpayload.h>
#include <string.h>
#include <vb2_api.h>

#include "vboot/context.h"
#include "vboot/ui.h"

struct locale_data {
	/* Number of supported languages and codes: en, ja, ... */
	uint32_t count;
	struct ui_locale locales[256];
};

static const struct locale_data *get_locale_data(void)
{
	static int cache_initialized = 0;
	static struct locale_data cached_locales;
	char *locales, *locales_raw, *loc;
	size_t size;

	if (cache_initialized)
		return &cached_locales;

	cached_locales.count = 0;

	/* Load locale list from cbfs */
	locales_raw = cbfs_ro_map("locales", &size);
	if (!locales_raw || !size) {
		UI_ERROR("locale list not found\n");
		return NULL;
	}

	/* Copy the file and null-terminate it */
	locales = realloc(locales_raw, size + 1);
	if (!locales) {
		UI_ERROR("Out of memory\n");
		free(locales_raw);
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
		info->id = cached_locales.count;
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

uint32_t ui_get_locale_count(void)
{
	const struct locale_data *locale_data = get_locale_data();

	if (!locale_data)
		return 0;

	return locale_data->count;
}

static vb2_error_t load_archive(const char *name,
				struct directory **dest,
				int from_ro)
{
	struct directory *dir;
	struct dentry *entry;
	size_t size = 0;
	int i;

	UI_INFO("Loading %s\n", name);
	*dest = NULL;

	if (from_ro)
		dir = cbfs_ro_map(name, &size);
	else
		dir = cbfs_map(name, &size);

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
static vb2_error_t get_graphic_archive(struct directory **dest)
{
	static struct directory *ro_cache;
	if (!ro_cache)
		VB2_TRY(load_archive("vbgfx.bin", &ro_cache, 1));

	*dest = ro_cache;
	return VB2_SUCCESS;
}

/*
 * Load locale-dependent graphics.
 *
 * On success, *ro_dest is guaranteed to be non-null. *rw_dest will be null
 * when no RW override is found.
 */
static vb2_error_t get_localized_graphic_archive(const char *locale_code,
						 struct directory **ro_dest,
						 struct directory **rw_dest)
{
	static struct directory *ro_cache;
	static struct directory *rw_cache;

	static char cached_code[UI_LOCALE_CODE_MAX_LEN + 1];
	char name[UI_CBFS_FILENAME_MAX_LEN + 1];

	if (ro_cache) {
		if (!strncmp(cached_code, locale_code, sizeof(cached_code))) {
			*ro_dest = ro_cache;
			*rw_dest = rw_cache;
			return VB2_SUCCESS;
		}
		/* No need to keep more than one locale graphics at a time */
		free(ro_cache);
		free(rw_cache);
		ro_cache = NULL;
	}

	snprintf(name, sizeof(name), "locale_%s.bin", locale_code);
	VB2_TRY(load_archive(name, &ro_cache, 1));

	/* Try to read from RW region while we are not in recovery mode */
	rw_cache = NULL;
	if (!(vboot_get_context()->flags & VB2_CONTEXT_RECOVERY_MODE)) {
		snprintf(name, sizeof(name), "rw_locale_%s.bin", locale_code);
		/*
		 * Silently ignore errors because rw_locale_*.bin may not exist
		 * in both firmware slots.
		 */
		load_archive(name, &rw_cache, 0);
	}

	strncpy(cached_code, locale_code, sizeof(cached_code) - 1);
	cached_code[sizeof(cached_code) - 1] = '\0';
	*ro_dest = ro_cache;
	*rw_dest = rw_cache;
	return VB2_SUCCESS;
}

/* Load font graphics. */
static vb2_error_t get_font_archive(struct directory **dest)
{
	static struct directory *ro_cache;
	if (!ro_cache)
		VB2_TRY(load_archive("font.bin", &ro_cache, 1));

	*dest = ro_cache;
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

		bitmap->name[UI_BITMAP_FILENAME_MAX_LEN] = '\0';
		strncpy(bitmap->name, name, UI_BITMAP_FILENAME_MAX_LEN);
		bitmap->data = (uint8_t *)dir + entry[i].offset;
		bitmap->size = entry[i].size;
		return VB2_SUCCESS;
	}

	UI_ERROR("File '%s' not found\n",  name);
	return VB2_ERROR_UI_MISSING_IMAGE;
}

vb2_error_t ui_load_bitmap(enum ui_archive_type type, const char *file,
			   const char *locale_code, struct ui_bitmap *bitmap)
{
	struct directory *ro_dir;
	struct directory *rw_dir;

	switch (type) {
	case UI_ARCHIVE_GENERIC:
		VB2_TRY(get_graphic_archive(&ro_dir));
		return find_bitmap_in_archive(ro_dir, file, bitmap);
	case UI_ARCHIVE_LOCALIZED:
		VB2_TRY(get_localized_graphic_archive(locale_code,
						      &ro_dir, &rw_dir));
		if (rw_dir) {
			UI_INFO("Searching RW override for %s\n", file);
			if (find_bitmap_in_archive(rw_dir, file, bitmap) ==
			    VB2_SUCCESS)
				return VB2_SUCCESS;
		}
		return find_bitmap_in_archive(ro_dir, file, bitmap);
	case UI_ARCHIVE_FONT:
		VB2_TRY(get_font_archive(&ro_dir));
		return find_bitmap_in_archive(ro_dir, file, bitmap);
	default:
		UI_WARN("Unknown archive type %d\n", type);
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}
}
