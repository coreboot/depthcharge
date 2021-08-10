/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2021 Google LLC.  */

#ifndef __DRIVERS_SOUND_AMD_I2S_SUPPORT_H__
#define __DRIVERS_SOUND_AMD_I2S_SUPPORT_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

/* Core boost register */
#define HW_CONFIG_REG		0xc0010015
#define HW_CONFIG_CPBDIS	(1 << 25)

/* ACP Device */
#define AMD_PCI_VID		0x1022
#define AMD_FAM17H_ACP_PCI_DID	0x15E2

/* ACP pin control registers */
#define ACP_I2S_PIN_CONFIG              0x1400
#define ACP_PAD_PULLUP_PULLDOWN_CTRL    0x1404

typedef struct {
	SoundRouteComponent component;
	SoundOps ops;
	uint32_t pin_config;
	uint32_t pad_ctrl;
	uint64_t save_config;
} amdI2sSupport;

amdI2sSupport *new_amd_i2s_support(void);

#endif /* __DRIVERS_SOUND_AMD_I2S_SUPPORT_H__ */
