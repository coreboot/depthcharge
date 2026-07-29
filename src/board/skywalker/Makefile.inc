# SPDX-License-Identifier: GPL-2.0-only OR MIT

depthcharge-y += board.c

ifneq ($(CONFIG_VARIANT_NAME), "")
depthcharge-y += $(call strip_quotes,$(CONFIG_VARIANT_NAME)).c
endif

depthcharge-$(CONFIG_USE_PANEL_BOE_NV140FHM) += panel_boe_nv140fhm.c
