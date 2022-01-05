// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "board/guybrush/include/variant.h"

#include "drivers/sound/rt5682.h"
#include "drivers/sound/rt5682s.h"

SoundRouteComponent *variant_get_audio_codec(I2cOps *i2c, uint8_t chip,
					     uint32_t mclk, uint32_t lrclk)
{
	if (lib_sysinfo.board_id <= 2) {
		rt5682Codec *rt5682 =
			new_rt5682_codec(i2c, chip, mclk, lrclk);

		return &rt5682->component;
	}

	rt5682sCodec *rt5682s =
		new_rt5682s_codec(i2c, chip, mclk, lrclk);

	return &rt5682s->component;
}
