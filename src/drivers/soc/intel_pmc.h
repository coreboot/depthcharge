/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#ifndef __DRIVERS_SOC_INTEL_PMC_H__
#define __DRIVERS_SOC_INTEL_PMC_H__

#include <stdint.h>

#define PMC_IPC_BUF_COUNT		4

#define PMC_IPC_CMD_COMMAND_SHIFT	0
#define PMC_IPC_CMD_COMMAND_MASK	0xff
#define PMC_IPC_CMD_MSI_SHIFT		8
#define PMC_IPC_CMD_MSI_MASK		0x01
#define PMC_IPC_CMD_SUB_COMMAND_SHIFT	12
#define PMC_IPC_CMD_SUB_COMMAND_MASK	0x0f
#define PMC_IPC_CMD_SIZE_SHIFT		16
#define PMC_IPC_CMD_SIZE_MASK		0xff

#define PMC_IPC_CMD_FIELD(name, val) \
	(((val) & PMC_IPC_CMD_##name##_MASK) << PMC_IPC_CMD_##name##_SHIFT)

#define PMC_IPC_CMD_NO_MSI		0

static inline uint32_t pmc_make_ipc_cmd(uint32_t cmd, uint32_t subcmd,
					uint32_t size)
{
	return PMC_IPC_CMD_FIELD(COMMAND, cmd) |
		PMC_IPC_CMD_FIELD(SUB_COMMAND, subcmd) |
		PMC_IPC_CMD_FIELD(MSI, PMC_IPC_CMD_NO_MSI) |
		PMC_IPC_CMD_FIELD(SIZE, size);
}

struct pmc_ipc_buffer {
	uint32_t buf[PMC_IPC_BUF_COUNT];
};

int pmc_send_cmd(uint32_t cmd, const struct pmc_ipc_buffer *wbuf,
		 struct pmc_ipc_buffer *rbuf);

#endif /* __DRIVERS_SOC_INTEL_PMC_H__ */
