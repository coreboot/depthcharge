# SPDX-License-Identifier: GPL-2.0

tests-y += bootconfig-test memchipinfo-test payload-test

bootconfig-test-srcs += tests/boot/bootconfig-test.c
bootconfig-test-srcs += src/boot/bootconfig.c

memchipinfo-test-srcs += tests/boot/memchipinfo.c
memchipinfo-test-srcs += tests/helpers/device_tree.c

payload-test-srcs += tests/boot/payload-test.c
payload-test-srcs += tests/stubs/libpayload/cache.c
payload-test-srcs += src/boot/payload.c
