/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

/* cr50 / Ti50 interrupt is attached to either GPIO_85 or GPIO_3 */
#define CR50_INT_85		85
#define CR50_INT_3		3

enum storage_media {
	STORAGE_NVME,
	STORAGE_SDHCI,
};

struct storage_config {
	enum storage_media media;
	pcidev_t pci_dev;
};

const struct storage_config *variant_get_storage_configs(size_t *count);

unsigned int variant_get_cr50_irq_gpio(void);
#endif /* _VARIANT_H_ */
