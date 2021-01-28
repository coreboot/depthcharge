// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2020 Google LLC.

#include <libpayload.h>

#include "drivers/soc/qcom_spmi.h"

#define PPID_MASK		(0xfffU << 8)

/* These are opcodes specific to this SPMI arbitrator, *not* SPMI commands. */
#define OPC_EXT_WRITEL		0
#define OPC_EXT_READL		1

#define ARB_STATUS_DONE		BIT(0)
#define ARB_STATUS_FAILURE	BIT(1)
#define ARB_STATUS_DENIED	BIT(2)
#define ARB_STATUS_DROPPED	BIT(3)

#define ERROR_APID_NOT_FOUND	(-(int)BIT(8))
#define ERROR_TIMEOUT		(-(int)BIT(9))

static QcomSpmiRegs *find_apid(QcomSpmi *me, uint32_t addr)
{
	size_t i;

	for (i = 0U; i < me->num_apid; i++) {
		uint32_t reg = read32(&me->apid_map[i]);
		if ((reg != 0U) && ((addr & PPID_MASK) == (reg & PPID_MASK)))
			return &me->regs_per_apid[i];
	}

	return NULL;
}

static int wait_for_done(QcomSpmiRegs *regs)
{
	uint64_t start = timer_us(0);

	while (timer_us(start) < 100 * 1000) {
		uint32_t status = read32(&regs->status);
		if ((status & ARB_STATUS_DONE) != 0U) {
			if ((status & ARB_STATUS_FAILURE) != 0U ||
			    (status & ARB_STATUS_DENIED) != 0U ||
			    (status & ARB_STATUS_DROPPED) != 0U)
				return -(int)(status & 0xff);
			return 0;
		}
	}
	printf("ERROR: SPMI_ARB timeout!\n");
	return ERROR_TIMEOUT;
}

static void arb_command(QcomSpmiRegs *regs, uint8_t opcode, uint32_t addr,
			uint8_t bytes)
{
	write32(&regs->cmd, (uint32_t)opcode << 27 |
			    (addr & 0xff) << 4 | (bytes - 1));
}

int spmi_read8(QcomSpmi *me, uint32_t addr)
{
	struct QcomSpmiRegs *regs = find_apid(me, addr);

	if (!regs)
		return ERROR_APID_NOT_FOUND;

	arb_command(regs, OPC_EXT_READL, addr, 1);

	int ret = wait_for_done(regs);
	if (ret != 0) {
		printf("ERROR: SPMI_ARB read error [0x%x]: 0x%x\n", addr, ret);
		return ret;
	}

	return read32(&regs->rdata0) & 0xff;
}

int spmi_write8(QcomSpmi *me, uint32_t addr, uint8_t data)
{
	struct QcomSpmiRegs *regs = find_apid(me, addr);

	if (!regs)
		return ERROR_APID_NOT_FOUND;

	write32(&regs->wdata0, data);
	arb_command(regs, OPC_EXT_WRITEL, addr, 1);

	int ret = wait_for_done(regs);
	if (ret != 0) {
		printf("ERROR: SPMI_ARB write error [0x%x] = 0x%x: 0x%x\n",
		       addr, data, ret);
	}

	return ret;
}

QcomSpmi *new_qcom_spmi(uintptr_t base, uintptr_t apid_map, size_t map_bytes)
{
	QcomSpmi *spmi = xzalloc(sizeof(*spmi));
	spmi->regs_per_apid = (void *)base;
	spmi->apid_map = (void *)apid_map;
	spmi->num_apid = map_bytes / sizeof(spmi->apid_map[0]);
	spmi->read8 = spmi_read8;
	spmi->write8 = spmi_write8;
	return spmi;
}
