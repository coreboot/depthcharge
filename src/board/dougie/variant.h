/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _BOARD_DOUGIE_VARIANT_H_
#define _BOARD_DOUGIE_VARIANT_H_

#include <stdbool.h>

bool variant_ufs_supported(void);
bool variant_emmc_supported(void);
bool variant_nvme_supported(void);
pcidev_t variant_get_nvme_pcidev(void);
uintptr_t variant_get_ec_spi_base(void);
uintptr_t variant_get_gsc_i2c_base(void);

#endif /* _BOARD_DOUGIE_VARIANT_H_ */
