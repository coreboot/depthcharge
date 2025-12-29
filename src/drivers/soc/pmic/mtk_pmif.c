// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <assert.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "drivers/soc/pmic/mtk_pmif.h"
#include "drivers/timer/timer.h"

#define GET_SWINF_0_FSM(x)	(((x) >> 1) & 0x7)

#define SWINF_FSM_IDLE		0x00
#define SWINF_FSM_WFVLDCLR	0x06

static int pmif_check_init_done(MtkPmifOps *me)
{
	MtkPmif *pmif = container_of(me, MtkPmif, ops);
	if (!(read32(&pmif->pmif_regs->init_done) & 0x1)) {
		printf("%s: pmif isn't initialized\n", __func__);
		return -1;
	}

	return 0;
}

static int pmif_check_swinf(MtkPmif *pmif, unsigned long timeout_us,
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

static int pmif_send_cmd(MtkPmifOps *me, bool write, u32 opc, u32 slvid,
			 u32 addr, u32 *rdata, u32 wdata, u32 len)
{
	MtkPmif *pmif = container_of(me, MtkPmif, ops);
	int ret;
	u32 data, bc = len - 1;

	/* Wait for Software Interface FSM state to be IDLE. */
	ret = pmif_check_swinf(pmif, PMIF_WAIT_IDLE_US, SWINF_FSM_IDLE);
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
		ret = pmif_check_swinf(pmif, PMIF_READ_US, SWINF_FSM_WFVLDCLR);
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

static u32 pmif_spi_read32(MtkPmifOps *me, u32 slvid, u32 reg)
{
	u32 data = 0;
	pmif_send_cmd(me, false, PMIF_CMD_REG_0, slvid, reg, &data, 0, 1);
	return data;
}

static void pmif_spi_write32(MtkPmifOps *me, u32 slvid, u32 reg, u32 data)
{
	pmif_send_cmd(me, true, PMIF_CMD_REG_0, slvid, reg, NULL, data, 1);
}

static u32 pmif_read_field(MtkPmifOps *me, u32 slvid, u32 reg, u32 mask,
			   u32 shift)
{
	u32 data = pmif_spi_read32(me, slvid, reg);
	data &= (mask << shift);
	data >>= shift;
	return data;
}

static void pmif_write_field(MtkPmifOps *me, u32 slvid, u32 reg, u32 val,
			     u32 mask, u32 shift)
{
	u32 old, new;

	old = pmif_spi_read32(me, slvid, reg);
	new = old & ~(mask << shift);
	new |= (val << shift);
	pmif_spi_write32(me, slvid, reg, new);
}

MtkPmif *new_mtk_pmif_spi(uintptr_t pmif_base, size_t channel_offset)
{
	MtkPmif *pmif = xzalloc(sizeof(*pmif));

	pmif->pmif_regs = (struct mtk_pmif_regs *)pmif_base;
	assert(pmif_base + channel_offset >= pmif_base);
	pmif->chan_regs = (struct mtk_chan_regs *)(pmif_base + channel_offset);

	pmif->ops.check_init_done = pmif_check_init_done;
	pmif->ops.read32 = pmif_spi_read32;
	pmif->ops.write32 = pmif_spi_write32;
	pmif->ops.read_field = pmif_read_field;
	pmif->ops.write_field = pmif_write_field;

	return pmif;
};
