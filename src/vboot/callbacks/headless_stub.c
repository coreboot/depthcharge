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

#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>  /* for VbScreenData */

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale)
{
	/* TODO(b/151200757): Support headless devices */
	return VB2_SUCCESS;
}

/*
 * TODO(chromium:1055125): Before decoupling EC/AUXFW sync and UI, keep this
 * dummy function here as a workaround.
 */
vb2_error_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale,
			      const VbScreenData *data)
{
	return VB2_SUCCESS;
}

int video_console_init(void)
{
	printf("%s stub called\n", __func__);
	return 0;
}
