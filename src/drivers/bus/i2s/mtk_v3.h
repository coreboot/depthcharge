/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DRIVERS_BUS_I2S_MTK_V3_H__
#define __DRIVERS_BUS_I2S_MTK_V3_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/route.h"

#if CONFIG(DRIVER_BUS_I2S_MT8189)
#include "drivers/bus/i2s/mt8189.h"
#elif CONFIG(DRIVER_BUS_I2S_MT8196)
#include "drivers/bus/i2s/mt8196.h"
#else
#error "Unsupported BUS I2S config for MediaTek"
#endif

#ifdef MTK_I2S_DEBUG
#define I2S_LOG(args...)	printf(args)
#else
#define I2S_LOG(args...)
#endif

enum {
	AFE_TDM_OUT0,
	AFE_TDM_OUT1,
};

typedef struct {
	SoundRouteComponent component;
	I2sOps ops;
	void *regs;
	uint32_t initialized;
	uint32_t channels;
	uint32_t rate;
	uint32_t word_len;
	uint32_t bit_len;
	uint32_t i2s_num;
} MtkI2s;

struct mtk_afe_rate {
	unsigned int rate;
	unsigned int reg_v_afe;
	unsigned int reg_v_etdm;
	unsigned int reg_v_etdm_relatch;
};

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate,
		    uint32_t word_len, uint32_t bit_len, uint32_t i2s_num);
int mtk_i2s_init(MtkI2s *bus);
void mtk_etdmout_enable(MtkI2s *bus, bool enable,
			uintptr_t buf_base, uintptr_t buf_end);
uintptr_t mtk_etdmout_get_current_buf(MtkI2s *bus);

#endif /* __DRIVERS_BUS_I2S_MTK_V3_H__ */
