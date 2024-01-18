// SPDX-License-Identifier: GPL-2.0

#include <keycodes.h>
#include <libpayload.h>

#include "drivers/ec/cros/ec.h"
#include "drivers/input/mkbp/keymatrix.h"
#include "tests/test.h"

#include "drivers/input/mkbp/mkbp.c"

/* Mock functions. */

static uint64_t mock_timer_ms;

uint64_t timer_us(uint64_t base)
{
	return mock_timer_ms * USECS_PER_MSEC - base;
}

static int mock_ec_interrupt;

int cros_ec_interrupt_pending(void)
{
	return mock_ec_interrupt;
}

int cros_ec_get_host_events(uint32_t *events_ptr)
{
	fail_msg("%s isn't expected to be called", __func__);
	return 0;
}

int cros_ec_clear_host_events(uint32_t events)
{
	fail_msg("%s isn't expected to be called", __func__);
	return 0;
}

#define MOCK_EC_FIFO_SIZE 64
static struct ec_response_get_next_event_v1
	mock_ec_event_fifo[MOCK_EC_FIFO_SIZE];
static size_t mock_ec_event_fifo_start, mock_ec_event_fifo_end;
static bool get_next_event_called;
static bool get_next_event_called_with_empty_fifo;

#define MOCK_EC_KEY_MATRIX_SIZE \
	(sizeof(((struct ec_response_get_next_event_v1 *)0)->data.key_matrix))
static uint8_t mock_keys[MOCK_EC_KEY_MATRIX_SIZE];
static uint32_t mock_buttons;

int cros_ec_get_next_event(struct ec_response_get_next_event_v1 *e)
{
	mock_timer_ms += 20;
	get_next_event_called = true;
	if (mock_ec_event_fifo_start == mock_ec_event_fifo_end) {
		get_next_event_called_with_empty_fifo = true;
		return -EC_RES_UNAVAILABLE;
	}

	memcpy(e, &mock_ec_event_fifo[mock_ec_event_fifo_start],
	       sizeof(struct ec_response_get_next_event_v1));
	mock_ec_event_fifo_start++;

	size_t data_size;
	switch (e->event_type) {
	case EC_MKBP_EVENT_KEY_MATRIX:
		data_size = sizeof(e->data.key_matrix);
		break;
	case EC_MKBP_EVENT_SENSOR_FIFO:
		data_size = sizeof(e->data.sensor_fifo);
		break;
	case EC_MKBP_EVENT_BUTTON:
		data_size = sizeof(e->data.buttons);
		break;
	default:
		fail_msg("Mock event_type %d is not supported", e->event_type);
	}

	if (mock_ec_event_fifo_start < mock_ec_event_fifo_end)
		e->event_type |= EC_MKBP_HAS_MORE_EVENTS;
	else
		mock_ec_interrupt = 0;

	return sizeof(e->event_type) + data_size;
}

/* Setup function. */

static int setup(void **state)
{
	/*
	 * A hack to reset the static variables 'combo_detected' and
	 * 'prev_timestamp_us' in mkbp_add_button(), by simulating button
	 * release.
	 */
	prev_bitmap = 0x1;
	mock_timer_ms = 0;
	mkbp_add_button(NULL, 0, 0x0);
	/* Reset global variables. */
	prev_bitmap = 0;
	memset(key_fifo, 0, sizeof(key_fifo));
	fifo_offset = 0;
	fifo_size = 0;
	/* Reset mock variables. */
	mock_ec_interrupt = 0;
	memset(mock_ec_event_fifo, 0, sizeof(mock_ec_event_fifo));
	mock_ec_event_fifo_start = 0;
	mock_ec_event_fifo_end = 0;
	get_next_event_called = false;
	get_next_event_called_with_empty_fifo = false;
	memset(mock_keys, 0, sizeof(mock_keys));
	mock_buttons = 0;
	return 0;
}

/* Test utilities. */

static void add_event_to_fifo(const struct ec_response_get_next_event_v1 *event)
{
	if (mock_ec_event_fifo_end >= MOCK_EC_FIFO_SIZE)
		fail_msg("Mock EC event fifo is full");
	/* EC_MKBP_HAS_MORE_EVENTS is set in cros_ec_get_next_event(). */
	if (event->event_type & EC_MKBP_HAS_MORE_EVENTS)
		fail_msg("Invalid mock EC event type: %#x", event->event_type);

	memcpy(&mock_ec_event_fifo[mock_ec_event_fifo_end], event,
	       sizeof(*event));
	mock_ec_event_fifo_end++;
	mock_ec_interrupt = 1;
}

static void add_key_matrix_event_to_fifo(const uint8_t *key_matrix)
{
	struct ec_response_get_next_event_v1 event = {
		.event_type = EC_MKBP_EVENT_KEY_MATRIX,
	};
	memcpy(event.data.key_matrix, key_matrix, MOCK_EC_KEY_MATRIX_SIZE);
	add_event_to_fifo(&event);
}

static void add_button_event_to_fifo(uint32_t buttons)
{
	struct ec_response_get_next_event_v1 event = {
		.event_type = EC_MKBP_EVENT_BUTTON,
		.data.buttons = buttons,
	};
	add_event_to_fifo(&event);
}

static size_t get_key_matrix_index(unsigned int col, unsigned int row,
				   unsigned int *bit)
{
	if (col >= mkbp_keymatrix.cols)
		fail_msg("col %d should be less than %d",
			 col, mkbp_keymatrix.cols);
	if (row >= mkbp_keymatrix.rows)
		fail_msg("row %d should be less than %d",
			 row, mkbp_keymatrix.rows);

	size_t count = col * mkbp_keymatrix.rows + row;
	size_t index = count / 8;
	if (index >= MOCK_EC_KEY_MATRIX_SIZE)
		fail_msg("key matrix index %d should be less than %d\n",
			 index, MOCK_EC_KEY_MATRIX_SIZE);
	*bit = count % 8;
	return index;
}

static void press_key(unsigned int col, unsigned int row)
{
	size_t index;
	unsigned int bit;
	index = get_key_matrix_index(col, row, &bit);
	mock_keys[index] |= 1U << bit;
	add_key_matrix_event_to_fifo(mock_keys);
}

static void release_key(unsigned int col, unsigned int row)
{
	size_t index;
	unsigned int bit;
	index = get_key_matrix_index(col, row, &bit);
	mock_keys[index] &= ~(1U << bit);
	add_key_matrix_event_to_fifo(mock_keys);
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

/* Test functions. */

#define ASSERT_GETCHAR(c) do { \
	assert_true(mkbp_keyboard.havekey()); \
	assert_int_equal(mkbp_keyboard.getchar(), c); \
} while (0)

#define ASSERT_NO_MORE_CHAR() assert_false(mkbp_keyboard.havekey())

struct mkbp_key_test_state {
	unsigned int col, row;
	int expected_char;
};

static void test_key(void **state)
{
	const struct mkbp_key_test_state *key_state = *state;
	unsigned int col = key_state->col;
	unsigned int row = key_state->row;

	/* Key is reported on press. */
	press_key(col, row);
	ASSERT_GETCHAR(key_state->expected_char);
	release_key(col, row);
	ASSERT_NO_MORE_CHAR();
}

static void test_multiple_keys(void **state)
{
	press_key(1, 4);
	ASSERT_GETCHAR('a');

	press_key(2, 4);
	ASSERT_GETCHAR('d');

	press_key(3, 4);
	ASSERT_GETCHAR('f');

	release_key(3, 4);
	release_key(1, 4);
	release_key(2, 4);
	ASSERT_NO_MORE_CHAR();
}

static void test_ctrl_d(void **state)
{
	press_key(14, 1);
	ASSERT_NO_MORE_CHAR();

	press_key(2, 4);
	ASSERT_GETCHAR('D' & 0x1f);

	release_key(2, 4);
	release_key(14, 1);
	ASSERT_NO_MORE_CHAR();
}

static void test_shift_d(void **state)
{
	press_key(7, 5);
	ASSERT_NO_MORE_CHAR();

	press_key(2, 4);
	ASSERT_GETCHAR('D');

	release_key(2, 4);
	release_key(7, 5);
	ASSERT_NO_MORE_CHAR();
}

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

static void test_power_long_press(void **state)
{
	mock_timer_ms = 1111;
	press_button(EC_MKBP_POWER_BUTTON);
	ASSERT_NO_MORE_CHAR();

	/* Press longer than 3 seconds is considered a long press.
	   However power button long press is not supported. */
	mock_timer_ms += 3000;
	ASSERT_NO_MORE_CHAR();

	release_button(EC_MKBP_POWER_BUTTON);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_up_long_press(void **state)
{
	mock_timer_ms = 2222;
	press_button(EC_MKBP_VOL_UP);
	ASSERT_NO_MORE_CHAR();

	mock_timer_ms += 4000;
	ASSERT_GETCHAR(UI_BUTTON_VOL_UP_LONG_PRESS);
	ASSERT_NO_MORE_CHAR();

	release_button(EC_MKBP_VOL_UP);
	ASSERT_NO_MORE_CHAR();
}

static void test_vol_down_long_press(void **state)
{
	mock_timer_ms = 3333;
	press_button(EC_MKBP_VOL_DOWN);
	ASSERT_NO_MORE_CHAR();

	mock_timer_ms += 5000;
	ASSERT_GETCHAR(UI_BUTTON_VOL_DOWN_LONG_PRESS);
	ASSERT_NO_MORE_CHAR();

	release_button(EC_MKBP_VOL_DOWN);
	ASSERT_NO_MORE_CHAR();
}

static void test_no_events(void **state)
{
	ASSERT_NO_MORE_CHAR();
	assert_false(get_next_event_called);
}

static void test_no_more_events(void **state)
{
	/* Add an event that will be ignored, and without the
	   EC_MKBP_HAS_MORE_EVENTS flag. */
	struct ec_response_get_next_event_v1 event = {
		.event_type = EC_MKBP_EVENT_SENSOR_FIFO,
	};
	add_event_to_fifo(&event);
	ASSERT_NO_MORE_CHAR();

	/* cros_ec_get_next_event() shouldn't be called again if
	   EC_MKBP_HAS_MORE_EVENTS isn't set. */
	assert_false(get_next_event_called_with_empty_fifo);
}

#define MKBP_KEY_TEST(_col, _row, _expected_char) { \
	.name = ("test_key-" #_expected_char), \
	.test_func = test_key, \
	.setup_func = setup, \
	.teardown_func = NULL, \
	.initial_state = (&(struct mkbp_key_test_state) { \
		.col = _col, \
		.row = _row, \
		.expected_char = _expected_char, \
	}), \
}

#define MKBP_TEST(func) cmocka_unit_test_setup(func, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Key matrix tests. */
		MKBP_KEY_TEST(1, 1, 0x1b),	// ESC
		MKBP_KEY_TEST(1, 5, 'z'),
		MKBP_KEY_TEST(11, 4, '\n'),
		MKBP_KEY_TEST(11, 5, ' '),
		MKBP_KEY_TEST(11, 7, KEY_UP),
		MKBP_KEY_TEST(11, 6, KEY_DOWN),
		MKBP_KEY_TEST(12, 7, KEY_LEFT),
		MKBP_KEY_TEST(12, 6, KEY_RIGHT),
		MKBP_TEST(test_multiple_keys),
		MKBP_TEST(test_ctrl_d),
		MKBP_TEST(test_shift_d),
		/* Button tests. */
		MKBP_TEST(test_power_short_press),
		MKBP_TEST(test_vol_up_short_press),
		MKBP_TEST(test_vol_down_short_press),
		MKBP_TEST(test_vol_up_down_combo),
		MKBP_TEST(test_undefined_combo),
		MKBP_TEST(test_power_long_press),
		MKBP_TEST(test_vol_up_long_press),
		MKBP_TEST(test_vol_down_long_press),
		/* Misc tests. */
		MKBP_TEST(test_no_events),
		MKBP_TEST(test_no_more_events),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
