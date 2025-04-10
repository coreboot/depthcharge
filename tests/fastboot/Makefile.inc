# SPDX-License-Identifier: GPL-2.0

tests-y += cmd-test
tests-y += vars-test

fastboot-test-common-config += CONFIG_UIP_MAX_TCP_MSS=1
fastboot-test-common-config += CONFIG_UIP_DEFAULT_RECEIVE_WINDOW=1
fastboot-test-common-config += CONFIG_UIP_DEFAULT_BUFSIZE=1
fastboot-test-common-config += CONFIG_UIP_LLH_LEN=14
fastboot-test-common-config += CONFIG_UIP_CONNS=10
fastboot-test-common-config += CONFIG_UIP_UDP_CONNS=10
fastboot-test-common-config += CONFIG_UIP_LINK_MTU=1500
fastboot-test-common-config += CONFIG_UIP_STATISTICS=0
/* TODO(b/430265340): Fix UIP #if guards that doesn't use Kconfig options */
fastboot-test-common-config += UIP_CONF_LL_802154=0
fastboot-test-common-config += UIP_CONF_LL_80211=0
fastboot-test-common-config += UIP_CONF_ICMP6=0
fastboot-test-common-srcs += tests/fastboot/fastboot_common_mocks.c
fastboot-test-common-srcs += src/fastboot/fastboot.c
fastboot-test-common-mocks += GetEntryPriority
fastboot-test-common-mocks += IsAndroid
fastboot-test-common-mocks += IsBootableEntry

# cmd-test
$(call copy-test,fastboot-test-common,cmd-test)
cmd-test-config += CONFIG_DRIVER_STORAGE_UFS=1
cmd-test-srcs += tests/fastboot/cmd-test.c
cmd-test-srcs += src/base/android_misc.c
cmd-test-srcs += src/fastboot/cmd.c
cmd-test-mocks += GptModified
cmd-test-mocks += GptUpdateKernelWithEntry
cmd-test-mocks += SetEntrySuccessful
cmd-test-mocks += android_misc_bcb_write
cmd-test-mocks += android_misc_bcb_read
cmd-test-mocks += android_misc_oem_cmdline_write
cmd-test-mocks += android_misc_oem_cmdline_read
cmd-test-mocks += android_misc_get_oem_cmd
cmd-test-mocks += android_misc_append_oem_cmd
cmd-test-mocks += android_misc_del_oem_cmd
cmd-test-mocks += android_misc_set_oem_cmd
cmd-test-mocks += android_misc_reset_oem_cmd

# vars-test
$(call copy-test,fastboot-test-common,vars-test)
vars-test-srcs += tests/fastboot/vars-test.c
vars-test-srcs += src/fastboot/vars.c
vars-test-mocks += GptGetEntrySizeBytes
vars-test-mocks += GptInit
vars-test-mocks += GptNextKernelEntry
vars-test-mocks += GetEntryTries
vars-test-mocks += IsBootableEntry
