// SPDX-License-Identifier: GPL-2.0

#include "drivers/ec/cros/ec.h"
#include "tests/test.h"

#include "drivers/input/mkbp/mkbp.c"

static int setup(void **state)
{
	/* A hack to reset the static variable 'combo_detected' in
	   mkbp_add_button(), by simulating button release. */
	prev_bitmap = 0x1;
	mkbp_add_button(NULL, 0, 0x0);
	/* Reset global variables. */
	prev_bitmap = 0;
	memset(key_fifo, 0, sizeof(key_fifo));
	fifo_offset = 0;
	fifo_size = 0;
	return 0;
}

/* Mock functions. */

int cros_ec_interrupt_pending(void)
{
	return 1;
}

int cros_ec_get_host_events(uint32_t *events_ptr)
{
	*events_ptr = 0;
	return 0;
}

int cros_ec_clear_host_events(uint32_t events)
{
	return 0;
}

int cros_ec_get_next_event(struct ec_response_get_next_event_v1 *e)
{
	e->event_type = mock_type(uint8_t);
	if (e->event_type != EC_MKBP_EVENT_BUTTON)
		return -EC_RES_UNAVAILABLE;
	e->data.buttons = mock_type(uint32_t);
	return sizeof(e->event_type) + sizeof(e->data.buttons);
}

/* Test functions. */

#define WILL_GET_BUTTON_EVENT(buttons) do { \
	will_return(cros_ec_get_next_event, EC_MKBP_EVENT_BUTTON); \
	will_return(cros_ec_get_next_event, buttons); \
} while (0)

#define WILL_HAVE_NO_EVENT() will_return(cros_ec_get_next_event, -1)

#define WILL_PRESS_BUTTON(buttons, button_bit) ({ \
	uint32_t _test_buttons = buttons; \
	_test_buttons |= 1U << button_bit; \
	WILL_GET_BUTTON_EVENT(_test_buttons); \
	_test_buttons; \
})

#define WILL_RELEASE_BUTTON(buttons, button_bit) ({ \
	uint32_t _test_buttons = buttons; \
	_test_buttons &= ~(1U << button_bit); \
	WILL_GET_BUTTON_EVENT(_test_buttons); \
	_test_buttons; \
})

#define ASSERT_GETCHAR(c) do { \
	assert_true(mkbp_keyboard.havekey()); \
	assert_int_equal(mkbp_keyboard.getchar(), c); \
} while (0)

#define ASSERT_NO_MORE_CHAR() assert_false(mkbp_keyboard.havekey())

static void test_power_short_press(void **state)
{
	uint32_t b = 0;
	b = WILL_PRESS_BUTTON(b, EC_MKBP_POWER_BUTTON);
	WILL_HAVE_NO_EVENT();
	ASSERT_NO_MORE_CHAR();

	/* Single button is reported on release. */
	b = WILL_RELEASE_BUTTON(b, EC_MKBP_POWER_BUTTON);
	WILL_HAVE_NO_EVENT();
	ASSERT_GETCHAR(UI_BUTTON_POWER_SHORT_PRESS);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_up_short_press(void **state)
{
	uint32_t b = 0;
	b = WILL_PRESS_BUTTON(b, EC_MKBP_VOL_UP);
	b = WILL_RELEASE_BUTTON(b, EC_MKBP_VOL_UP);
	WILL_HAVE_NO_EVENT();
	ASSERT_GETCHAR(UI_BUTTON_VOL_UP_SHORT_PRESS);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_down_short_press(void **state)
{
	uint32_t b = 0;
	b = WILL_PRESS_BUTTON(b, EC_MKBP_VOL_DOWN);
	b = WILL_RELEASE_BUTTON(b, EC_MKBP_VOL_DOWN);
	WILL_HAVE_NO_EVENT();
	ASSERT_GETCHAR(UI_BUTTON_VOL_DOWN_SHORT_PRESS);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_up_down_combo(void **state)
{
	/* Combo is reported on button press instead of release. */
	uint32_t b = 0;
	b = WILL_PRESS_BUTTON(b, EC_MKBP_VOL_UP);
	b = WILL_PRESS_BUTTON(b, EC_MKBP_VOL_DOWN);
	WILL_HAVE_NO_EVENT();
	ASSERT_GETCHAR(UI_BUTTON_VOL_UP_DOWN_COMBO_PRESS);
	ASSERT_NO_MORE_CHAR();

	/* Combo isn't reported again on release. */
	b = WILL_RELEASE_BUTTON(b, EC_MKBP_VOL_DOWN);
	b = WILL_RELEASE_BUTTON(b, EC_MKBP_VOL_UP);
	WILL_HAVE_NO_EVENT();
	ASSERT_NO_MORE_CHAR();
}

static void test_undefined_combo(void **state)
{
	uint32_t b = 0;
	b = WILL_PRESS_BUTTON(b, EC_MKBP_VOL_UP);

	/* Power button shouldn't be reported when holding another button. */
	b = WILL_PRESS_BUTTON(b, EC_MKBP_POWER_BUTTON);
	b = WILL_RELEASE_BUTTON(b, EC_MKBP_POWER_BUTTON);

	b = WILL_RELEASE_BUTTON(b, EC_MKBP_VOL_UP);
	WILL_HAVE_NO_EVENT();
	ASSERT_NO_MORE_CHAR();
}

#define MKBP_TEST(func) cmocka_unit_test_setup(func, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		MKBP_TEST(test_power_short_press),
		MKBP_TEST(test_vol_up_short_press),
		MKBP_TEST(test_vol_down_short_press),
		MKBP_TEST(test_vol_up_down_combo),
		MKBP_TEST(test_undefined_combo),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
