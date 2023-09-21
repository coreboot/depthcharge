# SPDX-License-Identifier: GPL-2.0

tests-y += memmapped-test

memmapped-test-srcs += src/drivers/flash/flash.c
memmapped-test-srcs += tests/drivers/flash/memmapped-test.c
memmapped-test-mocks += flash_set_ops
memmapped-test-config += CONFIG_DRIVER_FLASH_MEMMAPPED=1
