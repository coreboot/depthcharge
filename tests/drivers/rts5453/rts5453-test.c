// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#include <tests/test.h>

#include "drivers/ec/rts5453/rts5453_internal.h"

#define ARRAY_SIZE(ARR) (sizeof((ARR)) / sizeof((ARR)[0]))

struct rtk_upgrade_test_case {
	pdc_fw_ver_t current;
	pdc_fw_ver_t new;
	bool success;
} upgrade_test_cases[] = {
	/* From < 0.20 */
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(0, 21, 1), false},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(1, 23, 1), false},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Would fail due to config name scheme mismatch */
	{PDC_FWVER_TO_INT(0, 19, 1), PDC_FWVER_TO_INT(16, 0, 1), false},

	/* From == 0.20 */
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(0, 21, 1), true},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(1, 23, 1), true},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Update to prod version OK */
	{PDC_FWVER_TO_INT(0, 20, 1), PDC_FWVER_TO_INT(16, 0, 1), true},

	/* From > 0.20 */
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(0, 21, 1), true},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(1, 23, 1), true},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Update to prod version OK */
	{PDC_FWVER_TO_INT(0, 21, 1), PDC_FWVER_TO_INT(16, 0, 1), true},

	/* From < 1.22 */
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(0, 21, 1), false},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(1, 23, 1), false},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Would fail due to config name scheme mismatch */
	{PDC_FWVER_TO_INT(1, 21, 1), PDC_FWVER_TO_INT(16, 0, 1), false},

	/* From == 1.22 */
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(0, 21, 1), false},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(1, 23, 1), false},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Would fail due to config name scheme mismatch */
	{PDC_FWVER_TO_INT(1, 22, 1), PDC_FWVER_TO_INT(16, 0, 1), false},

	/* From > 1.22 */
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(0, 21, 1), true},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(1, 23, 1), true},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Update to prod version OK */
	{PDC_FWVER_TO_INT(1, 23, 1), PDC_FWVER_TO_INT(16, 0, 1), true},

	/* From == 2.0 */
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(0, 21, 1), true},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(1, 23, 1), true},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Update to prod version OK */
	{PDC_FWVER_TO_INT(2, 0, 1), PDC_FWVER_TO_INT(16, 0, 1), true},

	/* From == 2.1 */
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(0, 21, 1), false},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(1, 23, 1), false},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Would fail due to config name scheme mismatch */
	{PDC_FWVER_TO_INT(2, 1, 1), PDC_FWVER_TO_INT(16, 0, 1), false},

	/* From == 2.2 */
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(0, 19, 1), true},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(0, 20, 1), true},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(0, 21, 1), true},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(1, 21, 1), true},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(1, 22, 1), true},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(1, 23, 1), true},
	/* No upgrade to base FW (2.x) in DC */
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(2, 1, 1), false},
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	/* Update to prod version OK */
	{PDC_FWVER_TO_INT(2, 2, 1), PDC_FWVER_TO_INT(16, 0, 1), true},

	/* Production FW */

	/* Downgrade from prod to internal blocked */
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(0, 0, 1), false},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(0, 20, 1), false},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(1, 0, 1), false},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(1, 22, 1), false},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(2, 0, 1), false},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(2, 2, 1), false},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(3, 0, 1), false},

	/* Upgrade/downgrade within a major version OK */
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(16, 0, 1), true},
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(16, 1, 1), true},
	{PDC_FWVER_TO_INT(16, 1, 1), PDC_FWVER_TO_INT(16, 0, 1), true},
	{PDC_FWVER_TO_INT(16, 1, 1), PDC_FWVER_TO_INT(16, 1, 1), true},

	/* Upgrading major version OK */
	{PDC_FWVER_TO_INT(16, 0, 1), PDC_FWVER_TO_INT(17, 0, 1), true},
	{PDC_FWVER_TO_INT(16, 1, 1), PDC_FWVER_TO_INT(17, 0, 1), true},

	/* Downgrade major prod version blocked */
	{PDC_FWVER_TO_INT(17, 0, 1), PDC_FWVER_TO_INT(16, 0, 1), false},
	{PDC_FWVER_TO_INT(17, 1, 1), PDC_FWVER_TO_INT(16, 0, 1), false},
};

static void test_rtk_upgrade_compatibility(void **state)
{
	for (int i = 0; i < ARRAY_SIZE(upgrade_test_cases); i++) {
		pdc_fw_ver_t curr = upgrade_test_cases[i].current;
		pdc_fw_ver_t new = upgrade_test_cases[i].new;
		bool expected = upgrade_test_cases[i].success;
		bool actual = rts545x_check_update_compatibility(curr, new);

		if (actual != expected) {
			fail_msg("Upgrade from %u.%u.%u to %u.%u.%u should %s but %s",
				 PDC_FWVER_MAJOR(curr), PDC_FWVER_MINOR(curr),
				 PDC_FWVER_PATCH(curr), PDC_FWVER_MAJOR(new),
				 PDC_FWVER_MINOR(new), PDC_FWVER_PATCH(new),
				 expected ? "succeed" : "fail",
				 actual ? "succeeded" : "failed");
		}
	}
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_rtk_upgrade_compatibility),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
