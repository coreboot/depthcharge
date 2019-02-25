// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC.
 *
 * This is a module which allows reading of Cr50 button states using vendor
 * specific TPM command. It needs to be tightly synchronized with the cr50 TPM
 * codebase in src/third_party/tpm2
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/tpm/cr50_switches.h"
#include "drivers/tpm/tpm.h"
#include "drivers/tpm/tpm_utils.h"

struct tpm_get_btn_msg {
	struct tpm_vendor_header h;
	u8 button_state;
} __attribute__((packed));

/**
 * Read a button/switch state from the Cr50.
 *
 * @param tpm              Pointer to the TPM operations structure.
 * @param cr50_subcommand  Cr50 vendor specific sub-command for the
 *                         button/switch. Supported sub-commands are
 *                         VENDOR_CC_GET_REC_BTN and VENDOR_CC_GET_PWR_BTN.
 * @param button_state     On success, set to the button state read from the
 *                         Cr50.
 *
 * @return 0 on success, non-zero if an error is detected.
 */
static int cr50_switch_get(struct TpmOps *tpm, uint16_t cr50_subcommand,
			   int *button_state)
{
	struct tpm_vendor_header req;
	struct tpm_get_btn_msg res;
	size_t buffer_size = sizeof(res);
	int xmit_res;
	*button_state = 0;

	cr50_fill_vendor_cmd_header(&req, cr50_subcommand, 0);

	xmit_res = tpm->xmit(tpm, (void *)&req,
			     sizeof(struct tpm_vendor_header),
			     (void *)&res, &buffer_size);
	if (xmit_res || buffer_size < sizeof(struct tpm_get_btn_msg)) {
		printf("communications error on cmd %u: %d %ld\n",
		       cr50_subcommand, xmit_res, buffer_size);
		return -1;
	} else if(unmarshal_u32(&res.h.code)) {
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
static int cr50_rec_switch_get(GpioOps *me)
{
	TpmOps *tpm = container_of(me, Cr50Switch, ops)->tpm;
	int recovery_button = 0;

	/* Errors are logged by cr50_switch_get() */
	if (cr50_switch_get(tpm, VENDOR_CC_GET_REC_BTN, &recovery_button))
		return -1;

	return recovery_button;
}

Cr50Switch *new_cr50_rec_switch(TpmOps *tpm)
{
	Cr50Switch *rec_switch = xzalloc(sizeof(*rec_switch));

	rec_switch->tpm = tpm;
	rec_switch->ops.get = cr50_rec_switch_get;

	return rec_switch;
}

/*
 * Return state of power button (can also be used as trusted source to indicate
 * user physical presence).
 * 1       = pressed
 * 0       = not pressed
 * -1      = error / unknown
 */
static int cr50_power_switch_get(GpioOps *me)
{
	struct TpmOps *tpm;
	int power_button = 0;

	tpm = container_of(me, Cr50Switch, ops)->tpm;

	/* Errors are logged by cr50_switch_get() */
	if (cr50_switch_get(tpm, VENDOR_CC_GET_PWR_BTN, &power_button))
		return -1;

	return power_button;
}

Cr50Switch *new_cr50_power_switch(TpmOps *tpm)
{
	Cr50Switch *power_switch = xzalloc(sizeof(*power_switch));

	power_switch->tpm = tpm;
	power_switch->ops.get = cr50_power_switch_get;

	return power_switch;
}
