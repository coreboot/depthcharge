/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */

#ifndef __DRIVERS_BUS_TIGERLAKE_TCSS_H__
#define __DRIVERS_BUS_TIGERLAKE_TCSS_H__

#include <stdint.h>

struct tcss_port_map {
	uint8_t usb3;
	uint8_t usb2;
};

int board_tcss_get_port_mapping(const struct tcss_port_map **map);

#endif /* __DRIVERS_BUS_TIGERLAKE_TCSS_H__ */
