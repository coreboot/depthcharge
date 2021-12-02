/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2021 Google LLC.  */

#ifndef __DRIVERS_SOUND_AMD_ACP__H__
#define __DRIVERS_SOUND_AMD_ACP__H__

#include <libpayload.h>

#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"

/* ACP Device */
#define AMD_PCI_VID		0x1022
#define AMD_FAM17H_ACP_PCI_DID	0x15E2

typedef struct AmdAcp {
	SoundRouteComponent component;
	SoundOps sound_ops;
} AmdAcp;

AmdAcp *new_amd_acp(pcidev_t pci_dev, uint32_t lrclk, int16_t volume);

#endif /* __DRIVERS_SOUND_AMD_ACP__H__ */
