## SPDX-License-Identifier: GPL-2.0

depthcharge-y += board.c storage.c

ifneq ($(CONFIG_VARIANT_NAME), "")
depthcharge-y += $(call strip_quotes,$(CONFIG_VARIANT_NAME)).c
endif
