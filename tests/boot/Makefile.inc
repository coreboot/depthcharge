# SPDX-License-Identifier: GPL-2.0

tests-y += memchipinfo-test payload-test

memchipinfo-test-srcs += tests/boot/memchipinfo.c
memchipinfo-test-srcs += tests/helpers/device_tree.c

payload-test-srcs += tests/boot/payload-test.c
payload-test-srcs += tests/stubs/libpayload/cache.c
payload-test-srcs += src/boot/payload.c
