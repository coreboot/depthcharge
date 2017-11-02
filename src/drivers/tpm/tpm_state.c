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

#include "drivers/tpm/tpm.h"
#include "drivers/tpm/tpm_utils.h"

/*
 * The below structure represents the body of the response to the 'report tpm
 * state' vendor command.
 *
 * It is transferred over the wire, so it needs to be serialized/deserialized,
 * and it is likely to change, so its contents must be versioned.
 */
#define TPM_STATE_VERSION	1
struct tpm_vendor_state {
	uint32_t version;
	/*
	 * The following three fields are set by the TPM in case of an assert.
	 * There is no other processing than setting the source code line
	 * number, error code and the first 4 characters of the function name.
	 *
	 * We don't expect this happening, but it is included in the report
	 * just in case.
	 */
	uint32_t fail_line;	/* s_failLIne */
	uint32_t fail_code;	/* s_failCode */
	char func_name[4];	/* s_failFunction, limited to 4 chars */

	/*
	 * The following two fields are the current time filtered value of the
	 * 'failed tries' TPM counter, and the maximum allowed value of the
	 * counter.
	 *
	 * failed_tries == max_tries is the definition of the TPM lockout
	 * condition.
	 */
	uint32_t failed_tries;	/* gp.failedTries */
	uint32_t max_tries;	/* gp.maxTries */
	/* The below fields are present in version 2 and above. */
} __attribute__((packed));

static void stringify_state(struct tpm_vendor_state *s,
			    char *state_str,
			    size_t state_size)
{
	uint32_t version = unmarshal_u32(&s->version);
	size_t text_size = 0;

	text_size += snprintf(state_str + text_size,
			      state_size - text_size,
			      "v=%d", version);
	if (text_size >= state_size)
		return;

	if (version > TPM_STATE_VERSION)
		text_size += snprintf(state_str + text_size,
				      state_size - text_size,
				      " not fully supported\n");
	if (text_size >= state_size)
		return;

	if (version == 0)
		return;	/* This should never happen. */

	text_size += snprintf(state_str + text_size,
			      state_size - text_size,
			      " failed tries=%d max_tries=%d\n",
			      unmarshal_u32(&s->failed_tries),
			      unmarshal_u32(&s->max_tries));
	if (text_size >= state_size)
		return;

	if (unmarshal_u32(&s->fail_line)) {
		/* make sure function name is zero terminated. */
		char func_name[sizeof(s->func_name) + 1];

		memcpy(func_name, s->func_name, sizeof(s->func_name));
		func_name[sizeof(s->func_name)] = '\0';

		text_size += snprintf(state_str + text_size,
				      state_size - text_size,
				      "tpm failed: f %s line %d code %d\n",
				      func_name,
				      unmarshal_u32(&s->fail_line),
				      unmarshal_u32(&s->fail_code));
	}
}

/* Maximum size of the text describing internal TPM state. */
#define STATE_TEXT_SIZE 120

char *tpm_internal_state(struct TpmOps *me)
{
	struct tpm_vendor_header *h;
	struct tpm_vendor_state *s;
	size_t buffer_size = sizeof(struct tpm_vendor_header) +
		sizeof(struct tpm_vendor_state);
	char *state_str;

	/* Command to send to the TPM. */
	h = xzalloc(buffer_size);

	/* Response from the TPM. */
	s = (struct tpm_vendor_state *)(h + 1);

	state_str = xzalloc(STATE_TEXT_SIZE);

	marshal_u16(&h->tag, TPM_ST_NO_SESSIONS);
	marshal_u32(&h->size, sizeof(struct tpm_vendor_header));
	marshal_u32(&h->code, TPM_CC_VENDOR_BIT);
	marshal_u16(&h->subcommand_code, VENDOR_CC_REPORT_TPM_STATE);

	if (me->xmit(me, (void *)h, sizeof(*h), (void *)h, &buffer_size) ||
	    (buffer_size < sizeof(struct tpm_vendor_header))) {
		snprintf(state_str, STATE_TEXT_SIZE, "communications error");
	} else if(unmarshal_u32(&h->code)) {
		snprintf(state_str, STATE_TEXT_SIZE, "TPM error %d",
			 unmarshal_u32(&h->code));
	} else {
		/* TPM responded as expected. */
		stringify_state(s, state_str, STATE_TEXT_SIZE);
	}

	free(h);

	return state_str;
}
