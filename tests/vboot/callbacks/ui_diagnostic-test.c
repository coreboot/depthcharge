// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <mocks/callbacks.h>
#include <mocks/util/commonparams.h>
#include <vboot/util/commonparams.h>

#define SETUP_VB_TRY_LOAD_KERNEL(_rv, _disk_flags) \
	do { \
		will_return_always(VbTryLoadKernel, (_rv)); \
		expect_value_count(VbTryLoadKernel, disk_flags, \
				   (_disk_flags), WILL_RETURN_ALWAYS); \
	} while (0)

/* Mock functions */
uint32_t VbExIsShutdownRequested(void) { return mock_type(uint32_t); }

/* Tests */
struct ui_context test_ui_ctx;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;

	test_ui_ctx.ctx = vboot_get_context();

	*state = &test_ui_ctx;

	return 0;
}

static void test_diagnostics_screen_hisabled_and_hidden(void **state)
{
	struct ui_context *ui = *state;

	WILL_SHUTDOWN_IN(3);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_diag_get_storage_test_log, VB2_SUCCESS);

	assert_int_equal(vb2ex_diagnostic_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_diagnostics_screen(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 1);
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT);
	EXPECT_DISPLAY_UI_ANY();
	/* #1: Storage health screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS_STORAGE_HEALTH);
	EXPECT_DISPLAY_UI_ANY();
	/* #2: Short storage self-test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 2);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT);
	EXPECT_DISPLAY_UI_ANY();
	/* #3: Extended storage self-test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 3);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED);
	EXPECT_DISPLAY_UI_ANY();
	/* #4: Quick memory test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 4);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS_MEMORY_QUICK);
	EXPECT_DISPLAY_UI_ANY();
	/* #5: Full memory test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 5);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS_MEMORY_FULL);
	EXPECT_DISPLAY_UI_ANY();
	/* #6: Power off (End of menu) */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 6);

	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_get_locale_count, 10);
	expect_any_always(vb2ex_prepare_log_screen, str);
	will_return_maybe(vb2ex_prepare_log_screen, 1);
	will_return_maybe(vb2ex_diag_get_storage_test_log, VB2_SUCCESS);

	assert_int_equal(vb2ex_diagnostic_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_diagnostics_screen_no_nvme(void **state)
{
	struct ui_context *ui = *state;

	WILL_SHUTDOWN_IN(3);
	will_return_always(vb2ex_diag_get_storage_test_log,
			   VB2_ERROR_EX_UNIMPLEMENTED);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DIAGNOSTICS, MOCK_IGNORE, MOCK_IGNORE,
			  0xc, 0x0); /* 0xc = 0b1100 */

	assert_int_equal(vb2ex_diagnostic_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UI_TEST(test_diagnostics_screen_hisabled_and_hidden),
		UI_TEST(test_diagnostics_screen),
		UI_TEST(test_diagnostics_screen_no_nvme),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
