/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#ifndef __DRIVERS_BUS_INTEL_TCSS_H__
#define __DRIVERS_BUS_INTEL_TCSS_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "base/list.h"

typedef struct TcssCtrlr {
	uintptr_t regbar;
	size_t iom_pid;
	size_t iom_status_offset;
} TcssCtrlr;

/* Platforms implement this abstract function */
bool is_port_connected(int port, int usb2);

#endif /* __DRIVERS_BUS_INTEL_TCSS_H__ */
