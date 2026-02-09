/* SPDX-License-Identifier: BSD-3-Clause */

#include <libpayload.h>
#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"

#define LPASS_AON_CC_LPASS_CORE_HM_COLLAPSE_VOTE_FOR_Q6	((void *)0x06E14000)
#define LPASS_CORE_HM_GDSCR_ADDR			((void *)0x07E00000)
#define GCC_LPASS_CFG_NOC_SWAY_CBCR_ADDR		((void *)0x00147000)
#define LPASS_TOP_CC_LPASS_CORE_SWAY_AHB_LS_CBCR_ADDR	((void *)0x07E09000)

#define LPASS_CORE_HM_VOTE_POWER_OFF	0x1
#define GDSC_PWR_ON			(1 << 31)
#define CLK_CTL_EN_SHFT			0
#define HW_CTL				(1 << 1)

#define GDSC_TIMEOUT_US			150000

static int wait_for_condition(uint32_t timeout_us, int condition)
{
	uint64_t start = timer_us(0);

	while (!condition) {
		if (timer_us(start) > timeout_us)
			return 0;
		udelay(1);
	}
	return 1;
}

static int lpass_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	printf("LPASS: Performing cleanup before handoff\n");

	write32(LPASS_AON_CC_LPASS_CORE_HM_COLLAPSE_VOTE_FOR_Q6, LPASS_CORE_HM_VOTE_POWER_OFF);
	if (!wait_for_condition(GDSC_TIMEOUT_US,
				(read32(LPASS_CORE_HM_GDSCR_ADDR) & GDSC_PWR_ON))) {
		printf("LPASS: Core HM GDSC PWR_OFF timeout after vote\n");
		return -1;
	}

	clrbits32(GCC_LPASS_CFG_NOC_SWAY_CBCR_ADDR, (1 << CLK_CTL_EN_SHFT));

	clrbits32(LPASS_TOP_CC_LPASS_CORE_SWAY_AHB_LS_CBCR_ADDR, HW_CTL);

	printf("LPASS: Cleanup completed successfully\n");
	return 0;
}

static int lpass_cleanup_install(void)
{
	static CleanupFunc dev = {
		&lpass_cleanup,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(lpass_cleanup_install);
