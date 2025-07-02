# SPDX-License-Identifier: GPL-2.0

tests-y += elog-test
tests-y += sparse-test
tests-y += android_misc-test

elog-test-srcs += tests/mocks/fmap_area.c
elog-test-srcs += tests/base/elog.c

sparse-test-srcs += src/base/sparse.c
sparse-test-srcs += src/drivers/storage/blockdev.c
sparse-test-srcs += tests/base/sparse-test.c
sparse-test-srcs += tests/mocks/test_blockdev.c

android_misc-test-srcs += src/base/android_misc.c
android_misc-test-srcs += tests/base/android_misc-test.c
