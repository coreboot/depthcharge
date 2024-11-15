#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>

#include "drivers/sound/intel_audio_setup.h"

struct tpm_config {
	pcidev_t pci_dev;
};

const struct tpm_config *variant_get_tpm_config(void);
const int variant_get_ec_int(void);
int gsc_irq_status(void);

#endif /* _VARIANT_H_ */
