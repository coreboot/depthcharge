// SPDX-License-Identifier: GPL-2.0

#include <arch/cache.h>
#include <stdint.h>
#include <stdio.h>

#include "arch/arm/smc.h"
#include "vboot/widevine.h"

int widevine_write_smc_data(uint64_t function_id, uint8_t *data,
			    uint32_t length)
{
	if (!CONFIG(WIDEVINE_SMC)) {
		/* We don't have widevine SMC support now.
		   Just ignore the SMC call. */
		return 0;
	}

	/* Flush the cache before calling the SMC. */
	dcache_clean_by_mva(data, length);

	return smc(function_id, length, (uint64_t)data, 0, 0, 0, 0);
}
