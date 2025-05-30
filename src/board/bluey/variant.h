/*
* This file is part of the coreboot project.
 *
 * Copyright (C) 2025, The Linux Foundation.  All rights reserved.
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

#ifndef _BOARD_BLUEY_VARIANT_H_
#define _BOARD_BLUEY_VARIANT_H_

pcidev_t variant_get_nvme_pcidev(void);

uintptr_t variant_get_ec_spi_base(void);

uintptr_t variant_get_gsc_i2c_base(void);

#endif // _BOARD_BLUEY_VARIANT_H_

