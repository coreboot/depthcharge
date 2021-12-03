// SPDX-License-Identifier: GPL-2.0

#include <vboot/ui.h>
#include <vb2_api.h>

vb2_error_t ui_get_locale_info(uint32_t locale_id,
			       struct ui_locale const **locale)
{
	static struct ui_locale stub_locale;

	*locale = &stub_locale;
	return VB2_SUCCESS;
}

vb2_error_t ui_get_locale_info_fallback(uint32_t locale_id,
					uint32_t fallback_locale_id,
					struct ui_locale const **locale)
{
	return ui_get_locale_info(locale_id, locale);
}

vb2_error_t ui_get_language_name_bitmap(const char *locale_code,
					struct ui_bitmap *bitmap)
{
	return VB2_SUCCESS;
}

vb2_error_t ui_get_bitmap(const char *image_name, const char *locale_code,
			  int focused, struct ui_bitmap *bitmap)
{
	return VB2_SUCCESS;
}

uint32_t ui_get_locale_count(void)
{
	return 1;
}
