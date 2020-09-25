// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */

#include <assert.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stdint.h>

#include "drivers/ec/cros/ec.h"
#include "drivers/bus/usb/tigerlake_tcss.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/soc/tigerlake_pmc.h"

#if CONFIG(DRIVER_USB_TIGERLAKE_TCSS_DEBUG)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif
#define error(msg, args...)	printf("%s: " msg, __func__, ##args)

/* PMC IPC related offsets and commands */
#define PMC_IPC_USBC_CMD_ID		0xa7
#define PMC_IPC_USBC_SUBCMD_ID		0x0

#define PMC_IPC_CONN_DISC_REQ_SIZE	2

/*
 * From TGL EDS vol2a s3.6.
 */
#define IOM_BASE_ADDRESS		0xfbc10000
#define IOM_PORT_STATUS_OFFSET		0x0560
#define IOM_REG_LEN			4	/* Register length in bytes */
#define IOM_PORT_STATUS_CONNECTED	BIT(31)

#define TCSS_CONN_REQ_RES		0
#define TCSS_DISC_REQ_RES		1

/*
 * Polling the MUX significantly slows down depthcharge when something
 * is connected, so dampen the polls.
 */
#define MUX_POLL_PERIOD_US		100000	/* 100ms */

#define TCSS_CD_USAGE_SHIFT		0
#define TCSS_CD_USAGE_MASK		0x0f
#define TCSS_CD_USB3_SHIFT		4
#define TCSS_CD_USB3_MASK		0x0f
#define TCSS_CD_USB2_SHIFT		8
#define TCSS_CD_USB2_MASK		0x0f
#define TCSS_CD_UFP_SHIFT		12
#define TCSS_CD_UFP_MASK		0x01
#define TCSS_CD_HSL_SHIFT		13
#define TCSS_CD_HSL_MASK		0x01
#define TCSS_CD_SBU_SHIFT		14
#define TCSS_CD_SBU_MASK		0x01
#define TCSS_CD_ACC_SHIFT		15
#define TCSS_CD_ACC_MASK		0x01
#define TCSS_CD_FAILED_SHIFT		16
#define TCSS_CD_FAILED_MASK		0x01
#define TCSS_CD_FATAL_SHIFT		17
#define TCSS_CD_FATAL_MASK		0x01

#define TCSS_CD_FIELD(name, val) \
	(((val) & TCSS_CD_##name##_MASK) << TCSS_CD_##name##_SHIFT)

#define GET_TCSS_CD_FIELD(name, val) \
	(((val) >> TCSS_CD_##name##_SHIFT) & TCSS_CD_##name##_MASK)

#define TCSS_STATUS_HAS_FAILED(s)	GET_TCSS_CD_FIELD(FAILED, s)
/* !fatal means retry */
#define TCSS_STATUS_IS_FATAL(s)		GET_TCSS_CD_FIELD(FATAL, s)

static uint32_t tcss_make_cmd(int u, int u3, int u2, int ufp, int hsl, int sbu,
			      int acc)
{
	return TCSS_CD_FIELD(USAGE, u) |
		TCSS_CD_FIELD(USB3, u3) |
		TCSS_CD_FIELD(USB2, u2) |
		TCSS_CD_FIELD(UFP, ufp) |
		TCSS_CD_FIELD(HSL, hsl) |
		TCSS_CD_FIELD(SBU, sbu) |
		TCSS_CD_FIELD(ACC, acc);
}

static int tcss_port_count;
static const struct tcss_port_map *tcss_port_map;

static void *iom_reg(unsigned int iom_reg_offset)
{
	const uintptr_t iombase = IOM_BASE_ADDRESS;

	return (void *)(iombase + iom_reg_offset);
}

static const void *port_status_reg(int port)
{
	uintptr_t sts_offset;

	sts_offset = IOM_PORT_STATUS_OFFSET + IOM_REG_LEN * port;
	return (const void *)iom_reg(sts_offset);
}

static bool is_port_connected(int port)
{
	uint32_t sts;

	sts = read32(port_status_reg(port));
	return !!(sts & IOM_PORT_STATUS_CONNECTED);
}

static int send_conn_disc_msg(const struct pmc_ipc_buffer *req,
			  struct pmc_ipc_buffer *res)
{
	uint32_t cmd_reg;
	uint32_t res_reg;
	int tries = 2;
	int r;

	cmd_reg = pmc_make_ipc_cmd(PMC_IPC_USBC_CMD_ID, PMC_IPC_USBC_SUBCMD_ID,
				   PMC_IPC_CONN_DISC_REQ_SIZE);

	do {
		r = pmc_send_cmd(cmd_reg, req, res);
		if (r < 0) {
			error("pmc_send_cmd failed\n");
			return -1;
		}
		res_reg = res->buf[0];

		if (!TCSS_STATUS_HAS_FAILED(res_reg)) {
			debug("pmc_send_cmd succeeded\n");
			return 0;
		}

		if (TCSS_STATUS_IS_FATAL(res_reg)) {
			error("pmc_send_cmd status: fatal\n");
			return -1;
		}
	} while (--tries >= 0);

	error("pmc_send_cmd failed after retries\n");
	return -1;
}

/*
 * Query the EC for USB port connection status and update the TCSS
 * accordingly.
 */
static int update_port_state(int port)
{
	uint8_t mux_state;
	int ufp, dbg_acc;
	int r;
	bool usb_enabled;
	const struct tcss_port_map *map;

	assert(tcss_port_map);

	struct pmc_ipc_buffer req = { 0 };
	struct pmc_ipc_buffer res = { 0 };
	uint32_t cmd;

	r = cros_ec_get_usb_pd_mux_info(port, &mux_state);
	if (r < 0) {
		error("port C%d: get_usb_pd_mux_info failed\n", port);
		return -1;
	}

	usb_enabled = !!(mux_state & USB_PD_MUX_USB_ENABLED);
	map = &tcss_port_map[port];
	if (is_port_connected(map->usb3) == usb_enabled) {
		/*
		 * The TCSS USB port mux state matches the observed
		 * state of the Type-C USB port.
		 */
		return 0;
	}

	debug("port C%d state: usb enable %d mux conn %d\n",
	      port, usb_enabled, is_port_connected(map->usb3));

	r = cros_ec_get_usb_pd_control(port, &ufp, &dbg_acc);
	if (r < 0) {
		error("port C%d: pd_control failed\n", port);
		return -1;
	}

	/*
	 * PMC-IPC encoding of port numbers is 1-based but SoC TypeC
	 * port numbers are 0-based.
	 *
	 * TODO(b/149830546): Treat SBU and HSL orientation independently.
	 */
	if (usb_enabled) {
		cmd = tcss_make_cmd(
			TCSS_CONN_REQ_RES,
			map->usb3 + 1,
			map->usb2,
			!!ufp,
			!!(mux_state & USB_PD_MUX_POLARITY_INVERTED),
			!!(mux_state & USB_PD_MUX_POLARITY_INVERTED),
			!!dbg_acc);
	} else {
		cmd = tcss_make_cmd(
			TCSS_DISC_REQ_RES,
			map->usb3 + 1,
			map->usb2,
			0,
			0,
			0,
			0);
	}
	req.buf[0] = cmd;

	debug("port C%d req: usage %d usb3 %d usb2 %d "
	      "ufp %d ori_hsl %d ori_sbu %d dbg_acc %d\n",
	      port,
	      GET_TCSS_CD_FIELD(USAGE, cmd),
	      GET_TCSS_CD_FIELD(USB3, cmd),
	      GET_TCSS_CD_FIELD(USB2, cmd),
	      GET_TCSS_CD_FIELD(UFP, cmd),
	      GET_TCSS_CD_FIELD(HSL, cmd),
	      GET_TCSS_CD_FIELD(SBU, cmd),
	      GET_TCSS_CD_FIELD(ACC, cmd));

	return send_conn_disc_msg(&req, &res);
}

static void soc_usb_mux_poll(void)
{
	static uint64_t last_us = 0;
	uint64_t now_us;
	int i;

	now_us = timer_us(0);
	if (now_us < last_us + MUX_POLL_PERIOD_US)
		return;
	last_us = now_us;
	for (i = 0; i < tcss_port_count; i++)
		update_port_state(i);
}

/*
 * This overrides a weak function in the USB subsystem. It is called
 * prior to usb_initialize().
 */
void soc_usb_mux_init(void)
{
	tcss_port_count = board_tcss_get_port_mapping(&tcss_port_map);

	soc_usb_mux_poll();
	mdelay(100);		/* TODO(b/157721366): why is this needed */
}

/*
 * This overrides a weak function in libpayload. It is called at the
 * very beginning of usb_poll().
 */
void usb_poll_prepare(void)
{
	soc_usb_mux_poll();
}
