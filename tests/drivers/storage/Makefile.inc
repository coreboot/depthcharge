# SPDX-License-Identifier: GPL-2.0

tests-y += blockdev-test
tests-y += ufs-selftest-test

blockdev-test-srcs += tests/drivers/storage/blockdev-test.c
blockdev-test-srcs += src/drivers/storage/blockdev.c

ufs-selftest-test-srcs += tests/drivers/storage/ufs-selftest.c
ufs-selftest-test-config += CONFIG_DRIVER_STORAGE_UFS=1
