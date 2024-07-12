## SPDX-License-Identifier: GPL-2.0-only
depthcharge-$(CONFIG_DRIVER_SOC_INTEL_CSE) += cse.c
depthcharge-$(CONFIG_SOC_INTEL_CSE_LITE_SYNC_BY_PAYLOAD) += cse_lite.c
depthcharge-$(CONFIG_SOC_INTEL_CSE_LITE_SYNC_BY_PAYLOAD) += cse_lite_cmos.c
depthcharge-$(CONFIG_DRIVER_SOC_INTEL_CSE) += cse_spec.c
depthcharge-$(CONFIG_DRIVER_SOC_INTEL_CSE) += disable_heci.c
depthcharge-$(CONFIG_SOC_INTEL_CSE_SEND_EOP_BY_PAYLOAD) += cse_eop.c
