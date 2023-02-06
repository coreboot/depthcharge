/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __VBOOT_REFERENCE_WIDEVINE_H__
#define __VBOOT_REFERENCE_WIDEVINE_H__

#include <stddef.h>
#include <stdint.h>
#include <vb2_api.h>

int widevine_write_smc_data(uint64_t function_id, uint8_t *data,
			    uint32_t length);

uint32_t prepare_widevine_root_of_trust(struct vb2_context *ctx);

uint32_t prepare_widevine_tpm_pubkey(void);

#endif /* __VBOOT_REFERENCE_WIDEVINE_H__ */
