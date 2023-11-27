# SPDX-License-Identifier: GPL-2.0

tests-y += elog-test string-utils-test

elog-test-srcs += tests/mocks/fmap_area.c
elog-test-srcs += tests/base/elog.c

string-utils-test-srcs += src/base/string_utils.c
string-utils-test-srcs += tests/base/string_utils.c
