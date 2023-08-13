/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DRIVERS_SOC_CSE_ME_SPEC_H__
#define __DRIVERS_SOC_CSE_ME_SPEC_H__

#include <stdint.h>

#if CONFIG_ME_SPEC == 12
#include "me_12.h"
#elif CONFIG_ME_SPEC == 13
#include "me_13.h"
#elif CONFIG_ME_SPEC == 15
#include "me_15.h"
#elif CONFIG_ME_SPEC == 16
#include "me_16.h"
#elif CONFIG_ME_SPEC == 18
#include "me_18.h"
#endif

#endif /* __DRIVERS_SOC_CSE_ME_SPEC_H__ */
