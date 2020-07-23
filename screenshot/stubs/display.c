// SPDX-License-Identifier: GPL-2.0

#include <stdint.h>

#include "drivers/video/display.h"

void display_set_ops(DisplayOps *ops)
{
}

int display_init(void)
{
	return 0;
}

int display_init_required(void)
{
	return 0;
}

int backlight_update(uint8_t enable)
{
	return 0;
}

int display_screen(enum VbScreenType_t screen)
{
	return 0;
}
