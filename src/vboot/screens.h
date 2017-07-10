/*
 * Copyright 2015 Google Inc.
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

#ifndef __VBOOT_SCREENS_H__
#define __VBOOT_SCREENS_H__

#include <stdint.h>

int vboot_draw_screen(uint32_t screen, uint32_t locale);
int vboot_draw_ui(uint32_t screen, uint32_t locale,
		  uint32_t selected_index, uint32_t disabled_idx_mask,
		  uint32_t redraw_base);

/**
 * Return number of supported locales
 *
 * @return number of supported locales
 */
int vboot_get_locale_count(void);

#endif /* __VBOOT_SCREENS_H__ */
