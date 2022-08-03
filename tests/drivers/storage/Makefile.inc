# SPDX-License-Identifier: GPL-2.0

tests-y += ufs-selftest-test

ufs-selftest-test-srcs += tests/drivers/storage/ufs-selftest.c
ufs-selftest-test-config += CONFIG_DRIVER_STORAGE_UFS=1
