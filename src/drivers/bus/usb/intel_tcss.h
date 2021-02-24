/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#ifndef __DRIVERS_BUS_INTEL_TCSS_H__
#define __DRIVERS_BUS_INTEL_TCSS_H__

#include <stdint.h>
#include <stddef.h>

struct tcss_port_map {
	uint8_t usb3;
	uint8_t usb2;
};

typedef struct TcssConfig {
	uintptr_t regbar;
	size_t iom_pid;
	size_t iom_status_offset;
} TcssConfig;

int board_tcss_get_port_mapping(const struct tcss_port_map **map);

const TcssConfig *platform_get_tcss_config(void);

#endif /* __DRIVERS_BUS_INTEL_TCSS_H__ */
