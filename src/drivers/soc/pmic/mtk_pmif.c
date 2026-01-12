// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <assert.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "drivers/soc/pmic/mtk_pmif.h"
#include "drivers/timer/timer.h"

#define GET_SWINF_0_FSM(x)	(((x) >> 1) & 0x7)

#define SWINF_FSM_IDLE		0x00
#define SWINF_FSM_WFVLDCLR	0x06

static int check_init_done(MtkPmifOps *me)
{
	MtkPmif *pmif = container_of(me, MtkPmif, ops);
	if (!(read32(&pmif->pmif_regs->init_done) & 0x1)) {
		printf("%s: pmif isn't initialized\n", __func__);
		return -1;
	}

	return 0;
}

static int check_swinf(MtkPmif *pmif, unsigned long timeout_us,
		       u32 expected_status)
{
	u32 reg_rdata;
	struct stopwatch sw;

	stopwatch_init_usecs_expire(&sw, timeout_us);
	do {
		reg_rdata = read32(&pmif->chan_regs->ch_sta);
		if (stopwatch_expired(&sw))
			return -1;
	} while (GET_SWINF_0_FSM(reg_rdata) != expected_status);

	return 0;
}

/* This is shared for PWRAP and PMIF interfaces. */
static int send_cmd(MtkPmifOps *me, bool write, u32 opc, u32 slvid, u32 bc,
		    u32 addr, u32 *rdata, u32 wdata)
{
	MtkPmif *pmif = container_of(me, MtkPmif, ops);
	int ret;
	u32 data;

	/* Wait for Software Interface FSM state to be IDLE. */
	ret = check_swinf(pmif, PMIF_WAIT_IDLE_US, SWINF_FSM_IDLE);
	if (ret) {
		printf("%s: idle timeout\n", __func__);
		return -1;
	}

	/* Set the write data */
	if (write)
		write32(&pmif->chan_regs->wdata, wdata);

	/* Send the command. */
	write32(&pmif->chan_regs->ch_send,
		(opc << 30) | (write << 29) | (slvid << 24) | (bc << 16) | addr);

	if (!write) {
		/*
		 * Wait for Software Interface FSM state to be WFVLDCLR,
		 * read the data and clear the valid flag.
		 */
		ret = check_swinf(pmif, PMIF_READ_US, SWINF_FSM_WFVLDCLR);
		if (ret) {
			printf("%s: read timeout\n", __func__);
			return -1;
		}

		data = read32(&pmif->chan_regs->rdata);
		*rdata = data;
		write32(&pmif->chan_regs->ch_rdy, 0x1);
	}

	return 0;
}

/* Data type is always u16 for PWRAP interface. */
static int pwrap_send_cmd(MtkPmifOps *me, bool write, u32 addr, u16 *rdata,
			  u16 wdata)
{
	u32 rdata32 = 0;
	/* opc, slvid, bc are not used for PWRAP interface. */
	if (send_cmd(me, write, 0, 0, 0, addr, &rdata32, wdata))
		return -1;
	*rdata = (u16)rdata32;
	assert(*rdata == rdata32);
	return 0;
}

static u16 pwrap_spi_read16(MtkPmifOps *me, u32 slvid, u32 reg)
{
	u16 data = 0;
	pwrap_send_cmd(me, false, reg, &data, 0);
	return data;
}

static void pwrap_spi_write16(MtkPmifOps *me, u32 slvid, u32 reg, u16 data)
{
	pwrap_send_cmd(me, true, reg, NULL, data);
}

static u32 read_field(MtkPmifOps *me, u32 slvid, u32 reg, u32 mask, u32 shift)
{
	/* Only support u16 for now. */
	assert((mask << shift) <= UINT16_MAX);

	u32 data = pwrap_spi_read16(me, slvid, reg);

	data &= (mask << shift);
	data >>= shift;
	return data;
}

static void write_field(MtkPmifOps *me, u32 slvid, u32 reg, u32 val, u32 mask,
			u32 shift)
{
	u32 old, new;

	/* Only support u16 for now. */
	assert((mask << shift) <= UINT16_MAX);

	old = pwrap_spi_read16(me, slvid, reg);
	new = old & ~(mask << shift);
	new |= (val << shift);
	pwrap_spi_write16(me, slvid, reg, new);
}

MtkPmif *new_mtk_pmif_spi(uintptr_t pmif_base, size_t channel_offset)
{
	MtkPmif *pmif = xzalloc(sizeof(*pmif));

	pmif->pmif_regs = (struct mtk_pmif_regs *)pmif_base;
	assert(pmif_base + channel_offset >= pmif_base);
	pmif->chan_regs = (struct mtk_chan_regs *)(pmif_base + channel_offset);

	pmif->ops.check_init_done = check_init_done;
	pmif->ops.read16 = pwrap_spi_read16;
	pmif->ops.write16 = pwrap_spi_write16;
	pmif->ops.read_field = read_field;
	pmif->ops.write_field = write_field;

	return pmif;
};
