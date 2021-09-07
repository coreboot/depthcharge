# SPDX-License-Identifier: GPL-2.0

tests-y += menu-test
tests-y += menu-detachable-test
tests-y += navigation-test
tests-y += loop-test
tests-y += loop-detachable-test
tests-y += screens-test
tests-y += bitmap-test

menu-test-srcs += tests/vboot/ui/menu-test.c
menu-test-srcs += tests/vboot/ui/mock_screens.c
menu-test-srcs += src/vboot/ui/menu.c
menu-test-mocks += ui_screen_change

$(call copy-test,menu-test,menu-detachable-test)
menu-detachable-test-config += CONFIG_DETACHABLE=1

navigation-test-srcs += tests/vboot/ui/navigation-test.c
navigation-test-srcs += tests/vboot/ui/mock_screens.c
navigation-test-srcs += src/vboot/ui/navigation.c
navigation-test-srcs += src/vboot/ui/menu.c

loop-test-srcs += tests/vboot/ui/loop-test.c
loop-test-srcs += tests/vboot/ui/mock_screens.c
loop-test-srcs += tests/mocks/ui.c
loop-test-srcs += tests/mocks/ui_callbacks.c
loop-test-srcs += tests/mocks/ui_display.c
loop-test-srcs += tests/mocks/vb2api.c
loop-test-srcs += src/vboot/ui/navigation.c
loop-test-srcs += src/vboot/ui/menu.c
loop-test-mocks += VbExIsShutdownRequest
loop-test-mocks += vb2api_gbb_get_flags
loop-test-mocks += vb2api_get_locale_id
loop-test-mocks += ui_keyboard_read

$(call copy-test,loop-test,loop-detachable-test)
loop-detachable-test-config += CONFIG_DETACHABLE=1

screens-test-srcs += tests/vboot/ui/screens-test.c
screens-test-srcs += tests/mocks/ui_callbacks.c
screens-test-srcs += src/vboot/ui/navigation.c

bitmap-test-srcs += tests/vboot/ui/bitmap-test.c
bitmap-test-srcs += src/vboot/ui/bitmap.c
