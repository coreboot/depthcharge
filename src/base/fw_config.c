// SPDX-License-Identifier: GPL-2.0
/* Copyright 2020 Google LLC. */

#include "base/fw_config.h"
#include <inttypes.h>
#include <stdio.h>
#include <sysinfo.h>

bool fw_config_probe(const struct fw_config *match)
{
	if ((lib_sysinfo.fw_config & match->mask) == match->value) {
		if (match->field_name && match->option_name)
			printf("fw_config match found: %s=%s\n",
			       match->field_name, match->option_name);
		else
			printf("fw_config match found: mask=0x%" PRIx64
			       " value=0x%" PRIx64 "\n", match->mask,
			       match->value);

		return true;
	}

	return false;
}

bool fw_config_is_provisioned(void)
{
	return lib_sysinfo.fw_config != UNDEFINED_FW_CONFIG;
}
