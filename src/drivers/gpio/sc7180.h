/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2019, The Linux Foundation.  All rights reserved.
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

#ifndef _SOC_QUALCOMM_SC7180_GPIO_H_
#define _SOC_QUALCOMM_SC7180_GPIO_H_

#include "gpio.h"

#define TLMM_WEST_TILE_BASE	0x3500000
#define TLMM_NORTH_TILE_BASE	0x3900000
#define TLMM_SOUTH_TILE_BASE	0x3D00000

#define TLMM_GPIO_OFF_DELTA     0x1000

#define PIN(index, tlmm) \
	GPIO##index##_ADDR = TLMM_##tlmm##_TILE_BASE + index * TLMM_GPIO_OFF_DELTA

enum {
	PIN(0, SOUTH),
	PIN(1, SOUTH),
	PIN(2, SOUTH),
	PIN(3, SOUTH),
	PIN(4, NORTH),
	PIN(5, NORTH),
	PIN(6, NORTH),
	PIN(7, NORTH),
	PIN(8, NORTH),
	PIN(9, NORTH),
	PIN(10, NORTH),
	PIN(11, NORTH),
	PIN(12, SOUTH),
	PIN(13, SOUTH),
	PIN(14, SOUTH),
	PIN(15, SOUTH),
	PIN(16, SOUTH),
	PIN(17, SOUTH),
	PIN(18, SOUTH),
	PIN(19, SOUTH),
	PIN(20, SOUTH),
	PIN(21, NORTH),
	PIN(22, NORTH),
	PIN(23, SOUTH),
	PIN(24, SOUTH),
	PIN(25, SOUTH),
	PIN(26, SOUTH),
	PIN(27, SOUTH),
	PIN(28, SOUTH),
	PIN(29, NORTH),
	PIN(30, SOUTH),
	PIN(31, NORTH),
	PIN(32, NORTH),
	PIN(33, NORTH),
	PIN(34, SOUTH),
	PIN(35, SOUTH),
	PIN(36, SOUTH),
	PIN(37, SOUTH),
	PIN(38, SOUTH),
	PIN(39, SOUTH),
	PIN(40, SOUTH),
	PIN(41, SOUTH),
	PIN(42, NORTH),
	PIN(43, NORTH),
	PIN(44, NORTH),
	PIN(45, NORTH),
	PIN(46, NORTH),
	PIN(47, NORTH),
	PIN(48, NORTH),
	PIN(49, WEST),
	PIN(50, WEST),
	PIN(51, WEST),
	PIN(52, WEST),
	PIN(53, WEST),
	PIN(54, WEST),
	PIN(55, WEST),
	PIN(56, WEST),
	PIN(57, WEST),
	PIN(58, WEST),
	PIN(59, NORTH),
	PIN(60, NORTH),
	PIN(61, NORTH),
	PIN(62, NORTH),
	PIN(63, NORTH),
	PIN(64, NORTH),
	PIN(65, NORTH),
	PIN(66, NORTH),
	PIN(67, NORTH),
	PIN(68, NORTH),
	PIN(69, WEST),
	PIN(70, NORTH),
	PIN(71, NORTH),
	PIN(72, NORTH),
	PIN(73, WEST),
	PIN(74, WEST),
	PIN(75, WEST),
	PIN(76, WEST),
	PIN(77, WEST),
	PIN(78, WEST),
	PIN(79, WEST),
	PIN(80, WEST),
	PIN(81, WEST),
	PIN(82, WEST),
	PIN(83, WEST),
	PIN(84, WEST),
	PIN(85, WEST),
	PIN(86, NORTH),
	PIN(87, NORTH),
	PIN(88, NORTH),
	PIN(89, NORTH),
	PIN(90, NORTH),
	PIN(91, NORTH),
	PIN(92, NORTH),
	PIN(93, NORTH),
	PIN(94, SOUTH),
	PIN(95, WEST),
	PIN(96, WEST),
	PIN(97, WEST),
	PIN(98, WEST),
	PIN(99, WEST),
	PIN(100, WEST),
	PIN(101, NORTH),
	PIN(102, NORTH),
	PIN(103, NORTH),
	PIN(104, WEST),
	PIN(105, NORTH),
	PIN(106, NORTH),
	PIN(107, WEST),
	PIN(108, SOUTH),
	PIN(109, SOUTH),
	PIN(110, NORTH),
	PIN(111, NORTH),
	PIN(112, NORTH),
	PIN(113, NORTH),
	PIN(114, NORTH),
	PIN(115, WEST),
	PIN(116, WEST),
	PIN(117, WEST),
	PIN(118, WEST),
};

#endif // _SOC_QUALCOMM_SC7180_GPIO_H_
