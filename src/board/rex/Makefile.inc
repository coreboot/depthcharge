depthcharge-y += board.c audio.c

ifneq ($(CONFIG_VARIANT_NAME), "")
depthcharge-y += $(call strip_quotes,$(CONFIG_VARIANT_NAME)).c
endif
