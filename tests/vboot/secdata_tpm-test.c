// SPDX-License-Identifier: GPL-2.0

#include <mocks/mock_tlcl_rw.h>
#include <stdio.h>
#include <tests/test.h>
#include <tss_constants.h>
#include <vboot/secdata_tpm.h>

static void test_secdata_firmware_write(void **state)
{
	struct vb2_context *ctx;
	uint8_t buf[VB2_SECDATA_FIRMWARE_SIZE] = {0};

	vb2api_init(workbuf_firmware, sizeof(workbuf_firmware), &ctx);

	/* Write to unchanged firmware should be successful */
	ctx->flags &= ~VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	assert_int_equal(secdata_firmware_write(ctx), TPM_SUCCESS);

	/* Write to changed firmware but not in recovery mode should fail */
	ctx->flags |= VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	ctx->flags &= ~VB2_CONTEXT_RECOVERY_MODE;
	assert_int_equal(secdata_firmware_write(ctx), TPM_E_AREA_LOCKED);

	/* Check params provided to mock by UUT */
	expect_value(TlclWrite, index, FIRMWARE_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_FIRMWARE_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, 0);

	/* Write to changed firmware in recovery mode should be successful and
	   should return bytes to previously passed buffer. It should also unset
	   a firmware changed flag. */
	ctx->flags = (VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED |
				   VB2_CONTEXT_RECOVERY_MODE);
	memset(buf, 0x0B, sizeof(buf));
	memset(ctx->secdata_firmware, 0xF4, VB2_SECDATA_FIRMWARE_SIZE);
	assert_int_equal(secdata_firmware_write(ctx), TPM_SUCCESS);
	assert_memory_equal(ctx->secdata_firmware, buf,
			    VB2_SECDATA_FIRMWARE_SIZE);
	assert_false(ctx->flags & VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED);

	/* Expect TlclForceClear(), TlclSetEnable() and TlclSetDeactivated(0)
	   to be called for TPM_E_MAXNVWRITES as TlclWrite() return value. */
	expect_value(TlclWrite, index, FIRMWARE_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_FIRMWARE_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, TPM_E_MAXNVWRITES);

	expect_function_call(TlclForceClear);
	will_return(TlclForceClear, TPM_SUCCESS);
	expect_function_call(TlclSetEnable);
	will_return(TlclSetEnable, TPM_SUCCESS);
	expect_function_call(TlclSetDeactivated);
	expect_value(TlclSetDeactivated, flag, 0);
	will_return(TlclSetDeactivated, TPM_SUCCESS);

	expect_value(TlclWrite, index, FIRMWARE_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_FIRMWARE_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, 0);

	ctx->flags = (VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED |
				   VB2_CONTEXT_RECOVERY_MODE);
	memset(buf, 0xDD, sizeof(buf));
	memset(ctx->secdata_firmware, 0x22, VB2_SECDATA_FIRMWARE_SIZE);
	assert_int_equal(secdata_firmware_write(ctx), TPM_SUCCESS);
}

static void test_secdata_kernel_write(void **state)
{
	struct vb2_context *ctx;
	uint8_t buf[VB2_SECDATA_KERNEL_SIZE] = {0};

	vb2api_init(workbuf_kernel, sizeof(workbuf_kernel), &ctx);

	/* Write to unchanged kernel should be successful */
	ctx->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	assert_int_equal(secdata_kernel_write(ctx), TPM_SUCCESS);

	expect_value(TlclWrite, index, KERNEL_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_KERNEL_MIN_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, 0);
	/* Write to changed kernel should be successful and should return bytes
	   to previously passed buffer. It should also unset a kernel changed
	   flag. */
	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	memset(buf, 0xC8, sizeof(buf));
	memset(ctx->secdata_kernel, 0x37, VB2_SECDATA_KERNEL_SIZE);
	assert_int_equal(secdata_kernel_write(ctx), TPM_SUCCESS);
	assert_memory_equal(ctx->secdata_kernel, buf,
			    VB2_SECDATA_KERNEL_MIN_SIZE);
	assert_false(ctx->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED);

	expect_value(TlclWrite, index, KERNEL_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_KERNEL_MIN_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, 0);
	/* Write to changed kernel should be successful and should return bytes
	   to previously passed buffer. It should also unset a kernel changed
	   flag. */
	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	memset(buf, 0xC8, sizeof(buf));
	memset(ctx->secdata_kernel, 0x37, VB2_SECDATA_KERNEL_SIZE);
	assert_int_equal(secdata_kernel_write(ctx), TPM_SUCCESS);
	assert_memory_equal(ctx->secdata_kernel, buf,
			    VB2_SECDATA_KERNEL_MIN_SIZE);
	assert_false(ctx->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED);

	/* Expect TlclForceClear(), TlclSetEnable() and TlclSetDeactivated(0)
	   to be called for TPM_E_MAXNVWRITES as TlclWrite() return value. */
	expect_value(TlclWrite, index, KERNEL_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_KERNEL_MIN_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, TPM_E_MAXNVWRITES);

	expect_function_call(TlclForceClear);
	will_return(TlclForceClear, TPM_SUCCESS);
	expect_function_call(TlclSetEnable);
	will_return(TlclSetEnable, TPM_SUCCESS);
	expect_function_call(TlclSetDeactivated);
	expect_value(TlclSetDeactivated, flag, 0);
	will_return(TlclSetDeactivated, TPM_SUCCESS);

	expect_value(TlclWrite, index, KERNEL_NV_INDEX);
	expect_value(TlclWrite, length, VB2_SECDATA_KERNEL_MIN_SIZE);
	expect_not_value(TlclWrite, data, (uintptr_t)NULL);
	will_return(TlclWrite, (uintptr_t)buf);
	will_return(TlclWrite, 0);

	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	memset(buf, 0x55, sizeof(buf));
	memset(ctx->secdata_kernel, 0xAA, VB2_SECDATA_KERNEL_SIZE);
	assert_int_equal(secdata_kernel_write(ctx), TPM_SUCCESS);
	assert_memory_equal(ctx->secdata_kernel, buf,
			    VB2_SECDATA_KERNEL_MIN_SIZE);
	assert_false(ctx->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED);
}

static void test_secdata_kernel_lock(void **state)
{
	struct vb2_context *ctx;
	vb2api_init(workbuf_kernel, sizeof(workbuf_kernel), &ctx);

	/* Try to lock kernel but fail */
	expect_function_call(TlclLockPhysicalPresence);
	will_return(TlclLockPhysicalPresence, TPM_E_NON_FATAL);
	assert_int_equal(secdata_kernel_lock(ctx), TPM_E_NON_FATAL);

	/* Lock unlocked kernel */
	expect_function_call(TlclLockPhysicalPresence);
	will_return(TlclLockPhysicalPresence, TPM_SUCCESS);
	assert_int_equal(secdata_kernel_lock(ctx), TPM_SUCCESS);

	/* Locking a locked kernel should not fail */
	assert_int_equal(secdata_kernel_lock(ctx), TPM_SUCCESS);
}

static void test_secdata_fwmp_read(void **state)
{
	struct vb2_context *ctx;
	uint8_t buf[VB2_SECDATA_FWMP_MAX_SIZE] = {0};
	struct vb2_secdata_fwmp *sec = (struct vb2_secdata_fwmp *)&buf[0];

	vb2api_init(workbuf_firmware, sizeof(workbuf_firmware), &ctx);

	expect_value(TlclRead, index, FWMP_NV_INDEX);
	expect_value(TlclRead, length, VB2_SECDATA_FWMP_MIN_SIZE);
	expect_not_value(TlclRead, data, (uintptr_t)NULL);
	will_return(TlclRead, (uintptr_t)buf);
	will_return(TlclRead, TPM_E_BADINDEX);
	/* secdata_fwmp_read() should set NO_SECDATA_FWMP flag for
	   TPM_E_BADINDEX error code from TlclRead() */
	ctx->flags &= ~VB2_CONTEXT_NO_SECDATA_FWMP;
	assert_int_equal(secdata_fwmp_read(ctx), TPM_SUCCESS);
	assert_true(ctx->flags & VB2_CONTEXT_NO_SECDATA_FWMP);

	/* Error code other than TPM_E_BADINDEX should be returned */
	expect_value(TlclRead, index, FWMP_NV_INDEX);
	expect_value(TlclRead, length, VB2_SECDATA_FWMP_MIN_SIZE);
	expect_not_value(TlclRead, data, (uintptr_t)NULL);
	will_return(TlclRead, (uintptr_t)buf);
	will_return(TlclRead, TPM_E_BADTAG);
	ctx->flags &= ~VB2_CONTEXT_NO_SECDATA_FWMP;
	assert_int_equal(secdata_fwmp_read(ctx), TPM_E_BADTAG);
	assert_false(ctx->flags & VB2_CONTEXT_NO_SECDATA_FWMP);

	/* secdata_fwmp_read() should successfully read FWMP_MIN_SIZE bytes */
	expect_value(TlclRead, index, FWMP_NV_INDEX);
	expect_value(TlclRead, length, VB2_SECDATA_FWMP_MIN_SIZE);
	expect_not_value(TlclRead, data, (uintptr_t)NULL);
	will_return(TlclRead, (uintptr_t)buf);
	will_return(TlclRead, TPM_SUCCESS);
	ctx->flags &= ~VB2_CONTEXT_NO_SECDATA_FWMP;
	memset(buf, 0x58, sizeof(buf));
	memset(ctx->secdata_fwmp, 0xA7, VB2_SECDATA_FWMP_MAX_SIZE);
	sec->struct_size = VB2_SECDATA_FWMP_MIN_SIZE;
	assert_int_equal(secdata_fwmp_read(ctx), TPM_SUCCESS);
	assert_memory_equal(ctx->secdata_fwmp, buf, VB2_SECDATA_FWMP_MIN_SIZE);

	/* TlclRead() is called twice in secdata_fwmp_read(), so checks and
	   returns have to be set twice as well. */
	expect_value(TlclRead, index, FWMP_NV_INDEX);
	expect_value(TlclRead, length, VB2_SECDATA_FWMP_MIN_SIZE);
	expect_not_value(TlclRead, data, (uintptr_t)NULL);
	will_return(TlclRead, (uintptr_t)buf);
	will_return(TlclRead, TPM_SUCCESS);
	expect_value(TlclRead, index, FWMP_NV_INDEX);
	expect_value(TlclRead, length, VB2_SECDATA_FWMP_MAX_SIZE);
	expect_not_value(TlclRead, data, (uintptr_t)NULL);
	will_return(TlclRead, (uintptr_t)buf);
	will_return(TlclRead, TPM_SUCCESS);
	ctx->flags &= ~VB2_CONTEXT_NO_SECDATA_FWMP;
	memset(buf, 0xA7, sizeof(buf));
	memset(ctx->secdata_fwmp, 0xCC, VB2_SECDATA_FWMP_MAX_SIZE);
	/* Tell secdata_fwmp_read() that there is more data than
	   a minimal required amount. */
	sec->struct_size = VB2_SECDATA_FWMP_MAX_SIZE;
	assert_int_equal(secdata_fwmp_read(ctx), TPM_SUCCESS);
	assert_memory_equal(ctx->secdata_fwmp, buf, VB2_SECDATA_FWMP_MAX_SIZE);
}

static int setup_firmware_test(void **state)
{
	memset(workbuf_firmware, 0, sizeof(workbuf_firmware));
	return 0;
}

static int setup_kernel_test(void **state)
{
	memset(workbuf_kernel, 0, sizeof(workbuf_kernel));
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_secdata_firmware_write,
							   setup_firmware_test),
		cmocka_unit_test_setup(test_secdata_kernel_write,
							   setup_kernel_test),
		cmocka_unit_test_setup(test_secdata_kernel_lock,
							   setup_kernel_test),
		cmocka_unit_test_setup(test_secdata_fwmp_read,
							   setup_firmware_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
