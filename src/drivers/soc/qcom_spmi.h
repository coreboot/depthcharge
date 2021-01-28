/* SPDX-License-Identifier: GPL-2.0-or-later */
// Copyright 2020 Google LLC.

#ifndef __DRIVERS_SOC_QCOM_SPMI_H__
#define __DRIVERS_SOC_QCOM_SPMI_H__

#include <stdint.h>

// Individual register block per APID
typedef struct QcomSpmiRegs {
	uint32_t cmd;
	uint32_t config;
	uint32_t status;
	uint32_t _reserved0;
	uint32_t wdata0;
	uint32_t wdata1;
	uint32_t rdata0;
	uint32_t rdata1;
	uint8_t _reserved_empty_until_next_apid[0x10000 - 0x20];
} QcomSpmiRegs;
check_member(QcomSpmiRegs, rdata1, 0x1c);
_Static_assert(sizeof(QcomSpmiRegs) == 0x10000,
	       "QcomSpmiRegs must be 0x10000 bytes per APID");

typedef struct QcomSpmi {
	struct QcomSpmiRegs *regs_per_apid;	// indexed by APID
	uint32_t *apid_map;
	size_t num_apid;
	int (*read8)(struct QcomSpmi *me, uint32_t addr);
	int (*write8)(struct QcomSpmi *me, uint32_t addr, uint8_t data);
} QcomSpmi;

QcomSpmi *new_qcom_spmi(uintptr_t base, uintptr_t apid_map, size_t map_bytes);

#endif	// __DRIVERS_SOC_QCOM_SPMI_H__
