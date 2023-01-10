/*
 * Copyright 2017 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_TPM_GOOGLE_TPM_H__
#define __DRIVERS_TPM_GOOGLE_TPM_H__

#include <libpayload.h>

#include "drivers/tpm/tpm.h"

/*
 * This structure describes the header used for TPM Vendor commands and their
 * responses. Command payload or response (if any) are concatenated with the
 * header. All values are transmitted in big endian format.
 */

struct tpm_vendor_header {
	uint16_t tag;		  /* TPM_ST_NO_SESSIONS */
	uint32_t size;		  /* including this header */
	uint32_t code;		  /* Command out, Response code back. */
	uint16_t subcommand_code; /* Vendor subcommand, not used on response. */
} __attribute__((packed));

/*
 * TPMv2 Spec mandates that vendor-specific command codes have bit 29 set,
 * while bits 15-0 indicate the command. All other bits should be zero. We
 * define one of those 16-bit command values for Cr50 purposes, and use the
 * subcommand_code in struct TpmCmdHeader to further distinguish the desired
 * operation.
 */
#define TPM_CC_VENDOR_BIT   0x20000000

/* Cr50 vendor-specific subcommand codes. 16 bits available. */
enum vendor_cmd_cc {
	VENDOR_CC_REPORT_TPM_STATE = 23,
	VENDOR_CC_GET_REC_BTN = 29,
	VENDOR_CC_TPM_MODE = 40,
	VENDOR_CC_GET_PWR_BTN = 43,
};

/* Error codes reported by extension and vendor commands. */
enum vendor_cmd_rc {
	VENDOR_RC_NO_SUCH_COMMAND = 0x57f,
};

#define TPM_ST_NO_SESSIONS 0x8001

static inline void marshal_u8(void *buf, uint8_t val) { *(uint8_t *)buf = val; }
static inline void marshal_u16(void *buf, u16 val) { be16enc(buf, val); }
static inline void marshal_u32(void *buf, u32 val) { be32enc(buf, val); }
static inline uint8_t unmarshal_u8(void *buf) { return *(uint8_t *)buf; }
static inline uint32_t unmarshal_u32(void *buf) { return be32dec(buf); }

void tpm_google_fill_vendor_cmd_header(struct tpm_vendor_header *h,
				       u16 subcommand, size_t content_size);

/*
 * tpm_google_get_tpm_state()
 *
 * When implemented, queries the TPM, retrieves its internal state and then
 * returns a string containing the state informwation. The string space is
 * allocated by the this function and is expected to be freed by the caller.
 */
char *tpm_google_get_tpm_state(struct TpmOps *me);

/*
 * tpm_google_set_tpm_mode()
 *
 * Sets the TPM mode value and validates that it was changed. If one of the
 * following occurs, the function call fails:
 *   - TPM does not understand the instruction (old version)
 *   - TPM has already left the TpmModeEnabledTentative mode
 *   - TPM responds with a mode other than the requested mode
 *   - Some other communication error occurs
 *  Otherwise, the function call succeeds.
 *
 * Returns 0 on success or non-zero on failure (the failure result is typically
 * TPM_E_INTERNAL_ERROR if the command was not understood, otherwise -1).
 */
int tpm_google_set_tpm_mode(struct TpmOps *me, uint8_t mode_val);

#endif /* __DRIVERS_TPM_GOOGLE_TPM_H__*/
