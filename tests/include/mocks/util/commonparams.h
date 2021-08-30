/* SPDX-License-Identifier: GPL-2.0 */

#include <vb2_api.h>

#define WORKBUF_SIZE (16 * MiB)

extern int reset_mock_workbuf;
extern char mock_vboot_workbuf[WORKBUF_SIZE];
extern struct vb2_context *mock_vboot_context;
