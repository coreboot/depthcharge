// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/gpio/alderlake.h"

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	return &config;
}

const struct tcss_map *variant_get_tcss_map(size_t *count)
{
	*count = 0;
	return NULL;
}
