/*
 * Copyright 2014 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/lynxpoint_lp.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/pcat_beep.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/flag.h"

#include <vboot_api.h>
#include <vboot_nvstorage.h>

VbError_t VbDisplayScreen(VbCommonParams *cparams, uint32_t screen, int force,
                          VbNvContext *vncptr);

static void check_power_adapter(void)
{
	// GPIO48 = ADP_ID:
	//  0 = 65W or higer, or if the MB does not support probing adapter.
	//  1 = lower than 65W (ex, 45W, which we should alert and shutdown).
	LpPchGpio *ad_type = new_lp_pch_gpio_input(48);
	if (!ad_type->ops.get(&ad_type->ops))
		return;

	// Minimal setup of vboot environment for displaying GBB screen.
	common_params_init(0);
	cparams.gbb = (struct GoogleBinaryBlockHeader *)cparams.gbb_data;
	VbNvContext vnc;
	VbExNvStorageRead(vnc.raw);
	VbNvSetup(&vnc);
	VbDisplayScreen(&cparams, VB_SCREEN_WRONG_ADAPTER, 0, &vnc);
	VbExSleepMs(60 * 1000);
	power_off();
}

static int board_setup(void)
{
	if (sysinfo_install_flags())
		return 1;

	// Read the current value of the recovery button instead of the
	// value passed by coreboot.
	LpPchGpio *rec_gpio = new_lp_pch_gpio_input(12);
	if (flag_replace(FLAG_RECSW, new_gpio_not(&rec_gpio->ops)))
		return 1;

	MemMappedFlash *flash = new_mem_mapped_flash(0xff800000, 0x800000);
	if (flash_set_ops(&flash->ops))
		return 1;

	PcAtBeep *beep = new_pcat_beep();
	if (sound_set_ops(&beep->ops))
		return 1;

	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 31, 2));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	if (power_set_ops(&pch_power_ops))
		return 1;

	LpcTpm *tpm = new_lpc_tpm((void *)0xfed40000);
	if (tpm_set_ops(&tpm->ops))
		return 1;

	check_power_adapter();
	return 0;
}

INIT_FUNC(board_setup);
