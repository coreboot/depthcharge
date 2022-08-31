/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VARIANT_H_
#define _VARIANT_H_

/* EN_SPKR GPIOs */
#define EN_SPKR			139

enum audio_amp {
	AUDIO_AMP_RT1019,
	AUDIO_AMP_MAX98360,
	AUDIO_AMP_INVALID
};

const struct storage_config *variant_get_storage_configs(size_t *count);

#endif /* _VARIANT_H_ */
