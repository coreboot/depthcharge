/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2020, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SC7180_VIDEO_H
#define __SC7180_VIDEO_H

#include "drivers/gpio/gpio.h"
#include "drivers/video/display.h"

DisplayOps *new_sc7180_display(GpioOps *backlight_gpio_ops);

#endif

