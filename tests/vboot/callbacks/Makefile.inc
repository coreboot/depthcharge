# SPDX-License-Identifier: GPL-2.0

tests-y += ui_broken-test
tests-y += ui_broken-detachable-test
tests-y += ui_manual-test
tests-y += ui_manual-detachable-test
tests-y += ui_manual-ppkeyboard-test
tests-y += ui_developer-test
tests-y += ui_developer-detachable-test
tests-y += ui_diagnostic-test

ui-test-common-srcs += tests/mocks/ui_callbacks.c
ui-test-common-srcs += tests/mocks/vb2api.c
ui-test-common-srcs += tests/mocks/vboot_api.c
ui-test-common-srcs += tests/stubs/boot/payload.c
ui-test-common-srcs += tests/stubs/diag.c
ui-test-common-srcs += tests/stubs/vboot/ui/archive.c
ui-test-common-srcs += tests/stubs/vboot/ui/draw.c
ui-test-common-srcs += tests/stubs/vboot/ui/layout.c
ui-test-common-srcs += tests/stubs/vboot/ui/log.c
ui-test-common-srcs += src/vboot/callbacks/debug.c
ui-test-common-srcs += src/vboot/ui/loop.c
ui-test-common-srcs += src/vboot/ui/menu.c
ui-test-common-srcs += src/vboot/ui/navigation.c
ui-test-common-srcs += src/vboot/ui/screens.c

ui-test-common-mocks += vb2api_gbb_get_flags
ui-test-common-mocks += vb2api_get_locale_id
ui-test-common-mocks += vb2api_set_locale_id
ui-test-common-mocks += vb2api_request_diagnostics
ui-test-common-mocks += vb2api_diagnostic_ui_enabled
ui-test-common-mocks += vb2api_enable_developer_mode
ui-test-common-mocks += vb2api_disable_developer_mode
ui-test-common-mocks += vb2api_phone_recovery_ui_enabled
ui-test-common-mocks += vb2api_use_short_dev_screen_delay
ui-test-common-mocks += vb2api_get_dev_default_boot_target

# ui_broken-test
$(call copy-test,ui-test-common,ui_broken-test)
ui_broken-test-srcs += tests/vboot/callbacks/ui_broken-test.c
ui_broken-test-srcs += src/vboot/callbacks/ui.c

$(call copy-test,ui_broken-test,ui_broken-detachable-test)
ui_broken-detachable-test-config += CONFIG_DETACHABLE=1

# ui_manual-test
$(call copy-test,ui-test-common,ui_manual-test)
ui_manual-test-srcs += tests/vboot/callbacks/ui_manual-test.c

$(call copy-test,ui_manual-test,ui_manual-detachable-test)
ui_manual-detachable-test-config += CONFIG_DETACHABLE=1

$(call copy-test,ui_manual-test,ui_manual-ppkeyboard-test)
ui_manual-ppkeyboard-test-config += CONFIG_PHYSICAL_PRESENCE_KEYBOARD=1

# ui_developer-test
$(call copy-test,ui-test-common,ui_developer-test)
ui_developer-test-srcs += tests/vboot/callbacks/ui_developer-test.c
ui_developer-test-srcs += src/vboot/callbacks/ui.c

$(call copy-test,ui_developer-test,ui_developer-detachable-test)
ui_developer-detachable-test-config += CONFIG_DETACHABLE=1

# ui_diagnostic-test
$(call copy-test,ui-test-common,ui_diagnostic-test)
ui_diagnostic-test-srcs += tests/vboot/callbacks/ui_diagnostic-test.c
ui_diagnostic-test-srcs += src/vboot/callbacks/ui.c
