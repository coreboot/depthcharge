/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __DRIVERS_SOC_CSE_CSE_INTERNAL_H__
#define __DRIVERS_SOC_CSE_CSE_INTERNAL_H__

#include <assert.h>
#include <inttypes.h>
#include <libpayload.h>
#include <lp_vboot.h>
#include <pci.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "base/timestamp.h"
#include "drivers/power/power.h"
#include "drivers/soc/intel_common.h"
#include "drivers/timer/timer.h"
#include "vboot/stages.h"

#define post_code(pc)

#define PCH_DEVFN_CSE PCH_DEV_CSE
#define PCH_DEV_SLOT_CSE PCI_SLOT(PCH_DEV_CSE)

#define PCI_BASE_ADDRESS_MEM_ATTR_MASK	0x0f

void pmc_global_reset_disable_and_lock(void);
void pmc_global_reset_enable(bool enable);
void do_global_reset(void);
bool acpi_get_sleep_type_s3(void);

#endif // __DRIVERS_SOC_CSE_CSE_INTERNAL_H__
