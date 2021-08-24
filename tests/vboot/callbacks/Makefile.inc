# SPDX-License-Identifier: GPL-2.0

tests-y += ui_broken-test
tests-y += ui_broken-detachable-test

ui_broken-test-srcs += tests/vboot/callbacks/ui_broken-test.c
ui_broken-test-srcs += tests/mocks/vb2api.c
ui_broken-test-srcs += tests/mocks/ui_callbacks.c
ui_broken-test-srcs += tests/stubs/boot/payload.c
ui_broken-test-srcs += tests/stubs/vboot/ui/archive.c
ui_broken-test-srcs += tests/stubs/vboot/ui/draw.c
ui_broken-test-srcs += tests/stubs/vboot/ui/layout.c
ui_broken-test-srcs += tests/stubs/vboot/ui/log.c
ui_broken-test-srcs += src/vboot/callbacks/ui.c
ui_broken-test-srcs += src/vboot/ui/loop.c
ui_broken-test-srcs += src/vboot/ui/menu.c
ui_broken-test-srcs += src/vboot/ui/navigation.c
ui_broken-test-srcs += src/vboot/ui/screens.c
ui_broken-test-mocks += vb2api_gbb_get_flags
ui_broken-test-mocks += vb2api_get_locale_id

$(call copy-test,ui_broken-test,ui_broken-detachable-test)
ui_broken-detachable-test-config += CONFIG_DETACHABLE=1
