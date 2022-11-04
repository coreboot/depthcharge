// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
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

#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"
#include "drivers/soc/tigerlake.h"
#include "drivers/bus/usb/intel_tcss.h"

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int tigerlake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}

static const TcssCtrlr tgl_tcss_ctrlr = {
	.regbar = 0xfb000000,
	.iom_pid = 0xc1,
	.iom_status_offset = 0x560
};

const void *port_status_reg(int port)
{
	const uintptr_t status_reg = tgl_tcss_ctrlr.regbar +
		(tgl_tcss_ctrlr.iom_pid << REGBAR_PID_SHIFT) +
		(tgl_tcss_ctrlr.iom_status_offset + port * sizeof(uint32_t));
	return (const void *)status_reg;
}

bool is_port_connected(int port)
{
	uint32_t sts;

	sts = read32(port_status_reg(port));
	return !!(sts & IOM_PORT_STATUS_CONNECTED);
}
