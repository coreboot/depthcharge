# SPDX-License-Identifier: GPL-2.0

tests-y += bootconfig-test commandline-test memchipinfo-test payload-test

bootconfig-test-srcs += tests/boot/bootconfig-test.c
bootconfig-test-srcs += src/boot/bootconfig.c

commandline-test-srcs += tests/boot/commandline-test.c
commandline-test-srcs += src/boot/commandline.c

memchipinfo-test-srcs += tests/boot/memchipinfo.c
memchipinfo-test-srcs += tests/helpers/device_tree.c

payload-test-srcs += tests/boot/payload-test.c
payload-test-srcs += tests/stubs/libpayload/cache.c
payload-test-srcs += src/boot/payload.c
