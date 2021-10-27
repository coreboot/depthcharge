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

typedef struct TcssCtrlr {
	uintptr_t regbar;
	size_t iom_pid;
	size_t iom_status_offset;
} TcssCtrlr;

/* SoC-specific code should call this one time */
void register_tcss_ctrlr(const TcssCtrlr *tcss_ctrlr);

#endif /* __DRIVERS_BUS_INTEL_TCSS_H__ */
