// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <vb2_api.h>
#include <vboot/ui.h>

vb2_error_t ui_draw_rounded_box(int32_t x, int32_t y,
				int32_t width, int32_t height,
				const struct rgb_color *rgb,
				uint32_t thickness, uint32_t radius,
				int reverse)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_h_line(int32_t x, int32_t y,
			   int32_t length, int32_t thickness,
			   const struct rgb_color *rgb)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_mapped_bitmap(const struct ui_bitmap *bitmap,
				  int32_t x, int32_t y,
				  int32_t width, int32_t height,
				  const struct rgb_color *bg_color,
				  const struct rgb_color *fg_color,
				  uint32_t flags, int reverse)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_draw_bitmap(const struct ui_bitmap *bitmap,
			   int32_t x, int32_t y, int32_t width, int32_t height,
			   uint32_t flags, int reverse)
{
	return VB2_SUCCESS;
}

uint32_t ui_get_bitmap_num_lines(const struct ui_bitmap *bitmap)
{
	return 1;
}

vb2_error_t ui_draw_box(int32_t x, int32_t y,
			int32_t width, int32_t height,
			const struct rgb_color *rgb,
			int reverse)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_get_log_textbox_dimensions(enum ui_screen screen,
					  const char *locale_code,
					  uint32_t *lines_per_page,
					  uint32_t *chars_per_line)
{
	return VB2_SUCCESS;
}
