# SPDX-License-Identifier: GPL-2.0

tests-y += health_info-test
tests-y += health_info-helper-test
tests-y += report-test

health_info-test-srcs += src/diag/health_info.c
health_info-test-srcs += tests/diag/health_info-test.c
health_info-test-config += CONFIG_DRIVER_STORAGE_MMC=1
health_info-test-config += CONFIG_DRIVER_STORAGE_NVME=1
health_info-test-config += CONFIG_DRIVER_STORAGE_UFS=1

health_info-helper-test-srcs += tests/diag/health_info-helper-test.c

report-test-srcs += src/diag/report.c
report-test-srcs += tests/diag/report-test.c
report-test-srcs += tests/mocks/timer.c
