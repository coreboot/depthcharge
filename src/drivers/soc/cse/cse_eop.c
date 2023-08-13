/* SPDX-License-Identifier: GPL-2.0-only */

#include "base/cleanup_funcs.h"
#include "base/late_init_funcs.h"
#include "base/init_funcs.h"
#include "cse_internal.h"
#include "cse.h"

#define CSE_MAX_RETRY_CMD	3

enum cse_cmd_result {
	CSE_CMD_RESULT_GLOBAL_RESET_REQUESTED,
	CSE_CMD_RESULT_SUCCESS,
	CSE_CMD_RESULT_ERROR,
	CSE_CMD_RESULT_DISABLED,
	CSE_CMD_RESULT_RETRY,
};

static enum cse_cmd_result decode_heci_send_receive_error(enum cse_tx_rx_status ret)
{
	switch (ret) {
	case CSE_TX_ERR_CSE_NOT_READY:
	case CSE_RX_ERR_CSE_NOT_READY:
	case CSE_RX_ERR_RESP_LEN_MISMATCH:
	case CSE_RX_ERR_TIMEOUT:
		return CSE_CMD_RESULT_RETRY;
	default:
		return CSE_CMD_RESULT_ERROR;
	}
}

static enum cse_cmd_result cse_disable_mei_bus(void)
{
	struct bus_disable_message {
		uint8_t command;
		uint8_t reserved[3];
	} __packed msg = {
		.command = MEI_BUS_DISABLE_COMMAND,
	};
	struct bus_disable_resp {
		uint8_t command;
		uint8_t status;
		uint8_t reserved[2];
	} __packed reply = {};

	size_t reply_sz = sizeof(reply);
	enum cse_tx_rx_status ret;

	printk(BIOS_DEBUG, "HECI, Sending MEI BIOS DISABLE command\n");
	ret = heci_send_receive(&msg, sizeof(msg), &reply, &reply_sz, HECI_MEI_ADDR);

	if (ret) {
		printk(BIOS_ERR, "HECI: Failed to Disable MEI bus\n");
		return decode_heci_send_receive_error(ret);
	}

	if (reply.status) {
		printk(BIOS_ERR, "HECI: MEI_Bus_Disable Failed (status: %d)\n", reply.status);
		return CSE_CMD_RESULT_ERROR;
	}

	return CSE_CMD_RESULT_SUCCESS;
}

static enum cse_cmd_result cse_receive_eop(void)
{
	enum {
		EOP_REQUESTED_ACTION_CONTINUE = 0,
		EOP_REQUESTED_ACTION_GLOBAL_RESET = 1,
	};
	enum cse_tx_rx_status ret;
	struct end_of_post_resp {
		struct mkhi_hdr hdr;
		uint32_t requested_actions;
	} __packed resp = {};
	size_t resp_size = sizeof(resp);

	ret = heci_receive(&resp, &resp_size);
	if (ret)
		return decode_heci_send_receive_error(ret);

	if (resp.hdr.group_id != MKHI_GROUP_ID_GEN ||
	    resp.hdr.command != MKHI_END_OF_POST) {
		printk(BIOS_ERR, "HECI: EOP Unexpected response group or command.\n");
		return CSE_CMD_RESULT_ERROR;
	}

	if (resp.hdr.result) {
		printk(BIOS_ERR, "HECI: EOP Resp Failed: %u\n", resp.hdr.result);
		return CSE_CMD_RESULT_ERROR;
	}

	printk(BIOS_INFO, "CSE: EOP requested action: ");

	switch (resp.requested_actions) {
	case EOP_REQUESTED_ACTION_GLOBAL_RESET:
		printk(BIOS_INFO, "global reset\n");
		return CSE_CMD_RESULT_GLOBAL_RESET_REQUESTED;
	case EOP_REQUESTED_ACTION_CONTINUE:
		printk(BIOS_INFO, "continue boot\n");
		return CSE_CMD_RESULT_SUCCESS;
	default:
		printk(BIOS_INFO, "unknown %u\n", resp.requested_actions);
		return CSE_CMD_RESULT_ERROR;
	}
}

static enum cse_cmd_result cse_send_eop(void)
{
	enum cse_tx_rx_status ret;
	struct end_of_post_msg {
		struct mkhi_hdr hdr;
	} __packed msg = {
		.hdr = {
			.group_id = MKHI_GROUP_ID_GEN,
			.command = MKHI_END_OF_POST,
		},
	};

	/*
	 * Prerequisites:
	 * 1) HFSTS1 CWS is Normal
	 * 2) HFSTS1 COM is Normal
	 * 3) Only sent after DID (accomplished by compiling this into ramstage)
	 */

	if (cse_is_hfs1_com_soft_temp_disable()) {
		printk(BIOS_ERR, "HECI: Prerequisites not met for sending EOP\n");
		if (CONFIG(SOC_INTEL_CSE_LITE_SKU))
			return CSE_CMD_RESULT_ERROR;
		return CSE_CMD_RESULT_DISABLED;
	}

	if (!cse_is_hfs1_cws_normal() || !cse_is_hfs1_com_normal()) {
		printk(BIOS_ERR, "HECI: Prerequisites not met for sending EOP\n");
		return CSE_CMD_RESULT_ERROR;
	}

	printk(BIOS_INFO, "HECI: Sending End-of-Post\n");

	ret = heci_send(&msg, sizeof(msg), BIOS_HOST_ADDR, HECI_MKHI_ADDR);
	if (ret)
		return decode_heci_send_receive_error(ret);

	return CSE_CMD_RESULT_SUCCESS;
}

static enum cse_cmd_result cse_send_and_receive_eop(void)
{
	enum cse_cmd_result ret;

	ret = cse_send_eop();
	if (ret != CSE_CMD_RESULT_SUCCESS)
		return ret;

	return cse_receive_eop();
}

static enum cse_cmd_result cse_send_cmd_retries(enum cse_cmd_result (*cse_send_command)(void))
{
	size_t retry;
	enum cse_cmd_result ret;
	for (retry = 0; retry < CSE_MAX_RETRY_CMD; retry++) {
		ret = cse_send_command();
		if (ret != CSE_CMD_RESULT_RETRY)
			break;
	}
	return ret;
}

/*
 * On EOP error, the BIOS is required to send an MEI bus disable message to the
 * CSE, followed by disabling all MEI devices. After successfully completing
 * this, it is safe to boot.
 */
static void cse_handle_eop_error(void)
{
	if (cse_send_cmd_retries(cse_disable_mei_bus))
		die("Failed to disable MEI bus while recovering from EOP error\n"
		    "Preventing system from booting into an insecure state.\n");

	if (!cse_disable_mei_devices())
		die("Error disabling MEI devices while recovering from EOP error\n"
		    "Preventing system from booting into an insecure state.\n");
}

static void handle_cse_eop_result(enum cse_cmd_result result)
{
	switch (result) {
	case CSE_CMD_RESULT_GLOBAL_RESET_REQUESTED:
		printk(BIOS_INFO, "CSE requested global reset in EOP response, resetting...\n");
		do_global_reset();
		break;
	case CSE_CMD_RESULT_SUCCESS:
		printk(BIOS_INFO, "CSE EOP successful, continuing boot\n");
		break;
	case CSE_CMD_RESULT_DISABLED:
		printk(BIOS_INFO, "CSE is disabled, continuing boot\n");
		break;
	case CSE_CMD_RESULT_ERROR: /* fallthrough */
	default:
		printk(BIOS_ERR, "Failed to send EOP to CSE, %d\n", result);
		/* For vboot, trigger recovery mode if applicable, as there is
		   likely something very broken in this case. */
		if (!vboot_in_recovery())
			cse_trigger_vboot_recovery(CSE_EOP_FAIL);

		/* In non-vboot builds or recovery mode, follow the BWG in order
		   to continue to boot securely. */
		cse_handle_eop_error();
		break;
	}
}

static void do_send_end_of_post(bool wait_for_completion)
{
	static bool eop_sent = false, eop_complete = false;
	enum cse_cmd_result ret;

	if (eop_complete) {
		printk(BIOS_WARNING, "EOP already sent\n");
		return;
	}

	if (acpi_get_sleep_type_s3()) {
		printk(BIOS_INFO, "Skip sending EOP during S3 resume\n");
		return;
	}

	/*
	 * If CSE is already hidden then accessing CSE registers would be wrong and will
	 * receive junk, hence, return as CSE is already disabled.
	 */
	if (!is_cse_enabled()) {
		printk(BIOS_DEBUG, "CSE is disabled, cannot send End-of-Post (EOP) message\n");
		return;
	}

	if (!eop_sent) {
		set_cse_device_state(PCH_DEVFN_CSE, DEV_ACTIVE);
		timestamp_add_now(TS_ME_END_OF_POST_START);
		ret = cse_send_cmd_retries(cse_send_eop);
		if (ret == CSE_CMD_RESULT_SUCCESS)
			eop_sent = true;
	}

	if (!wait_for_completion)
		return;

	set_cse_device_state(PCH_DEVFN_CSE, DEV_ACTIVE);
	ret = cse_receive_eop();
	if (ret != CSE_CMD_RESULT_SUCCESS) {
		ret = cse_send_cmd_retries(cse_send_and_receive_eop);
		handle_cse_eop_result(ret);
	}
	timestamp_add_now(TS_ME_END_OF_POST_END);

	set_cse_device_state(PCH_DEVFN_CSE, DEV_IDLE);

	eop_complete = true;
}

/*
 * Don't send EOP if the following conditions are met:
 * CSE Lite:
 * 1. "The platform is running CSE-Lite SKU" AND
 * 2. 'The CSE is running the RO FW" AND
 * 3. "The board is in recovery mode"
 *
 * Other CSE Type:
 * 1. "The board is in recovery mode"
 *
 * The above conditions summarize that the CSE is in "SOFT TEMP DISABLE" state,
 * hence, don't send the EOP command to CSE.
 */
static bool is_cse_eop_supported(void)
{
	/* CSE Lite */
	if ((CONFIG(SOC_INTEL_CSE_LITE_SKU) && vboot_in_recovery()) &&
		cse_is_hfs1_com_soft_temp_disable()) {
		printk(BIOS_INFO, "HECI: coreboot in recovery mode; found CSE Lite in expected "
		       "SOFT TEMP DISABLE state, skipping EOP\n");
		return false;
	}
	/* Other CSE Type */
	if (cse_is_hfs1_com_soft_temp_disable()) {
		printk(BIOS_INFO, "HECI: coreboot in recovery mode; found CSE in expected "
		       "SOFT TEMP DISABLE state, skipping EOP\n");
		return false;
	}

	return true;
}

static void send_cse_eop_async(void *unused)
{
	if (is_cse_eop_supported())
		do_send_end_of_post(false);
}

static void send_cse_eop_with_late_finalize(void *unused)
{
	if (is_cse_eop_supported())
		do_send_end_of_post(true);

	cse_late_finalize();
}

static int intel_cse_send_eop_async(struct LateInitFunc *init)
{
	if (CONFIG(SOC_INTEL_CSE_SEND_EOP_BY_PAYLOAD))
		send_cse_eop_async(NULL);
	return 0;
}

LATE_INIT_FUNC(intel_cse_send_eop_async);

static int intel_cse_finalize(struct CleanupFunc *cleanup, CleanupType type)
{
	if (CONFIG(SOC_INTEL_CSE_SEND_EOP_BY_PAYLOAD))
		send_cse_eop_with_late_finalize(NULL);
	return 0;
}

static int intel_cse_finalize_install(void)
{
	static CleanupFunc dev =
	{
		&intel_cse_finalize,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(intel_cse_finalize_install);

