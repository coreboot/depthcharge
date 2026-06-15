#ifndef __DRIVERS_SOC_QCOM_SOC_H__
#define __DRIVERS_SOC_QCOM_SOC_H__

#if CONFIG(DRIVER_SOC_X1P42100)
#include "drivers/soc/x1p42100.h"
#elif CONFIG(DRIVER_SOC_CALYPSO)
#include "drivers/soc/calypso.h"
#endif

#endif
