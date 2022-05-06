/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_COMMON_H
#define _TESTS_VBOOT_COMMON_H

#include <tests/test.h>
#include <mocks/callbacks.h>
#include <vboot_api.h>
#include <vb2_api.h>
#include <vboot/ui.h>

#define ASSERT_VB2_SUCCESS(expr) assert_int_equal((expr), VB2_SUCCESS)

/* Do not reference these 2 functions directly in tests. Use the macro below. */
static inline vb2_error_t _try_load_internal_disk(void)
{
	return mock_type(vb2_error_t);
}

static inline vb2_error_t _try_load_external_disk(void)
{
	return mock_type(vb2_error_t);
}

/* Macros for mocking VbTryLoadKernel(). */
#define WILL_LOAD_INTERNAL_ALWAYS(value) \
	will_return_always(_try_load_internal_disk, value)

#define WILL_LOAD_EXTERNAL(value) \
	will_return(_try_load_external_disk, value)
#define WILL_LOAD_EXTERNAL_COUNT(value, count) \
	will_return_count(_try_load_external_disk, value, count)
#define WILL_LOAD_EXTERNAL_MAYBE(value) \
	will_return_maybe(_try_load_external_disk, value)
#define WILL_LOAD_EXTERNAL_ALWAYS(value) \
	will_return_always(_try_load_external_disk, value)
#define WILL_HAVE_NO_EXTERNAL() \
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_ERROR_LK_NO_DISK_FOUND)

/* Force set the constant boot_mode in vboot context. */
static inline void set_boot_mode(struct vb2_context *ctx,
				 enum vb2_boot_mode boot_mode)
{
	enum vb2_boot_mode *local_boot_mode =
		(enum vb2_boot_mode *)&(ctx->boot_mode);
	*local_boot_mode = boot_mode;
}

#endif /* _TESTS_VBOOT_COMMON_H */
