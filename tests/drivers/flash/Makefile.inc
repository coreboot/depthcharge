# SPDX-License-Identifier: GPL-2.0

tests-y += memmapped-test

memmapped-test-srcs += src/drivers/flash/flash.c
memmapped-test-srcs += src/drivers/flash/memmapped.c
memmapped-test-srcs += tests/drivers/flash/memmapped-test.c
