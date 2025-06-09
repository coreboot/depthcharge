// SPDX-License-Identifier: GPL-2.0

#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

#include "drivers/ec/cros/lpc.h"
#include "drivers/sound/intel_audio_setup.h"

#define I2C0 PCI_DEV(0, 0x15, 0)
#define I2C7 PCI_DEV(0, 0x10, 1)

struct tpm_config {
	pcidev_t pci_dev;
};

const struct tpm_config *variant_get_tpm_config(void);
const int variant_get_ec_int(void);
int gsc_irq_status(void);
const CrosEcLpcBusVariant variant_get_ec_lpc_bus(void);

#endif /* _VARIANT_H_ */
