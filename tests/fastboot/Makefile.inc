# SPDX-License-Identifier: GPL-2.0

tests-y += cmd-test

cmd-test-config += CONFIG_UIP_MAX_TCP_MSS=1
cmd-test-config += CONFIG_UIP_DEFAULT_RECEIVE_WINDOW=1
cmd-test-config += CONFIG_UIP_DEFAULT_BUFSIZE=1
cmd-test-config += CONFIG_UIP_LLH_LEN=14
cmd-test-config += CONFIG_UIP_CONNS=10
cmd-test-config += CONFIG_UIP_UDP_CONNS=10
cmd-test-config += CONFIG_UIP_LINK_MTU=1500
cmd-test-config += CONFIG_UIP_STATISTICS=0
cmd-test-config += CONFIG_DRIVER_STORAGE_UFS=1
/* TODO(b/430265340): Fix UIP #if guards that doesn't use Kconfig options */
cmd-test-config += UIP_CONF_LL_802154=0
cmd-test-config += UIP_CONF_LL_80211=0
cmd-test-config += UIP_CONF_ICMP6=0
cmd-test-srcs += tests/fastboot/cmd-test.c
cmd-test-srcs += src/base/android_misc.c
cmd-test-srcs += src/fastboot/cmd.c
cmd-test-srcs += src/fastboot/fastboot.c
cmd-test-mocks += GptUpdateKernelWithEntry
cmd-test-mocks += GetEntryPriority
cmd-test-mocks += android_misc_bcb_write
cmd-test-mocks += android_misc_bcb_read
cmd-test-mocks += android_misc_oem_cmdline_write
cmd-test-mocks += android_misc_oem_cmdline_read
cmd-test-mocks += android_misc_get_oem_cmd
cmd-test-mocks += android_misc_append_oem_cmd
cmd-test-mocks += android_misc_del_oem_cmd
cmd-test-mocks += android_misc_set_oem_cmd
cmd-test-mocks += android_misc_reset_oem_cmd
