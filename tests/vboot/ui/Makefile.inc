# SPDX-License-Identifier: GPL-2.0

tests-y += menu-test
tests-y += menu-detachable-test
tests-y += navigation-test

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
navigation-test-srcs += src/vboot/callbacks/debug.c
