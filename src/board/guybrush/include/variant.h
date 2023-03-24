/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

/* GSC interrupt is attached to either GPIO_85 or GPIO_3 */
#define GSC_INT_85		85
#define GSC_INT_3		3

/* EN_SPKR GPIOs */
#define EN_SPKR			70
#define EN_SPKR_GB		31

unsigned int variant_get_gsc_irq_gpio(void);

SoundRouteComponent *variant_get_audio_codec(I2cOps *i2c, uint8_t chip,
					     uint32_t mclk, uint32_t lrclk);

enum audio_amp {
	AUDIO_AMP_RT1019,
	AUDIO_AMP_MAX98360,
};

enum audio_amp variant_get_audio_amp(void);

unsigned int variant_get_en_spkr_gpio(void);

#endif /* _VARIANT_H_ */
