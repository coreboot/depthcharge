// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright 2021 Google LLC.  */

#include <arch/msr.h>
#include <libpayload.h>

#include "drivers/sound/amd_i2s_support.h"
#include "pci.h"

static int amd_i2s_support_enable(SoundRouteComponentOps *me)
{
	pcidev_t pci_dev;
	uintptr_t acp_base;
	amdI2sSupport *i2s = container_of(me, amdI2sSupport, component.ops);

	if (pci_find_device(AMD_PCI_VID, AMD_FAM17H_ACP_PCI_DID, &pci_dev))
		acp_base = pci_read_config32(pci_dev, PCI_BASE_ADDRESS_0);
	else
		return -1;

	/* Save Core Boost setting */
	i2s->save_config = _rdmsr(HW_CONFIG_REG);
	i2s->pin_config = read32((void *)(acp_base + ACP_I2S_PIN_CONFIG));
	i2s->pad_ctrl =
		read32((void *)(acp_base + ACP_PAD_PULLUP_PULLDOWN_CTRL));
	/* Disable Core Boost while bit-banging I2S */
	_wrmsr(HW_CONFIG_REG, i2s->save_config | HW_CONFIG_CPBDIS);
	/* tri-state ACP pins */
	write32((void *)(acp_base + ACP_I2S_PIN_CONFIG), 7);
	write32((void *)(acp_base + ACP_PAD_PULLUP_PULLDOWN_CTRL), 0);

	return 0;
}

static int amd_i2s_support_disable(SoundRouteComponentOps *me)
{
	pcidev_t pci_dev;
	uintptr_t acp_base;
	amdI2sSupport *i2s = container_of(me, amdI2sSupport, component.ops);

	if (pci_find_device(AMD_PCI_VID, AMD_FAM17H_ACP_PCI_DID, &pci_dev))
		acp_base = pci_read_config32(pci_dev, PCI_BASE_ADDRESS_0);
	else
		return -1;

	/* Restore previous Core Boost setting */
	_wrmsr(HW_CONFIG_REG, i2s->save_config);
	/* Restore ACP reg settings */
	write32((void *)(acp_base + ACP_I2S_PIN_CONFIG), i2s->pin_config);
	write32((void *)(acp_base + ACP_PAD_PULLUP_PULLDOWN_CTRL),
		i2s->pad_ctrl);

	return 0;
}

amdI2sSupport *new_amd_i2s_support()
{
	amdI2sSupport *i2s = xzalloc(sizeof(*i2s));

	i2s->component.ops.enable = &amd_i2s_support_enable;
	i2s->component.ops.disable = &amd_i2s_support_disable;

	return i2s;
}
