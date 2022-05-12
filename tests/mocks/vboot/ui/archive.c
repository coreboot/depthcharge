// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vboot/ui.h>
#include <vb2_api.h>

vb2_error_t ui_get_locale_info(uint32_t locale_id,
			       struct ui_locale const **locale)
{
	static struct ui_locale stub_locale;

	stub_locale.id = locale_id;
	*locale = &stub_locale;
	return VB2_SUCCESS;
}

uint32_t ui_get_locale_count(void)
{
	return mock_type(uint32_t);
}

vb2_error_t ui_load_bitmap(enum ui_archive_type type, const char *file,
			   const char *locale_code, struct ui_bitmap *bitmap)
{
	return mock_type(vb2_error_t);
}
