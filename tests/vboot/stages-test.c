// SPDX-License-Identifier: GPL-2.0

#include <drivers/power/power.h>
#include <mocks/util/commonparams.h>
#include <tests/test.h>
#include <vboot/secdata_tpm.h>

#include "vboot/stages.c"

static int setup(void **state)
{
	reset_mock_workbuf = 1;
	return 0;
}

/* Mock functions */

vb2_error_t vb2ex_commit_data(struct vb2_context *ctx)
{
	return mock_type(vb2_error_t);
}

#define expect_reboot(expression, return_value) do { \
	will_return(vb2ex_commit_data, return_value); \
	expect_assert_failure(expression); \
} while (0)

#define expect_fail_and_reboot(expression) do { \
	expect_function_call(vb2api_fail); \
	expect_reboot(run_cleanup(CleanupOnHandoff), VB2_SUCCESS); \
} while (0)

void reboot(void)
{
	mock_assert(0, __func__, __FILE__, __LINE__);
	/* Infinte loop is necessary due to __attribute__((noreturn)) */
	while (1);
}

vb2_error_t vb2ex_tpm_set_mode(enum vb2_tpm_mode mode_val)
{
	return mock_type(vb2_error_t);
}

uint32_t secdata_kernel_lock(struct vb2_context *ctx)
{
	return mock_type(uint32_t);
}

void vb2api_fail(struct vb2_context *ctx, uint8_t reason, uint8_t subcode)
{
	function_called();
}

/* Test functions */

static inline void run_cleanup(CleanupType types)
{
	commit_and_lock_cleanup_func(&commit_and_lock_cleanup, types);
}

static void test_cleanup_func_on_reboot(void **state)
{
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	run_cleanup(CleanupOnReboot);
}

static void test_cleanup_func_on_poweroff(void **state)
{
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	run_cleanup(CleanupOnPowerOff);
}

static void test_cleanup_func_on_poweroff_commit_fail(void **state)
{
	expect_reboot(run_cleanup(CleanupOnPowerOff), VB2_ERROR_MOCK);
}

static void test_cleanup_func_on_handoff_in_recovery(void **state)
{
	struct vb2_context *ctx = vboot_get_context();
	ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	run_cleanup(CleanupOnHandoff);
}

static void test_cleanup_func_on_handoff_disable_tpm(void **state)
{
	struct vb2_context *ctx = vboot_get_context();
	ctx->flags |= VB2_CONTEXT_DISABLE_TPM;
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	will_return(vb2ex_tpm_set_mode, VB2_SUCCESS);
	run_cleanup(CleanupOnHandoff);
}

static void test_cleanup_func_on_handoff_disable_tpm_fail(void **state)
{
	struct vb2_context *ctx = vboot_get_context();
	ctx->flags |= VB2_CONTEXT_DISABLE_TPM;
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	will_return(vb2ex_tpm_set_mode, VB2_ERROR_MOCK);
	expect_fail_and_reboot(run_cleanup(CleanupOnHandoff));
}

static void test_cleanup_func_on_handoff_lock_tpm(void **state)
{
	will_return(secdata_kernel_lock, 0);
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	run_cleanup(CleanupOnHandoff);
}

static void test_cleanup_func_on_handoff_lock_tpm_fail(void **state)
{
	will_return(secdata_kernel_lock, 1);
	will_return(vb2ex_commit_data, VB2_SUCCESS);
	expect_fail_and_reboot(run_cleanup(CleanupOnHandoff));
}

#define STAGES_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		STAGES_TEST(test_cleanup_func_on_reboot),
		STAGES_TEST(test_cleanup_func_on_poweroff),
		STAGES_TEST(test_cleanup_func_on_poweroff_commit_fail),
		STAGES_TEST(test_cleanup_func_on_handoff_in_recovery),
		STAGES_TEST(test_cleanup_func_on_handoff_disable_tpm),
		STAGES_TEST(test_cleanup_func_on_handoff_disable_tpm_fail),
		STAGES_TEST(test_cleanup_func_on_handoff_lock_tpm),
		STAGES_TEST(test_cleanup_func_on_handoff_lock_tpm_fail),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
