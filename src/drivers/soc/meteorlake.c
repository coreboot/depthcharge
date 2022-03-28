// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Intel Corporation */

#include "drivers/soc/meteorlake.h"
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"

static const SocGpeConfig cfg = {
	.gpe_max = GPE_MAX,
	.acpi_base = ACPI_BASE_ADDRESS,
	.gpe0_sts_off = GPE0_STS_OFF,
};

int meteorlake_get_gpe(int gpe)
{
	return soc_get_gpe(gpe, &cfg);
}
