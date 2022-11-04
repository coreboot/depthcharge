// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Intel Corporation.
 * Copyright 2021 Google LLC.
 */

#include <assert.h>
#include <coreboot_tables.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stdint.h>

#include "drivers/ec/cros/ec.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/soc/intel_pmc.h"

#if CONFIG(DRIVER_USB_INTEL_TCSS_DEBUG)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif
#define error(msg, args...)	printf("%s: " msg, __func__, ##args)

/* PMC IPC related offsets and commands */
#define PMC_IPC_USBC_CMD_ID		0xa7
#define PMC_IPC_USBC_SUBCMD_ID		0x0

#define PMC_IPC_CONN_DISC_REQ_SIZE	2

#define IOM_PORT_STATUS_CONNECTED	BIT(31)
#define REGBAR_PID_SHIFT		16

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

static const TcssCtrlr *ctrlr;

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

static const void *port_status_reg(int port)
{
	assert(ctrlr);

	const uintptr_t status_reg = ctrlr->regbar +
		(ctrlr->iom_pid << REGBAR_PID_SHIFT) +
		(ctrlr->iom_status_offset + port * sizeof(uint32_t));
	return (const void *)status_reg;
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

static int get_typec_hsl_sbu_orientation(bool mux_is_inverted, enum type_c_orientation o)
{
	switch (o) {
	case TYPEC_ORIENTATION_REVERSE:
		return 1;
		break;

	case TYPEC_ORIENTATION_NORMAL:
		return 0;
		break;

	case TYPEC_ORIENTATION_NONE:
		return mux_is_inverted ? 1 : 0;
		break;

	default:
		break;
	}

	return 0;
}

/*
 * Query the EC for USB port connection status for each of the type-c ports and
 * update the TCSS for each accordingly.
 */
static int update_all_tcss_ports_states(void)
{
	const struct type_c_port_info *tcss_port_info;
	const struct type_c_info *type_c_info;
	struct pmc_ipc_buffer req = { 0 };
	struct pmc_ipc_buffer res = { 0 };
	enum type_c_orientation hsl;
	enum type_c_orientation sbu;
	unsigned int ec_port;
	unsigned int usb2;
	unsigned int usb3;
	uint8_t mux_state;
	bool usb_enabled;
	int ufp, dbg_acc;
	uint32_t cmd;
	int r = 0;

	type_c_info = (struct type_c_info *)lib_sysinfo.type_c_info;
	tcss_port_info = type_c_info->port_info;

	for (ec_port = 0; ec_port < type_c_info->port_count; ec_port++) {
		r = cros_ec_get_usb_pd_mux_info(ec_port, &mux_state);
		if (r < 0) {
			error("port C%d: get_usb_pd_mux_info failed\n",
					ec_port);
			continue;
		}

		usb_enabled = !!(mux_state & USB_PD_MUX_USB_ENABLED);
		usb3 = tcss_port_info[ec_port].usb3_port_number;

		/*
		 * PMC-IPC encoding of port numbers is 1-based but SoC TypeC
		 * port numbers are 0-based.
		 */
		if (is_port_connected(usb3 - 1) == usb_enabled) {
			/*
			 * The TCSS USB port mux state matches the observed
			 * state of the Type-C USB port.
			 */
			continue;
		}

		usb2 = tcss_port_info[ec_port].usb2_port_number;
		debug("port C%d state: usb enable %d mux conn %d, usb2 %u, "
				"usb3 %u\n", ec_port, usb_enabled,
				is_port_connected(usb3-1), usb2, usb3);

		r = cros_ec_get_usb_pd_control(ec_port, &ufp, &dbg_acc);
		if (r < 0) {
			error("port C%d: pd_control failed\n", ec_port);
			return -1;
		}

		if (usb_enabled) {
			const bool mux_is_inverted = mux_state & USB_PD_MUX_POLARITY_INVERTED;
			hsl = tcss_port_info[ec_port].data_orientation;
			sbu = tcss_port_info[ec_port].sbu_orientation;
			cmd = tcss_make_cmd(
				TCSS_CONN_REQ_RES,
				usb3,
				usb2,
				!!ufp,
				get_typec_hsl_sbu_orientation(mux_is_inverted, hsl),
				get_typec_hsl_sbu_orientation(mux_is_inverted, sbu),
				!!dbg_acc);
		} else {
			cmd = tcss_make_cmd(
				TCSS_DISC_REQ_RES,
				usb3,
				usb2,
				0,
				0,
				0,
				0);
		}
		req.buf[0] = cmd;

		debug("port C%d req: usage %d usb3 %d usb2 %d "
		      "ufp %d ori_hsl %d ori_sbu %d dbg_acc %d\n",
		      ec_port,
		      GET_TCSS_CD_FIELD(USAGE, cmd),
		      GET_TCSS_CD_FIELD(USB3, cmd),
		      GET_TCSS_CD_FIELD(USB2, cmd),
		      GET_TCSS_CD_FIELD(UFP, cmd),
		      GET_TCSS_CD_FIELD(HSL, cmd),
		      GET_TCSS_CD_FIELD(SBU, cmd),
		      GET_TCSS_CD_FIELD(ACC, cmd));

		r = send_conn_disc_msg(&req, &res);
	}

	return r;
}

void soc_usb_mux_poll(void)
{
	static uint64_t last_us = 0;
	uint64_t now_us;

	now_us = timer_us(0);
	if (now_us < last_us + MUX_POLL_PERIOD_US)
		return;
	last_us = now_us;

	update_all_tcss_ports_states();
}

/*
 * This overrides a weak function in the USB subsystem. It is called
 * prior to usb_initialize().
 */
void soc_usb_mux_init(void)
{
	soc_usb_mux_poll();
	mdelay(100);		/* TODO(b/157721366): why is this needed */
}

void register_tcss_ctrlr(const TcssCtrlr *tcss_ctrlr)
{
	ctrlr = tcss_ctrlr;
}
