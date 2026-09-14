/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2026, The Linux Foundation.  All rights reserved.
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

#include <libpayload.h>
#include "drivers/soc/x1p42100.h"
#include "variant.h"

uintptr_t variant_get_ec_spi_base(void)
{
	return QUP_SERIAL13_BASE;
}

uintptr_t variant_get_gsc_i2c_base(void)
{
	return QUP_SERIAL10_BASE;
}

bool variant_nvme_supported(void)
{
	return false;
}

bool variant_ufs_supported(void)
{
	return true;
}
