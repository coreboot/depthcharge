// SPDX-License-Identifier: GPL-2.0

#include <base/elog.h>
#include <diag/common.h>
#include <drivers/storage/blockdev.h>
#include <mocks/callbacks.h>
#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/context.h>
#include <tests/vboot/ui/common.h>
#include <vb2_api.h>
#include <vboot/stages.h>

/* Mock functions */
int ui_is_lid_open(void)
{
	return mock();
}

int has_external_display(void)
{
	return 0;
}

/* Helper functions */

char expected_event_data[ELOG_MAX_EVENT_DATA_SIZE];
const char mock_report_data[] = "MOCK_DIAG_REPORT_DUMP";

/* Event size = report size + subtype */
#define EXPECTED_RAW_EVENT_SIZE (sizeof(mock_report_data) + 1)

#define EXPECT_DIAG_REPORT(_type, _result) do {				       \
	expect_value(diag_report_start_test, type, (_type));		       \
	expect_value(diag_report_end_test, result, (_result));		       \
} while (0)

#define WILL_DUMP_REPORT(_data) do {					       \
	will_return(diag_report_dump, (_data));				       \
	will_return(diag_report_dump, sizeof(_data));			       \
} while (0)

#define EXPECT_DIAG_DUMP_AND_ELOG() do {				       \
	expect_value(diag_report_dump, size, ELOG_MAX_EVENT_DATA_SIZE - 1);    \
	expect_value(elog_add_event_raw, event_type,			       \
		     ELOG_TYPE_CROS_DIAGNOSTICS);			       \
	expect_value(elog_add_event_raw, data_size,			       \
		     EXPECTED_RAW_EVENT_SIZE);				       \
	expect_memory(elog_add_event_raw, data, expected_event_data,	       \
		      EXPECTED_RAW_EVENT_SIZE);				       \
} while (0)

/* Tests */
struct ui_context test_ui_ctx;
struct vb2_kernel_params test_kparams;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;

	test_ui_ctx.ctx = vboot_get_context();
	set_boot_mode(test_ui_ctx.ctx, VB2_BOOT_MODE_DIAGNOSTICS);

	memset(&test_kparams, 0, sizeof(test_kparams));
	test_ui_ctx.kparams = &test_kparams;
	*state = &test_ui_ctx;

	diag_report_clear();

	/* Prepare the expected raw value of event log */
	if (EXPECTED_RAW_EVENT_SIZE > ELOG_MAX_EVENT_DATA_SIZE)
		fail_msg("The mock_report_data exceeds maximum buffer size");
	memset(expected_event_data, 0, sizeof(expected_event_data));
	expected_event_data[0] = ELOG_CROS_DIAGNOSTICS_LOGS;
	memcpy(expected_event_data + 1, mock_report_data,
	       sizeof(mock_report_data));

	return 0;
}

static void test_diagnostics_screen_disabled_and_hidden(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(3);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_always(diag_storage_test_supported,
			   BLOCKDEV_TEST_OPS_TYPE_SHORT |
			   BLOCKDEV_TEST_OPS_TYPE_EXTENDED);
	will_return_maybe(memory_test_init, DIAG_TEST_PASSED);
	WILL_DUMP_REPORT(mock_report_data);
	EXPECT_DIAG_DUMP_AND_ELOG();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_diagnostics_screen(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 1);
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT);
	EXPECT_UI_DISPLAY_ANY();
	/* #1: Storage health screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS_STORAGE_HEALTH);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_DIAG_REPORT(ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH,
			   ELOG_CROS_DIAG_RESULT_PASSED);
	/* #2: Short storage self-test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_DIAG_REPORT(ELOG_CROS_DIAG_TYPE_STORAGE_TEST_SHORT,
			   ELOG_CROS_DIAG_RESULT_PASSED);
	/* #3: Extended storage self-test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 3);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_DIAG_REPORT(ELOG_CROS_DIAG_TYPE_STORAGE_TEST_EXTENDED,
			   ELOG_CROS_DIAG_RESULT_PASSED);
	/* #4: Quick memory test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 4);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS_MEMORY_QUICK);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_DIAG_REPORT(ELOG_CROS_DIAG_TYPE_MEMORY_QUICK,
			   ELOG_CROS_DIAG_RESULT_PASSED);
	/* #5: Full memory test screen */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 5);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS_MEMORY_FULL);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_DIAG_REPORT(ELOG_CROS_DIAG_TYPE_MEMORY_FULL,
			   ELOG_CROS_DIAG_RESULT_PASSED);
	/* #6: Power off (End of menu) */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, 6);

	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_get_locale_count, 10);
	SET_LOG_DIMENSIONS(40, 20);
	will_return_always(diag_storage_test_supported, 3);
	will_return_always(diag_dump_storage_test_log, DIAG_TEST_PASSED);
	will_return_always(diag_storage_test_control, DIAG_TEST_PASSED);
	will_return_always(memory_test_init, DIAG_TEST_PASSED);
	will_return_always(memory_test_run, DIAG_TEST_PASSED);

	WILL_DUMP_REPORT(mock_report_data);
	EXPECT_DIAG_DUMP_AND_ELOG();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_diagnostics_screen_no_storage_self_test(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(3);
	will_return_always(diag_storage_test_supported, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, MOCK_IGNORE,
			  0xc, 0x0); /* 0xc = 0b1100 */
	WILL_DUMP_REPORT(mock_report_data);
	EXPECT_DIAG_DUMP_AND_ELOG();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_diagnostics_screen_no_storage_extended_test(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(3);
	will_return_always(diag_storage_test_supported,
			   BLOCKDEV_TEST_OPS_TYPE_SHORT);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DIAGNOSTICS, MOCK_IGNORE, MOCK_IGNORE,
			  0x8, 0x0); /* 0x4 = 0b1000 */
	WILL_DUMP_REPORT(mock_report_data);
	EXPECT_DIAG_DUMP_AND_ELOG();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UI_TEST(test_diagnostics_screen_disabled_and_hidden),
		UI_TEST(test_diagnostics_screen),
		UI_TEST(test_diagnostics_screen_no_storage_self_test),
		UI_TEST(test_diagnostics_screen_no_storage_extended_test),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
