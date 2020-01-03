/*
 * Copyright (C) 2016 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
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

#include "drivers/soc/apollolake.h"
#include "drivers/soc/intel_common.h"

static const SocGpeConfig cfg = {
	.gpe_max = GPE0_DW3_31,
	.acpi_base = ACPI_PMIO_BASE,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int apollolake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}
