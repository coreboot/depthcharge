# SPDX-License-Identifier: GPL-2.0

subdirs-y := ui

tests-y += load_kernel-test
tests-y += secdata_tpm-test
tests-y += stages-test
tests-y += ui-broken-test
tests-y += ui-broken-detachable-test
tests-y += ui-manual-test
tests-y += ui-manual-detachable-test
tests-y += ui-manual-chromebox-test
tests-y += ui-developer-test
tests-y += ui-developer-detachable-test
tests-y += ui-diagnostic-test

load_kernel-test-srcs += tests/stubs/base/timestamp.c
load_kernel-test-srcs += tests/vboot/load_kernel-test.c
load_kernel-test-srcs += src/vboot/load_kernel.c
load_kernel-test-mocks += vb2api_fail
load_kernel-test-mocks += vb2api_load_kernel vb2api_load_minios_kernel

secdata_tpm-test-srcs += tests/vboot/secdata_tpm-test.c
secdata_tpm-test-srcs += tests/mocks/tlcl_rw.c
secdata_tpm-test-srcs += src/vboot/secdata_tpm.c
secdata_tpm-test-srcs += src/vboot/callbacks/tpm.c
secdata_tpm-test-mocks += TlclWrite TlclRead TlclForceClear \
	TlclSetEnable TlclSetDeactivated

stages-test-srcs += tests/vboot/stages-test.c
stages-test-mocks += vb2ex_commit_data vb2api_fail
stages-test-config += CONFIG_DRIVER_TPM_GOOGLE=1

ui-test-common-srcs += tests/mocks/diag/health_info.c
ui-test-common-srcs += tests/mocks/diag/memory.c
ui-test-common-srcs += tests/mocks/diag/storage_test.c
ui-test-common-srcs += tests/mocks/libpayload/cbmem_console.c
ui-test-common-srcs += tests/mocks/ui.c
ui-test-common-srcs += tests/mocks/ui_callbacks.c
ui-test-common-srcs += tests/mocks/ui_display.c
ui-test-common-srcs += tests/mocks/vb2api.c
ui-test-common-srcs += tests/mocks/vboot/load_kernel.c
ui-test-common-srcs += tests/mocks/vboot/ui/archive.c
ui-test-common-srcs += tests/mocks/vboot/ui/beep.c
ui-test-common-srcs += tests/mocks/vboot/ui/bitmap.c
ui-test-common-srcs += tests/mocks/vboot/ui/layout.c
ui-test-common-srcs += tests/stubs/base/elog.c
ui-test-common-srcs += tests/stubs/boot/payload.c
ui-test-common-srcs += tests/stubs/diag/report.c
ui-test-common-srcs += tests/stubs/drivers/storage/blockdev.c
ui-test-common-srcs += tests/stubs/libpayload/cache.c
ui-test-common-srcs += tests/stubs/vboot/firmware_id.c
ui-test-common-srcs += tests/stubs/vboot/ui/draw.c
ui-test-common-srcs += src/debug/stubs.c
ui-test-common-srcs += src/fastboot/fastboot_stubs.c
ui-test-common-srcs += src/vboot/ui/display.c
ui-test-common-srcs += src/vboot/ui/log.c
ui-test-common-srcs += src/vboot/ui/loop.c
ui-test-common-srcs += src/vboot/ui/menu.c
ui-test-common-srcs += src/vboot/ui/navigation.c
ui-test-common-srcs += src/vboot/ui/screens.c

ui-test-common-mocks += cbmem_console_snapshot
ui-test-common-mocks += dc_dev_enter_firmware_shell
ui-test-common-mocks += dc_dev_firmware_shell_enabled
ui-test-common-mocks += ui_display
ui-test-common-mocks += vb2api_gbb_get_flags
ui-test-common-mocks += vb2api_get_locale_id
ui-test-common-mocks += vb2api_set_locale_id
ui-test-common-mocks += vb2api_request_diagnostics
ui-test-common-mocks += vb2api_diagnostic_ui_enabled
ui-test-common-mocks += vb2api_enable_developer_mode
ui-test-common-mocks += vb2api_disable_developer_mode
ui-test-common-mocks += vb2api_use_short_dev_screen_delay
ui-test-common-mocks += vb2api_get_dev_default_boot_target

# ui-broken-test
$(call copy-test,ui-test-common,ui-broken-test)
ui-broken-test-srcs += tests/vboot/ui-broken-test.c
ui-broken-test-srcs += src/vboot/ui.c
ui-broken-test-srcs := $(filter-out tests/mocks/libpayload/cbmem_console.c, \
	$(ui-broken-test-srcs))

$(call copy-test,ui-broken-test,ui-broken-detachable-test)
ui-broken-detachable-test-config += CONFIG_DETACHABLE=1

# ui-manual-test
$(call copy-test,ui-test-common,ui-manual-test)
ui-manual-test-srcs += tests/vboot/ui-manual-test.c
ui-manual-test-srcs := $(filter-out tests/mocks/vboot/ui/archive.c, \
	$(ui-manual-test-srcs))
ui-manual-test-config += CONFIG_PHYSICAL_PRESENCE_KEYBOARD=1

$(call copy-test,ui-manual-test,ui-manual-detachable-test)
ui-manual-detachable-test-config += CONFIG_PHYSICAL_PRESENCE_KEYBOARD=1
ui-manual-detachable-test-config += CONFIG_DETACHABLE=1

$(call copy-test,ui-manual-test,ui-manual-chromebox-test)
ui-manual-chromebox-test-config += CONFIG_PHYSICAL_PRESENCE_KEYBOARD=0

# ui-developer-test
$(call copy-test,ui-test-common,ui-developer-test)
ui-developer-test-srcs := $(filter-out tests/stubs/boot/payload.c, \
	$(ui-developer-test-srcs))
ui-developer-test-srcs += tests/mocks/boot/payload.c
ui-developer-test-srcs += tests/vboot/ui-developer-test.c
ui-developer-test-srcs += src/vboot/ui.c

$(call copy-test,ui-developer-test,ui-developer-detachable-test)
ui-developer-detachable-test-config += CONFIG_DETACHABLE=1

# ui-diagnostic-test
$(call copy-test,ui-test-common,ui-diagnostic-test)
ui-diagnostic-test-srcs += tests/mocks/base/elog.c
ui-diagnostic-test-srcs += tests/mocks/diag/report.c
ui-diagnostic-test-srcs += tests/vboot/ui-diagnostic-test.c
ui-diagnostic-test-srcs += src/vboot/ui.c
ui-diagnostic-test-srcs := $(filter-out \
	tests/stubs/base/elog.c \
	tests/stubs/diag/report.c \
	tests/stubs/libpayload/cache.c \
	, $(ui-diagnostic-test-srcs))
