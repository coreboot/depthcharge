/* SPDX-License-Identifier: GPL-2.0-only */

#include "cse_internal.h"
#include "drivers/soc/intel_pmc.h"

#define PMC_IPC_MEI_DISABLE_ID			0xa9
#define PMC_IPC_MEI_DISABLE_SUBID_ENABLE	0
#define PMC_IPC_MEI_DISABLE_SUBID_DISABLE	1

bool cse_disable_mei_devices(void)
{
	struct pmc_ipc_buffer req = { 0 };
	struct pmc_ipc_buffer rsp;
	uint32_t cmd;

	cmd = pmc_make_ipc_cmd(PMC_IPC_MEI_DISABLE_ID, PMC_IPC_MEI_DISABLE_SUBID_DISABLE, 0);
	if (pmc_send_cmd(cmd, &req, &rsp) != CB_SUCCESS) {
		printk(BIOS_ERR, "CSE: Failed to disable MEI devices\n");
		return false;
	}

	return true;
}

/* Disable HECI using PMC IPC communication */
static void heci1_disable_using_pmc(void)
{
	cse_disable_mei_devices();
}

void heci1_disable(void)
{
	if (CONFIG(SOC_INTEL_CSE_HECI1_DISABLE_USING_PMC_IPC)) {
		printk(BIOS_INFO, "Disabling Heci using PMC IPC\n");
		return heci1_disable_using_pmc();
	} else {
		printk(BIOS_ERR, "%s Error: Unable to make HECI1 function disable!\n",
				__func__);
	}
}
