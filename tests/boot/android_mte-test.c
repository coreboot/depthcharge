// SPDX-License-Identifier: GPL-2.0

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "base/android_misc.h"
#include "boot/android_mte.h"
#include "boot/commandline.h"
#include "vboot/util/memory.h"
#include "tests/test.h"

/* Mocks */
int get_cmdline_uint32_value(const char *cmdline, const char *key, uint32_t *value)
{
	check_expected(key);
	*value = mock_type(uint32_t);
	return mock_type(int);
}

bool memory_has_tag_storage(void)
{
	return mock_type(bool);
}

void memory_free_tag_storage(void)
{
}

int android_misc_system_space_read(BlockDev *bdev, GptData *gpt,
				   struct misc_system_space *space)
{
	struct misc_system_space *mock_space = mock_ptr_type(struct misc_system_space *);
	if (mock_space) {
		memcpy(space, mock_space, sizeof(*space));
		return 0;
	}
	return -1;
}

int android_misc_system_space_write(BlockDev *bdev, GptData *gpt,
				    struct misc_system_space *space)
{
	return 0;
}

/* Helper to setup misc partition state */
static void setup_misc_mte(bool valid, uint32_t mode)
{
	static struct misc_system_space space;
	memset(&space, 0, sizeof(space));

	if (valid) {
		space.memtag_message.version = ANDROID_MISC_MEMTAG_MESSAGE_VERSION;
		space.memtag_message.magic = ANDROID_MISC_MEMTAG_MAGIC;
		space.memtag_message.memtag_mode = mode;
		will_return(android_misc_system_space_read, &space);
	} else {
		will_return(android_misc_system_space_read, NULL);
	}

	android_mte_get_misc_ctrl(NULL, NULL);
}

#define DEFAULT_ON	0x0001
#define DEFAULT_OFF	0x0000

#define MISC_EMPTY	0x0
#define MISC_ON		ANDROID_MISC_MEMTAG_MODE_MEMTAG
#define MISC_OFF	ANDROID_MISC_MEMTAG_MODE_MEMTAG_OFF

struct mte_test_state {
	uint32_t default_mode;
	uint32_t misc_mode;
	bool expect_enabled;
};

static void test_mte(void **state)
{
	const struct mte_test_state *test_state = *state;
	char cmdline[1024] = "existing_param=1";

	if (test_state->misc_mode == MISC_EMPTY)
		setup_misc_mte(false, 0);
	else
		setup_misc_mte(true, test_state->misc_mode);

	expect_string(get_cmdline_uint32_value, key, "bl.mtectrl");
	will_return(get_cmdline_uint32_value, test_state->default_mode);
	will_return(get_cmdline_uint32_value, 0);

	will_return(memory_has_tag_storage, true);

	int ret = android_mte_setup(cmdline, sizeof(cmdline));
	assert_int_equal(ret, 0);

	if (test_state->expect_enabled)
		assert_null(strstr(cmdline, "arm64.nomte"));
	else
		assert_non_null(strstr(cmdline, "arm64.nomte"));
}

#define MTE_TEST(_default, _misc, _expect) { \
	.name = "MTE_TEST_" #_default "_" #_misc, \
	.test_func = test_mte, \
	.setup_func = NULL, \
	.teardown_func = NULL, \
	.initial_state = (&(struct mte_test_state) { \
		.default_mode = _default, \
		.misc_mode = _misc, \
		.expect_enabled = _expect, \
	}), \
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		MTE_TEST(DEFAULT_ON, MISC_EMPTY, true),
		MTE_TEST(DEFAULT_ON, MISC_OFF, false),
		MTE_TEST(DEFAULT_ON, MISC_ON, true),
		MTE_TEST(DEFAULT_OFF, MISC_EMPTY, false),
		MTE_TEST(DEFAULT_OFF, MISC_OFF, false),
		MTE_TEST(DEFAULT_OFF, MISC_ON, true),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
