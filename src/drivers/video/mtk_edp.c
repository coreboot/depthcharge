/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <libpayload.h>
#include <stdbool.h>

#include "mtk_edp.h"

#define MTK_EDP_APB_WSTRB		(0x2010)
#define MTK_EDP_APB_WSTRB_BYTE0		(0x11)
#define MTK_EDP_ENC0_CTRL		(0x3000)
#define MTK_EDP_VIDEO_MUTE_SEL		BIT(3)
#define MTK_EDP_VIDEO_MUTE_SW		BIT(2)
#define MTK_EDP_SECURE_REG11		(0x402c)
#define MTK_EDP_SEC_MUTE_SEL		BIT(4)
#define MTK_EDP_SEC_MUTE_SW		BIT(3)
#define MTK_EDP_TOP_PWR_STATE		(0x2000)
#define MTK_EDP_PHY_CTRL		(0x3f44)
#define MTK_EDP_PHY_OW_EN		BIT(2)
#define MTK_EDP_PHY_OW_VAL_MASK		(BIT(4) | BIT(3))

void mtk_edp_tx_disable(uintptr_t edp_base)
{
	write32p(edp_base + MTK_EDP_APB_WSTRB, MTK_EDP_APB_WSTRB_BYTE0);
	setbits32p(edp_base + MTK_EDP_ENC0_CTRL,
		   MTK_EDP_VIDEO_MUTE_SEL | MTK_EDP_VIDEO_MUTE_SW);
	write32p(edp_base + MTK_EDP_APB_WSTRB, 0);

	write32p(edp_base + MTK_EDP_APB_WSTRB, MTK_EDP_APB_WSTRB_BYTE0);
	setbits32p(edp_base + MTK_EDP_SECURE_REG11,
		   MTK_EDP_SEC_MUTE_SEL | MTK_EDP_SEC_MUTE_SW);
	write32p(edp_base + MTK_EDP_APB_WSTRB, 0);

	write32p(edp_base + MTK_EDP_TOP_PWR_STATE, 0);
	setbits32p(edp_base + MTK_EDP_PHY_CTRL, MTK_EDP_PHY_OW_EN);
	clrbits32p(edp_base + MTK_EDP_PHY_CTRL, MTK_EDP_PHY_OW_VAL_MASK);
	clrbits32p(edp_base + MTK_EDP_PHY_CTRL, MTK_EDP_PHY_OW_EN);
}

bool mtk_edp_is_enabled(uintptr_t edp_base)
{
	uint32_t pwr_state;

	if (!edp_base)
		return false;

	pwr_state = read32p(edp_base + MTK_EDP_TOP_PWR_STATE);
	printf("%s: eDP is %s (TOP_PWR_STATE=0x%x)\n", __func__,
	       pwr_state ? "ENABLED" : "DISABLED", pwr_state);
	return !!pwr_state;
}
