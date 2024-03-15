// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC.
 *
 * This is a module which allows reading of GSC button states using vendor
 * specific TPM command. It needs to be tightly synchronized with the GSC TPM
 * codebase in src/third_party/tpm2
 */

#include <libpayload.h>

#include "drivers/tpm/google/switches.h"
#include "drivers/tpm/google/tpm.h"
#include "drivers/tpm/tpm.h"

struct tpm_get_btn_msg {
	struct tpm_vendor_header h;
	u8 button_state;
} __attribute__((packed));

/**
 * Read a button/switch state from the GSC.
 *
 * @param tpm              Pointer to the TPM operations structure.
 * @param gsc_subcommand   GSC vendor specific sub-command for the
 *                         button/switch. Supported sub-commands are
 *                         VENDOR_CC_GET_REC_BTN and VENDOR_CC_GET_PWR_BTN.
 * @param button_state     On success, set to the button state read from the
 *                         GSC.
 *
 * @return 0 on success, non-zero if an error is detected.
 */
static int gsc_switch_get(struct TpmOps *tpm, uint16_t gsc_subcommand,
			   int *button_state)
{
	struct tpm_vendor_header req;
	struct tpm_get_btn_msg res;
	size_t buffer_size = sizeof(res);
	int xmit_res;
	uint32_t header_code;

	/* Default switch/button state is zero if the command fails */
	*button_state = 0;

	tpm_google_fill_vendor_cmd_header(&req, gsc_subcommand, 0);

	xmit_res = tpm->xmit(tpm, (void *)&req,
			     sizeof(struct tpm_vendor_header),
			     (void *)&res, &buffer_size);

	/*
	 * The response size varies depending on whether the GSC supports
	 * the vendor command, but should always have a valid header.
	 */
	if (xmit_res || buffer_size < sizeof(struct tpm_vendor_header)) {
		printf("communications error on cmd %u: %d %ld\n",
		       gsc_subcommand, xmit_res, buffer_size);
		return -1;
	}

	header_code = unmarshal_u32(&res.h.code);
	if (header_code == VENDOR_RC_NO_SUCH_COMMAND) {
		/*
		 * During factory setup, the GSC runs very old firmware so
		 * don't treat an unsupported command as an error.
		 */
		printf("Command %u not supported, switch state is off\n",
			gsc_subcommand);
		return 0;
	}

	/* Verify the response contains the payload byte */
	if (buffer_size < sizeof(struct tpm_get_btn_msg)) {
		printf("communications error on cmd %u: response size %ld\n",
		       gsc_subcommand, buffer_size);
		return -1;
	}

	if (header_code) {
		/* Unexpected error */
		printf("TPM error 0x%x\n", unmarshal_u32(&res.h.code));
		return -1;
	}

	/* TPM responded as expected. */
	*button_state = res.button_state;

	return 0;
}

/*
 * Return state of recovery button.
 * 1       = pressed
 * 0       = not pressed
 * -1      = error / unknown
 */
static int gsc_rec_switch_get(GpioOps *me)
{
	TpmOps *tpm = container_of(me, GscSwitch, ops)->tpm;
	int recovery_button = 0;

	/* Errors are logged by gsc_switch_get() */
	if (gsc_switch_get(tpm, VENDOR_CC_GET_REC_BTN, &recovery_button))
		return -1;

	return recovery_button;
}

GscSwitch *new_gsc_rec_switch(TpmOps *tpm)
{
	GscSwitch *rec_switch = xzalloc(sizeof(*rec_switch));

	rec_switch->tpm = tpm;
	rec_switch->ops.get = gsc_rec_switch_get;

	return rec_switch;
}

/*
 * Return state of power button (can also be used as trusted source to indicate
 * user physical presence).
 * 1       = pressed
 * 0       = not pressed
 * -1      = error / unknown
 */
static int gsc_power_switch_get(GpioOps *me)
{
	struct TpmOps *tpm;
	int power_button = 0;

	tpm = container_of(me, GscSwitch, ops)->tpm;

	/* Errors are logged by gsc_switch_get() */
	if (gsc_switch_get(tpm, VENDOR_CC_GET_PWR_BTN, &power_button))
		return -1;

	return power_button;
}

GscSwitch *new_gsc_power_switch(TpmOps *tpm)
{
	GscSwitch *power_switch = xzalloc(sizeof(*power_switch));

	power_switch->tpm = tpm;
	power_switch->ops.get = gsc_power_switch_get;

	return power_switch;
}
