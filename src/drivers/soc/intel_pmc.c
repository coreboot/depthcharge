// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#include <libpayload.h>

#include "drivers/soc/intel_pmc.h"
#include "drivers/timer/timer.h"

#if CONFIG(DRIVER_SOC_INTEL_PMC_DEBUG)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif
#define error(msg, args...)	printf("%s: " msg, __func__, ##args)

/*
 * From Intel 500 Series PCH EDS vol2 s4.4.
 */
#define IPC_CMD_OFFSET			0x0
#define IPC_STS_OFFSET			0x4
#define IPC_STS_BUSY			BIT(0)
#define IPC_STS_ERROR			BIT(1)
#define IPC_STS_ERROR_CODE_SHIFT	16
#define IPC_STS_ERROR_CODE_MASK		0xff

#define IS_IPC_STS_BUSY(status)		((status) & IPC_STS_BUSY)
#define IPC_STS_HAS_ERROR(status)	((status) & IPC_STS_ERROR)
#define IPC_STS_ERROR_CODE(sts)	(((sts) >> IPC_STS_ERROR_CODE_SHIFT) & \
				 IPC_STS_ERROR_CODE_MASK)

/*
 * WBUF register block offset 0x80..0x8f
 * there are 4 consecutive 32 bit registers
 */
#define IPC_WBUF0			0x80

/*
 * RBUF register block offset 0x90..0x9f
 * there are 4 consecutive 32 bit registers
 */
#define IPC_RBUF0			0x90

#define IPC_XFER_TIMEOUT_MS		1000 /* max 1s */

#define PCH_PWRM_BASE_ADDRESS		0xfe000000

static void *pmc_reg(unsigned int pmc_reg_offset)
{
	const uintptr_t pmcbase = PCH_PWRM_BASE_ADDRESS;

	return (void *)(pmcbase + pmc_reg_offset);
}

static const void *pmc_rbuf(unsigned int ix)
{
	return pmc_reg(IPC_RBUF0 + ix * sizeof(uint32_t));
}

static void *pmc_wbuf(unsigned int ix)
{
	return pmc_reg(IPC_WBUF0 + ix * sizeof(uint32_t));
}

static int pmc_check_ipc_sts(void)
{
	struct stopwatch sw;
	uint32_t ipc_sts;

	stopwatch_init_msecs_expire(&sw, IPC_XFER_TIMEOUT_MS);
	do {
		ipc_sts = read32(pmc_reg(IPC_STS_OFFSET));
		if (!IS_IPC_STS_BUSY(ipc_sts)) {
			debug("STS_BUSY done after %llu us\n",
			      (uint64_t)timer_us(sw.start));
			if (IPC_STS_HAS_ERROR(ipc_sts)) {
				error("IPC_STS.error_code 0x%x\n",
				      IPC_STS_ERROR_CODE(ipc_sts));
				return -1;
			}
			return 0;
		}
		udelay(50);
	} while (!stopwatch_expired(&sw));

	error("PMC IPC timeout after %u ms\n", IPC_XFER_TIMEOUT_MS);
	return -1;
}

int pmc_send_cmd(uint32_t cmd, const struct pmc_ipc_buffer *wbuf,
		 struct pmc_ipc_buffer *rbuf)
{
	for (int i = 0; i < PMC_IPC_BUF_COUNT; ++i)
		write32(pmc_wbuf(i), wbuf->buf[i]);

	write32(pmc_reg(IPC_CMD_OFFSET), cmd);

	if (pmc_check_ipc_sts()) {
		error("PMC IPC command 0x%x failed\n", cmd);
		return -1;
	}

	for (int i = 0; i < PMC_IPC_BUF_COUNT; ++i)
		rbuf->buf[i] = read32(pmc_rbuf(i));

	return 0;
}
