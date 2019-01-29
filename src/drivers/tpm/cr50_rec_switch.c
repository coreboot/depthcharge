/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a module which allows retreiving internal tpm state, when
 * supported. It needs to be tightly synchronised with the cr50 TPM codebase
 * in src/third_party/tpm2
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/tpm/cr50_rec_switch.h"
#include "drivers/tpm/tpm.h"
#include "drivers/tpm/tpm_utils.h"

struct tpm_get_rec_btn_msg {
	struct tpm_vendor_header h;
	u8 button_state;
} __attribute__((packed));

/*
 * Return state of recovery button.
 * nonzero = pressed
 * 0       = not pressed
 */
static int cr50_rec_switch_get(GpioOps *me)
{
	TpmOps *tpm = container_of(me, Cr50RecSwitch, ops)->tpm;
	struct tpm_vendor_header req;
	struct tpm_get_rec_btn_msg res;
	size_t buffer_size = sizeof(struct tpm_get_rec_btn_msg);
	u8 recovery_button = 0;

	cr50_fill_vendor_cmd_header(&req, VENDOR_CC_GET_REC_BTN, 0);

	if (tpm->xmit(tpm, (void *)&req, sizeof(struct tpm_vendor_header),
		      (void *)&res, &buffer_size) ||
	    (buffer_size < sizeof(struct tpm_get_rec_btn_msg))) {
		printf("communications error\n");
	} else if(unmarshal_u32(&res.h.code)) {
		printf("TPM error 0x%x\n", unmarshal_u32(&res.h.code));
	} else {
		/* TPM responded as expected. */
		recovery_button = res.button_state;
	}

	return recovery_button;
}

Cr50RecSwitch *new_cr50_rec_switch(TpmOps *tpm)
{
	Cr50RecSwitch *rec_switch = xzalloc(sizeof(*rec_switch));

	rec_switch->tpm = tpm;
	rec_switch->ops.get = cr50_rec_switch_get;

	return rec_switch;
}
