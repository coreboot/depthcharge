// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "base/init_funcs.h"
#include "drivers/soc/alderlake.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"
#include "drivers/bus/usb/intel_tcss.h"

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int alderlake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}

static const TcssCtrlr adl_tcss_ctrlr = {
	.regbar = 0xfb000000,
	.iom_pid = 0xc1,
	.iom_status_offset = 0x160
};

static int register_tcss(void)
{
	if (CONFIG(DRIVER_USB_INTEL_TCSS))
		register_tcss_ctrlr(&adl_tcss_ctrlr);

	return 0;
}

INIT_FUNC(register_tcss);
