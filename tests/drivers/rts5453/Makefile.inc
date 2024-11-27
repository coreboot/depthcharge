# SPDX-License-Identifier: GPL-2.0

tests-y += rts5453-test

rts5453-test-srcs += src/drivers/ec/rts5453/rts5453.c
rts5453-test-srcs += tests/drivers/rts5453/rts5453-test.c

rts5453-test-config += CONFIG_DRIVER_EC_RTS545X_VID=0
rts5453-test-config += CONFIG_DRIVER_EC_RTS545X_PID=0
