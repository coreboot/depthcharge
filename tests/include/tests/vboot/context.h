/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __MOCKS_VBOOT_CONTEXT_H__
#define __MOCKS_VBOOT_CONTEXT_H__

#include <libpayload.h>
#include <vb2_api.h>

#include "vboot/context.h"

#define WORKBUF_SIZE (16 * MiB)

extern int reset_mock_workbuf;
extern char mock_vboot_workbuf[WORKBUF_SIZE];
extern struct vb2_context *mock_vboot_context;

#endif /* __MOCKS_VBOOT_CONTEXT_H__ */
