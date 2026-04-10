// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2026 Google LLC
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

/*
 * Get size of the fastboot log line which starts at character pointed by 'iter'.
 *
 * Returns number of characters in line or 0 if 'iter' is invalid. At exit, 'iter' is set
 * to the first character of the next line or UINT64_MAX if it was the last line.
 */
static size_t get_next_fastboot_log_line(struct fastboot_log *log, uint64_t *iter,
					 uint32_t chars_per_line)
{
	if (*iter == UINT64_MAX)
		return 0;

	for (int i = 1; i <= chars_per_line; i++) {
		char c = fastboot_log_get_byte_at_iter(log, *iter);
		if (!fastboot_log_inc_iter(log, iter)) {
			/* No more chars, invalidate iterator and return last line length */
			*iter = UINT64_MAX;
			return i;
		}
		if (c == '\n')
			return i;
	}

	if (fastboot_log_get_byte_at_iter(log, *iter) == '\n') {
		/* This '\n' is effectively printed in this line, so count it */
		if (!fastboot_log_inc_iter(log, iter))
			/* No more chars, invalidate iterator */
			*iter = UINT64_MAX;
		return chars_per_line + 1;
	}
	return chars_per_line;
}

/*
 * Get size of the fastboot log line which ends at character pointed by 'iter'.
 *
 * Returns number of characters in line or 0 if 'iter' is invalid. At exit, 'iter' is set
 * to the last character of the previous line or UINT64_MAX if it was the first line.
 */
static size_t get_prev_fastboot_log_line(struct fastboot_log *log, uint64_t *iter,
					 uint32_t chars_per_line)
{
	int start_len = 0;

	if (*iter == UINT64_MAX)
		return 0;

	if (fastboot_log_get_byte_at_iter(log, *iter) == '\n') {
		/*
		 * This '\n' is effectively printed at the end of the line, skip it, but count
		 * towards line length. Increase chars limit too.
		 */
		start_len++;
		chars_per_line++;
		if (!fastboot_log_dec_iter(log, iter)) {
			/* No more chars, invalidate iterator and return last line length */
			*iter = UINT64_MAX;
			return 1;
		}
	}

	for (int i = start_len; i < chars_per_line; i++) {
		char c = fastboot_log_get_byte_at_iter(log, *iter);
		if (c == '\n')
			return i;
		if (!fastboot_log_dec_iter(log, iter)) {
			/* No more chars, invalidate iterator and return last line length */
			*iter = UINT64_MAX;
			return i;
		}
	}

	return chars_per_line;
}

/*
 * Get size of the fastboot log page that starts at 'start' byte.
 *
 * Returns size of the page or 0 if there is no enough lines from the start byte to fill whole
 * printable area.
 */
static size_t get_next_fastboot_log_page_len(struct fastboot_log *log, uint64_t start,
					     uint32_t chars_per_line, uint32_t lines_per_page)
{
	uint64_t iter = fastboot_log_get_iter(log, start);
	size_t page_len = 0;

	for (int line = 0; line < lines_per_page; line++) {
		size_t line_len = get_next_fastboot_log_line(log, &iter, chars_per_line);
		if (line_len == 0)
			return 0;
		page_len += line_len;
	}

	return page_len;
}

/*
 * Get size of the fastboot log page that ends at 'end' - 1 byte ('end' is the first byte of
 * the next page)
 *
 * Returns size of the page or 0 if there is no enough lines from the end byte to fill whole
 * printable area.
 */
static size_t get_prev_fastboot_log_page_len(struct fastboot_log *log, uint64_t end,
					     uint32_t chars_per_line, uint32_t lines_per_page)
{
	uint64_t iter = fastboot_log_get_iter(log, end - 1);
	size_t page_len = 0;
	size_t line_len = 0;

	for (int line = 0; line < lines_per_page; line++) {
		line_len = get_prev_fastboot_log_line(log, &iter, chars_per_line);
		if (line_len == 0)
			return 0;
		page_len += line_len;
	}
	line_len = 0;
	while (iter != UINT64_MAX && fastboot_log_get_byte_at_iter(log, iter) != '\n')
		/*
		 * Last line is wider than screen and is split between pages. Adjust page len,
		 * to not print part of the line which would be printed on the previous page.
		 */
		line_len = get_prev_fastboot_log_line(log, &iter, chars_per_line);
	if (line_len)
		page_len -= chars_per_line - line_len;

	return page_len;
}

void ui_fb_log_set_first_page(struct ui_log_info *ui_log, struct fastboot_log *log)
{
	const uint64_t oldest_byte = fastboot_log_get_oldest_available_byte(log);
	size_t page_len = get_next_fastboot_log_page_len(log, oldest_byte,
							 ui_log->chars_per_line,
							 ui_log->lines_per_page);
	if (page_len == 0)
		/* No full page, print everything */
		page_len = SIZE_MAX;

	ui_log->impl.fastboot_log.top_of_screen_byte_anchor = oldest_byte;
	ui_log->impl.fastboot_log.bytes_on_screen = page_len;
}

void ui_fb_log_set_last_page(struct ui_log_info *ui_log, struct fastboot_log *log)
{
	const uint64_t total_bytes = fastboot_log_get_total_bytes(log);
	size_t page_len = get_prev_fastboot_log_page_len(log, total_bytes,
							 ui_log->chars_per_line,
							 ui_log->lines_per_page);
	if (page_len == 0)
		/* No full page, print everything */
		page_len = total_bytes - fastboot_log_get_oldest_available_byte(log);

	ui_log->impl.fastboot_log.top_of_screen_byte_anchor = total_bytes - page_len;
	ui_log->impl.fastboot_log.bytes_on_screen = page_len;
}

void ui_fb_log_set_next_page(struct ui_log_info *ui_log, struct fastboot_log *log)
{
	const uint64_t start = ui_log->impl.fastboot_log.top_of_screen_byte_anchor +
			       ui_log->impl.fastboot_log.bytes_on_screen;

	size_t page_len = get_next_fastboot_log_page_len(log, start, ui_log->chars_per_line,
							 ui_log->lines_per_page);

	if (page_len == 0) {
		/* Can't get whole page from 'start' byte. Get last page instead. */
		ui_fb_log_set_last_page(ui_log, log);
		return;
	}

	ui_log->impl.fastboot_log.top_of_screen_byte_anchor = start;
	ui_log->impl.fastboot_log.bytes_on_screen = page_len;
}

void ui_fb_log_set_prev_page(struct ui_log_info *ui_log, struct fastboot_log *log)
{
	const uint64_t end = ui_log->impl.fastboot_log.top_of_screen_byte_anchor;
	size_t page_len = get_prev_fastboot_log_page_len(log, end, ui_log->chars_per_line,
							 ui_log->lines_per_page);

	if (page_len == 0) {
		/* Can't get whole page from 'end' byte. Get first page instead. */
		ui_fb_log_set_first_page(ui_log, log);
		return;
	}

	ui_log->impl.fastboot_log.top_of_screen_byte_anchor = end - page_len;
	ui_log->impl.fastboot_log.bytes_on_screen = page_len;
}

vb2_error_t ui_fb_log_init(enum ui_screen screen, const char *locale_code,
			   struct ui_log_info *log)
{
	VB2_TRY(ui_log_common_init(screen, locale_code, log, UI_LOG_TYPE_FASTBOOT));
	/* Fastboot log draw function will update this and print last page on the first run */
	log->impl.fastboot_log.total_bytes = UINT64_MAX;
	log->impl.fastboot_log.top_of_screen_byte_anchor = UINT64_MAX;
	log->impl.fastboot_log.bytes_on_screen = 0;

	return VB2_SUCCESS;
}
