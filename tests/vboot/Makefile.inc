# SPDX-License-Identifier: GPL-2.0

subdirs-y := ui callbacks

tests-y += secdata_tpm-test

secdata_tpm-test-srcs += tests/vboot/secdata_tpm-test.c
secdata_tpm-test-srcs += tests/mocks/tlcl_rw.c
secdata_tpm-test-srcs += src/vboot/secdata_tpm.c
secdata_tpm-test-srcs += src/vboot/callbacks/tpm.c
secdata_tpm-test-mocks += TlclWrite TlclRead TlclForceClear \
	TlclSetEnable TlclSetDeactivated
