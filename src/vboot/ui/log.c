// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
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

#include <vb2_api.h>

#include "vboot/ui.h"

vb2_error_t ui_log_set_anchors(struct ui_log_info *log,
			       const char *const anchors[], size_t num)
{
	struct ui_anchor_info *anchor_info = &log->impl.static_log.anchor_info;
	const char *pos;
	int i, j;
	size_t size;

	if (num == 0)
		return VB2_SUCCESS;

	/* Initialize anchor_info */
	anchor_info->total_count = 0;
	free(anchor_info->per_page_count);
	size = sizeof(*(anchor_info->per_page_count)) * log->impl.static_log.page_count;
	anchor_info->per_page_count = malloc(size);

	if (!anchor_info->per_page_count)
		return VB2_ERROR_UI_MEMORY_ALLOC;

	memset(anchor_info->per_page_count, 0, size);

	for (i = 0; i < num; i++) {
		j = 0;
		pos = strstr(log->impl.static_log.page_start[j], anchors[i]);

		/* Search the page number that contains current anchor */
		while (pos && j < log->impl.static_log.page_count) {
			if (pos >= log->impl.static_log.page_start[j]) {
				j++;
			} else {
				anchor_info->total_count++;
				/* If the anchor can't be found in current page,
				   then it must exist in previous page. */
				anchor_info->per_page_count[j - 1]++;
				/* Search next anchor from current page */
				pos = strstr(log->impl.static_log.page_start[j], anchors[i]);
			}
		}

		/* For the anchor in the last page */
		if (pos) {
			anchor_info->total_count++;
			anchor_info->per_page_count[j - 1]++;
		}
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_log_common_init(enum ui_screen screen, const char *locale_code,
			       struct ui_log_info *log, enum ui_log_type type)
{
	uint32_t lines_per_page, chars_per_line;

	VB2_TRY(ui_get_log_textbox_dimensions(screen, locale_code,
					      &lines_per_page,
					      &chars_per_line));

	if (lines_per_page == 0 || chars_per_line == 0) {
		UI_ERROR("Failed to initialize log_info, dimensions: %ux%u\n",
			 lines_per_page, chars_per_line);
		return VB2_ERROR_UI_LOG_INIT;
	}

	/* Clear previous log */
	if (log->type == UI_LOG_TYPE_STATIC)
		free(log->impl.static_log.page_start);
	memset(log, 0, sizeof(*log));

	/* Set common fields */
	log->type = type;
	log->lines_per_page = lines_per_page;
	log->chars_per_line = chars_per_line;

	return VB2_SUCCESS;
}

vb2_error_t ui_log_init(enum ui_screen screen, const char *locale_code,
			const char *str, struct ui_log_info *log)
{
	uint32_t chars_current_line;
	uint32_t lines;
	uint32_t pages;
	const char *ptr;

	if (str == NULL) {
		UI_ERROR("Failed to initialize log_info, str is NULL\n");
		return VB2_ERROR_UI_LOG_INIT;
	}
	VB2_TRY(ui_log_common_init(screen, locale_code, log, UI_LOG_TYPE_STATIC));

	/* TODO(b/166741235): Replace <TAB> with 8 spaces. */
	/* Count number of characters and lines. */
	lines = 0;
	ptr = str;
	while (*ptr != '\0') {
		chars_current_line = 0;
		lines++;
		while (*ptr != '\0') {
			chars_current_line++;
			if (*ptr == '\n') {
				ptr++;
				break;
			}
			/* Wrap current line, put current character into next
			   line. */
			if (chars_current_line > log->chars_per_line)
				break;
			ptr++;
		}
	}

	/* Initialize log_info entries. */
	log->impl.static_log.str = str;
	log->impl.static_log.page_count = DIV_ROUND_UP(lines, log->lines_per_page);
	log->impl.static_log.page_start = malloc((log->impl.static_log.page_count + 1) *
						 sizeof(const char *));
	if (!log->impl.static_log.page_start) {
		UI_ERROR("Failed to malloc page_start array, "
			 "page_count: %u\n", log->impl.static_log.page_count);
		return VB2_ERROR_UI_MEMORY_ALLOC;
	}

	/* Calculate page_start. */
	lines = 0;
	pages = 0;
	ptr = str;
	while (*ptr != '\0') {
		if (lines % log->lines_per_page == 0)
			log->impl.static_log.page_start[pages++] = ptr;
		chars_current_line = 0;
		lines++;
		while (*ptr != '\0') {
			chars_current_line++;
			if (*ptr == '\n') {
				ptr++;
				break;
			}
			/* Wrap current line, put current character into next
			   line. */
			if (chars_current_line > log->chars_per_line)
				break;
			ptr++;
		}
	}
	/* Set the last entry to the end of log string. */
	log->impl.static_log.page_start[pages] = ptr;

	UI_INFO("Initialize log_info, page_count: %u, dimensions: %ux%u\n",
		log->impl.static_log.page_count, log->lines_per_page, log->chars_per_line);

	return VB2_SUCCESS;
}

char *ui_log_get_page_content(const struct ui_log_info *log, uint32_t page)
{
	int i;
	char *buf;
	uint32_t chars_current_line;
	const char *ptr, *line_start;

	if (page >= log->impl.static_log.page_count) {
		UI_ERROR("Failed to get page content, "
			 "page: %u, page_count: %u\n", page, log->impl.static_log.page_count);
		return NULL;
	}

	buf = malloc((log->chars_per_line + 1) * log->lines_per_page + 1);
	if (!buf) {
		UI_ERROR("Failed to malloc string buffer, page: %u, "
			 "dimensions: %ux%u\n",
			 page, log->lines_per_page, log->chars_per_line);
		return NULL;
	}

	i = 0;
	ptr = log->impl.static_log.page_start[page];
	while (ptr < log->impl.static_log.page_start[page + 1]) {
		chars_current_line = 0;
		line_start = ptr;
		while (ptr <= log->impl.static_log.page_start[page + 1]) {
			chars_current_line++;
			if (*ptr == '\n' || *ptr == '\0') {
				strncpy(buf + i, line_start,
					chars_current_line);
				i += chars_current_line;
				if (*ptr == '\n')
					ptr++;
				break;
			}
			/* Wrap current line, put current character into next
			   line. */
			if (chars_current_line > log->chars_per_line) {
				strncpy(buf + i, line_start,
					log->chars_per_line);
				i += log->chars_per_line;
				buf[i++] = '\n';  /* Add newline manually. */
				break;
			}
			ptr++;
		}
	}
	/* No newline at the end of the last line. */
	if (i > 0 && buf[i - 1] == '\n')
		i--;
	buf[i] = '\0';

	return buf;
}
