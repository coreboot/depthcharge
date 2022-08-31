// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/skyrim/include/variant.h"

enum audio_amp variant_get_audio_amp(void)
{
	if (fw_config_probe(FW_CONFIG(AUDIO_DB, AUDIO_DB_C_ALC5682I_A_ALC1019)))
		return AUDIO_AMP_RT1019;
	else if (fw_config_probe(
		FW_CONFIG(AUDIO_DB, AUDIO_DB_C_NAU88L25YGB_A_MAX98360AENL)))
		return AUDIO_AMP_MAX98360;
	else
		return AUDIO_AMP_INVALID;
}
