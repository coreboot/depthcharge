// SPDX-License-Identifier: GPL-2.0

#include "drivers/ec/cros/ec.h"
#include "tests/test.h"

#include "drivers/input/mkbp/mkbp.c"

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

#define MOCK_EC_FIFO_SIZE 64
static struct ec_response_get_next_event_v1
	mock_ec_event_fifo[MOCK_EC_FIFO_SIZE];
static size_t mock_ec_event_fifo_start, mock_ec_event_fifo_end;

static uint32_t mock_buttons;

int cros_ec_get_next_event(struct ec_response_get_next_event_v1 *e)
{
	if (mock_ec_event_fifo_start == mock_ec_event_fifo_end)
		return -EC_RES_UNAVAILABLE;

	memcpy(e, &mock_ec_event_fifo[mock_ec_event_fifo_start],
	       sizeof(struct ec_response_get_next_event_v1));
	mock_ec_event_fifo_start++;

	if (e->event_type != EC_MKBP_EVENT_BUTTON)
		return -EC_RES_UNAVAILABLE;
	return sizeof(e->event_type) + sizeof(e->data.buttons);
}

/* Setup function. */

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
	/* Reset mock variables. */
	memset(mock_ec_event_fifo, 0, sizeof(mock_ec_event_fifo));
	mock_ec_event_fifo_start = 0;
	mock_ec_event_fifo_end = 0;
	mock_buttons = 0;
	return 0;
}

/* Test functions. */

static void add_button_event_to_fifo(uint32_t buttons)
{
	if (mock_ec_event_fifo_end >= MOCK_EC_FIFO_SIZE)
		fail_msg("Mock EC event fifo is full");

	struct ec_response_get_next_event_v1 *event =
		&mock_ec_event_fifo[mock_ec_event_fifo_end];
	event->event_type = EC_MKBP_EVENT_BUTTON;
	event->data.buttons = buttons;
	mock_ec_event_fifo_end++;
}

static void press_button(unsigned int button_bit)
{
	mock_buttons |= 1U << button_bit;
	add_button_event_to_fifo(mock_buttons);
}

static void release_button(unsigned int button_bit)
{
	mock_buttons &= ~(1U << button_bit);
	add_button_event_to_fifo(mock_buttons);
}

#define ASSERT_GETCHAR(c) do { \
	assert_true(mkbp_keyboard.havekey()); \
	assert_int_equal(mkbp_keyboard.getchar(), c); \
} while (0)

#define ASSERT_NO_MORE_CHAR() assert_false(mkbp_keyboard.havekey())

static void test_power_short_press(void **state)
{
	press_button(EC_MKBP_POWER_BUTTON);
	ASSERT_NO_MORE_CHAR();

	/* Single button is reported on release. */
	release_button(EC_MKBP_POWER_BUTTON);
	ASSERT_GETCHAR(UI_BUTTON_POWER_SHORT_PRESS);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_up_short_press(void **state)
{
	press_button(EC_MKBP_VOL_UP);
	release_button(EC_MKBP_VOL_UP);
	ASSERT_GETCHAR(UI_BUTTON_VOL_UP_SHORT_PRESS);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_down_short_press(void **state)
{
	press_button(EC_MKBP_VOL_DOWN);
	release_button(EC_MKBP_VOL_DOWN);
	ASSERT_GETCHAR(UI_BUTTON_VOL_DOWN_SHORT_PRESS);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_up_down_combo(void **state)
{
	/* Combo is reported on button press instead of release. */
	press_button(EC_MKBP_VOL_UP);
	press_button(EC_MKBP_VOL_DOWN);
	ASSERT_GETCHAR(UI_BUTTON_VOL_UP_DOWN_COMBO_PRESS);
	ASSERT_NO_MORE_CHAR();

	/* Combo isn't reported again on release. */
	release_button(EC_MKBP_VOL_DOWN);
	release_button(EC_MKBP_VOL_UP);
	ASSERT_NO_MORE_CHAR();
}

static void test_undefined_combo(void **state)
{
	press_button(EC_MKBP_VOL_UP);

	/* Power button shouldn't be reported when holding another button. */
	press_button(EC_MKBP_POWER_BUTTON);
	release_button(EC_MKBP_POWER_BUTTON);

	release_button(EC_MKBP_VOL_UP);
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
