// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <tss_constants.h>

#include "drivers/tpm/google/tpm.h"
#include "drivers/tpm/tpm.h"

void tpm_google_fill_vendor_cmd_header(struct tpm_vendor_header *h,
				       u16 subcommand, size_t content_size)
{
	marshal_u16(&h->tag, TPM_ST_NO_SESSIONS);
	marshal_u32(&h->size, sizeof(struct tpm_vendor_header) + content_size);
	marshal_u32(&h->code, TPM_CC_VENDOR_BIT);
	marshal_u16(&h->subcommand_code, subcommand);
}

int tpm_google_set_tpm_mode(struct TpmOps *me, uint8_t mode_val)
{
	struct tpm_vendor_header *h;
	uint8_t *m;
	uint8_t buffer[sizeof(*h) + sizeof(*m)];
	uint32_t header_code;
	size_t recv_len = sizeof(buffer);

	/* Command to send to the TPM and its response. */
	h = (struct tpm_vendor_header *)buffer;

	/* Mode sent to the TPM, and TPM's response (overwritten by xmit). */
	m = (uint8_t *)(h + 1);

	tpm_google_fill_vendor_cmd_header(h, VENDOR_CC_TPM_MODE, sizeof(*m));
	marshal_u8(m, mode_val);

	if (me->xmit(me, (const uint8_t *)h, sizeof(buffer),
		     (uint8_t *)h, &recv_len)) {
		printf("%s: IO error\n", __func__);
		return TPM_E_COMMUNICATION_ERROR;
	}

	header_code = unmarshal_u32(&h->code);
	if (header_code == VENDOR_RC_NO_SUCH_COMMAND) {
		printf("%s: Command not supported\n", __func__);
		return TPM_E_NO_SUCH_COMMAND;
	}
	if (header_code) {
		printf("%s: Invalid header code: %d\n", __func__,
		       header_code);
		return TPM_E_INVALID_RESPONSE;
	}
	if (recv_len != sizeof(buffer)) {
		printf("%s: Invalid response\n", __func__);
		return TPM_E_INVALID_RESPONSE;
	}
	if (unmarshal_u8(m) != mode_val) {
		printf("%s: Invalid TPM mode response: %d (expect %d)\n",
		       __func__, unmarshal_u8(m), mode_val);
		return TPM_E_INVALID_RESPONSE;
	}
	return TPM_SUCCESS;
}
