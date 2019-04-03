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

#ifndef _SOC_QUALCOMM_QCS405_GPIO_H_
#define _SOC_QUALCOMM_QCS405_GPIO_H_

#include "drivers/gpio/gpio.h"

typedef union {
	uint32_t addr;
} qcs405GpioSpec;

typedef struct {
	uint32_t _res1;
	uint32_t in_out;
	uint32_t _res2;
	uint32_t intr_status;
} TlmmGpio;

typedef struct {
	GpioOps ops;
	qcs405GpioSpec gpio;
} Qcs405GpioCfg;

#define TLMM_TILE_SIZE		0x00400000
#define TLMM_GPIO_OFF_DELTA	0x00001000
#define TLMM_GPIO_TILE_NUM	3

#define TLMM_EAST_TILE_BASE	0x07B00000
#define TLMM_NORTH_TILE_BASE	0x01300000
#define TLMM_SOUTH_TILE_BASE	0x01000000

/* GPIO TLMM: Status */
#define GPIO_DISABLE		0
#define GPIO_ENABLE		1

/* GPIO IO: Mask */
#define GPIO_IO_IN_BMSK		0x1
#define GPIO_IO_OUT_BMSK	0x1

/* GPIO IO: Shift */
#define GPIO_IO_IN_SHFT		0
#define GPIO_IO_OUT_SHFT	1

/* GPIO MAX Valid # */
#define GPIO_NUM_MAX		119

#define GPIO(num) ((qcs405GpioSpec){.addr = GPIO##num##_ADDR})

#define PIN(index, tlmm) \
GPIO##index##_ADDR = (TLMM_##tlmm##_TILE_BASE + index * TLMM_GPIO_OFF_DELTA)

enum {
	PIN(0, SOUTH),
	PIN(1, SOUTH),
	PIN(2, SOUTH),
	PIN(3, SOUTH),
	PIN(4, SOUTH),
	PIN(5, SOUTH),
	PIN(6, SOUTH),
	PIN(7, SOUTH),
	PIN(8, SOUTH),
	PIN(9, SOUTH),
	PIN(10, SOUTH),
	PIN(11, SOUTH),
	PIN(12, SOUTH),
	PIN(13, SOUTH),
	PIN(14, SOUTH),
	PIN(15, SOUTH),
	PIN(16, SOUTH),
	PIN(17, NORTH),
	PIN(18, NORTH),
	PIN(19, NORTH),
	PIN(20, NORTH),
	PIN(21, SOUTH),
	PIN(22, NORTH),
	PIN(23, NORTH),
	PIN(24, NORTH),
	PIN(25, NORTH),
	PIN(26, EAST),
	PIN(27, EAST),
	PIN(28, EAST),
	PIN(29, EAST),
	PIN(30, NORTH),
	PIN(31, NORTH),
	PIN(32, NORTH),
	PIN(33, NORTH),
	PIN(34, SOUTH),
	PIN(35, SOUTH),
	PIN(36, NORTH),
	PIN(37, NORTH),
	PIN(38, NORTH),
	PIN(39, EAST),
	PIN(40, EAST),
	PIN(41, EAST),
	PIN(42, EAST),
	PIN(43, EAST),
	PIN(44, EAST),
	PIN(45, EAST),
	PIN(46, EAST),
	PIN(47, EAST),
	PIN(48, EAST),
	PIN(49, EAST),
	PIN(50, EAST),
	PIN(51, EAST),
	PIN(52, EAST),
	PIN(53, EAST),
	PIN(54, EAST),
	PIN(55, EAST),
	PIN(56, EAST),
	PIN(57, EAST),
	PIN(58, EAST),
	PIN(59, EAST),
	PIN(60, NORTH),
	PIN(61, NORTH),
	PIN(62, NORTH),
	PIN(63, NORTH),
	PIN(64, NORTH),
	PIN(65, NORTH),
	PIN(66, NORTH),
	PIN(67, NORTH),
	PIN(68, NORTH),
	PIN(69, NORTH),
	PIN(70, NORTH),
	PIN(71, NORTH),
	PIN(72, NORTH),
	PIN(73, NORTH),
	PIN(74, NORTH),
	PIN(75, NORTH),
	PIN(76, NORTH),
	PIN(77, NORTH),
	PIN(78, EAST),
	PIN(79, EAST),
	PIN(80, EAST),
	PIN(81, EAST),
	PIN(82, EAST),
	PIN(83, NORTH),
	PIN(84, NORTH),
	PIN(85, NORTH),
	PIN(86, EAST),
	PIN(87, EAST),
	PIN(88, EAST),
	PIN(89, EAST),
	PIN(90, EAST),
	PIN(91, EAST),
	PIN(92, EAST),
	PIN(93, EAST),
	PIN(94, EAST),
	PIN(95, EAST),
	PIN(96, EAST),
	PIN(97, EAST),
	PIN(98, EAST),
	PIN(99, EAST),
	PIN(100, EAST),
	PIN(101, EAST),
	PIN(102, EAST),
	PIN(103, EAST),
	PIN(104, EAST),
	PIN(105, EAST),
	PIN(106, EAST),
	PIN(107, EAST),
	PIN(108, EAST),
	PIN(109, EAST),
	PIN(110, EAST),
	PIN(111, EAST),
	PIN(112, EAST),
	PIN(113, EAST),
	PIN(114, EAST),
	PIN(115, EAST),
	PIN(116, EAST),
	PIN(117, NORTH),
	PIN(118, NORTH),
	PIN(119, EAST),
};

Qcs405GpioCfg *new_qcs405_gpio_input(qcs405GpioSpec gpio);
Qcs405GpioCfg *new_qcs405_gpio_output(qcs405GpioSpec gpio);
Qcs405GpioCfg *new_qcs405_gpio_latched(qcs405GpioSpec gpio);
GpioOps *new_qcs405_gpio_input_from_coreboot(uint32_t gpio);
GpioOps *new_qcs405_gpio_output_from_coreboot(uint32_t gpio);
GpioOps *new_qcs405_gpio_latched_from_coreboot(uint32_t gpio);

#endif // _SOC_QUALCOMM_QCS405_GPIO_H_
