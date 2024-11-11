/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __TESTS_MOCKS_UTIL_COMMONPARAMS_H__
#define __TESTS_MOCKS_UTIL_COMMONPARAMS_H__

#include <vb2_api.h>

#define WORKBUF_SIZE (16 * MiB)

extern int reset_mock_workbuf;
extern char mock_vboot_workbuf[WORKBUF_SIZE];
extern struct vb2_context *mock_vboot_context;

#endif /* __TESTS_MOCKS_UTIL_COMMONPARAMS_H__ */
