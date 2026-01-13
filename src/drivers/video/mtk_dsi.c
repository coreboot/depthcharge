// SPDX-License-Identifier: GPL-2.0-only OR MIT

#include <libpayload.h>

#include "mtk_dsi.h"

static void mtk_dsi_start(struct mtk_dsi *dsi)
{
	/*
	 * Only start primary dsi0 for single or dual channel mode.
	 * The secondary dsi1 is dual-enabled and does not require a separate start.
	 * Starting only the primary dsi0 ensures correct synchronization
	 * with secondary dsi1 if there is.
	 */
	if (dsi->reg_base[0]) {
		write32p(dsi->reg_base[0] + DSI_START, 0);
		write32p(dsi->reg_base[0] + DSI_START, DSI_START_FLD_DSI_START);
	}
}

static void mtk_dsi_stop(struct mtk_dsi *dsi)
{
	/*
	 * Only stop primary dsi0 for single or dual channel mode.
	 * The secondary dsi1 is dual-enabled and does not require a separate stop.
	 * Stopping only the primary dsi0 ensures correct synchronization
	 * with secondary dsi1 if there is.
	 */
	if (dsi->reg_base[0])
		write32p(dsi->reg_base[0] + DSI_START, 0);
}

static void mtk_dsi_poll_idle(uintptr_t base, int idx, uint64_t timeout_us)
{
	uint64_t start = timer_us(0);
	while (read32p(base + DSI_INTSTA) & DSI_BUSY) {
		if (timer_us(start) > timeout_us) {
			printf("DSI[%d] wait for idle timeout!\n", idx);
			break;
		}
	}
}

static void mtk_dsi_wait_for_idle(struct mtk_dsi *dsi)
{
	const uint64_t timeout_us = 2 * USECS_PER_SEC;

	for (int i = 0; i < ARRAY_SIZE(dsi->reg_base); i++) {
		uintptr_t base = dsi->reg_base[i];
		if (!base)
			break;

		mtk_dsi_poll_idle(base, i, timeout_us);
	}
}

bool mtk_dsi_is_enabled(struct mtk_dsi *dsi)
{
	bool enabled;
	uint32_t dsi_start;

	if (!dsi->reg_base[0]) {
		printf("%s: reg_base[0] is NULL, DSI disabled\n", __func__);
		return false;
	}

	dsi_start = read32p(dsi->reg_base[0] + DSI_START);
	enabled = dsi_start & DSI_START_FLD_DSI_START;
	printf("%s: DSI is %s (DSI_START=0x%x)\n", __func__,
	       enabled ? "ENABLED" : "DISABLED", dsi_start);
	return enabled;
}

static void mtk_dsi_switch_to_command_mode(struct mtk_dsi *dsi)
{
	mtk_dsi_stop(dsi);
	mtk_dsi_wait_for_idle(dsi);

	for (int i = 0; i < ARRAY_SIZE(dsi->reg_base); i++) {
		uintptr_t base = dsi->reg_base[i];
		if (!base)
			break;

		write32p(dsi->reg_base[i] + DSI_MODE_CTRL, CMD_MODE);
	}
}

static bool mtk_dsi_is_read_command(enum mipi_dsi_transaction type)
{
	switch (type) {
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		return true;
	default:
		return false;
	}
}

static enum cb_err dsi_cmdq(uintptr_t base, int idx, enum mipi_dsi_transaction type,
			    const u8 *data, u8 len)
{
	write32p(base + DSI_INTSTA, 0);

	u32 config;
	if (mtk_dsi_is_read_command(type))
		config = BTA;
	else
		config = (len > 2) ? LONG_PACKET : SHORT_PACKET;

	u32 prefix = config | type << 8;
	int prefsz = 2;
	if (len > 2) {
		prefix |= len << 16;
		prefsz += 2;
	}

	u32 *dsi_cmdq = (u32 *)(base + DSI_CMDQ);
	buffer_to_fifo32_prefix(data, prefix, prefsz, prefsz + len, dsi_cmdq,
				4, 4);
	write32p(base + DSI_CMDQ_SIZE, DIV_ROUND_UP(prefsz + len, 4) | CMDQ_SIZE_SEL);

	return CB_SUCCESS;
}

static enum cb_err mtk_dsi_cmdq(enum mipi_dsi_transaction type, const u8 *data, u8 len,
				void *user_data)
{
	struct mtk_dsi *dsi = (struct mtk_dsi *)user_data;
	size_t channels = ARRAY_SIZE(dsi->reg_base);
	if (!lib_sysinfo.framebuffer.flags.has_dual_pipe)
		channels = 1;

	for (int i = 0; i < channels; i++) {
		uintptr_t base = dsi->reg_base[i];
		if (!base)
			break;

		enum cb_err ret = dsi_cmdq(base, i, type, data, len);
		if (ret != CB_SUCCESS)
			return ret;
	}

	mtk_dsi_start(dsi);
	mtk_dsi_wait_for_idle(dsi);

	return CB_SUCCESS;
}

void mtk_dsi_poweroff(struct mtk_dsi *dsi)
{
	const struct cb_panel_poweroff *panel_poweroff =
		(struct cb_panel_poweroff *)phys_to_virt(lib_sysinfo.cb_panel_poweroff);

	if (panel_poweroff) {
		mtk_dsi_switch_to_command_mode(dsi);
		mipi_panel_parse_commands(panel_poweroff->cmd, mtk_dsi_cmdq, dsi);
	}

	/* To make output LP11 state */
	mtk_dsi_stop(dsi);
}
