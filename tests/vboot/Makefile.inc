# SPDX-License-Identifier: GPL-2.0

subdirs-y := ui callbacks

tests-y += secdata_tpm-test
tests-y += stages-test

secdata_tpm-test-srcs += tests/vboot/secdata_tpm-test.c
secdata_tpm-test-srcs += tests/mocks/tlcl_rw.c
secdata_tpm-test-srcs += src/vboot/secdata_tpm.c
secdata_tpm-test-srcs += src/vboot/callbacks/tpm.c
secdata_tpm-test-mocks += TlclWrite TlclRead TlclForceClear \
	TlclSetEnable TlclSetDeactivated

stages-test-srcs += tests/vboot/stages-test.c
stages-test-mocks += vb2ex_commit_data vb2api_fail
stages-test-config += CONFIG_DRIVER_TPM_GOOGLE=1
