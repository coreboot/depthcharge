/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

enum storage_media {
	STORAGE_NVME,
	STORAGE_SDHCI,
};

struct storage_config {
	enum storage_media media;
	pcidev_t pci_dev;
};

const struct storage_config *variant_get_storage_configs(size_t *count);
#endif /* _VARIANT_H_ */
