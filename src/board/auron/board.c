// SPDX-License-Identifier: GPL-2.0

#include <pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/lynxpoint_lp.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/hda_codec.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	HdaCodec *codec = new_hda_codec();
	sound_set_ops(&codec->ops);

	// The realtek codec doesn't report its beep_nid (NID 1)
	set_hda_beep_nid_override(codec, 1);

	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 31, 2));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	power_set_ops(&pch_power_ops);

	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	return 0;
}

INIT_FUNC(board_setup);
