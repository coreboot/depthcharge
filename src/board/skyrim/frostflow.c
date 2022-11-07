// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/skyrim/include/variant.h"

enum audio_amp variant_get_audio_amp(void)
{
	return AUDIO_AMP_RT1019;
}
