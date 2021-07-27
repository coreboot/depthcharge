/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#ifndef __DRIVERS_BUS_INTEL_TCSS_H__
#define __DRIVERS_BUS_INTEL_TCSS_H__

#include <stdint.h>
#include <stddef.h>

#include "base/list.h"

typedef struct tcss_port {
	unsigned int usb2_port;
	unsigned int usb3_port;
	unsigned int ec_port;
	ListNode list_node;
} TcssPort;

typedef struct TcssCtrlr {
	uintptr_t regbar;
	size_t iom_pid;
	size_t iom_status_offset;
} TcssCtrlr;

/*
 * Each USB Type-C port consists of a TCP (USB3) and a USB2 port from
 * the SoC. This table captures the mapping.
 *
 * SoC USB2 ports are numbered 1..10
, * SoC TCP (USB3) ports are numbered 0..3
 */
struct tcss_map {
	unsigned int usb2_port;
	unsigned int usb3_port;
	unsigned int ec_port;
};

void register_tcss_ports(const struct tcss_map *ports, size_t num_ports);

/* SoC-specific code should call this one time */
void register_tcss_ctrlr(const TcssCtrlr *tcss_ctrlr);

#endif /* __DRIVERS_BUS_INTEL_TCSS_H__ */
