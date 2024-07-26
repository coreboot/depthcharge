## SPDX-License-Identifier: GPL-2.0

tests-y += mkbp-keymatrixv30-test
tests-y += mkbp-keymatrixv30-tkl-test
tests-y += mkbp-test

mkbp-test-common-srcs += src/drivers/input/mkbp/buttons.c
mkbp-test-common-srcs += src/drivers/input/mkbp/layout/us.c
mkbp-test-common-srcs += tests/drivers/input/mkbp/mkbp-test.c

mkbp-test-common-mocks += timer_us

# mkbp-test
$(call copy-test,mkbp-test-common,mkbp-test)
mkbp-test-srcs += src/drivers/input/mkbp/keymatrix/standard_v10.c
mkbp-test-config += CONFIG_DRIVER_INPUT_MKBP_KEYMATRIX_STANDARD_V10=1

# mkbp-keymatrixv30-test
$(call copy-test,mkbp-test-common,mkbp-keymatrixv30-test)
mkbp-keymatrixv30-test-srcs += src/drivers/input/mkbp/keymatrix/standard_v30.c
mkbp-keymatrixv30-test-config += CONFIG_DRIVER_INPUT_MKBP_KEYMATRIX_STANDARD_V30=1

# mkbp-keymatrixv30-tkl-test
$(call copy-test,mkbp-test-common,mkbp-keymatrixv30-tkl-test)
mkbp-keymatrixv30-tkl-test-srcs += src/drivers/input/mkbp/keymatrix/standard_v30_tkl.c
mkbp-keymatrixv30-tkl-test-config += CONFIG_DRIVER_INPUT_MKBP_KEYMATRIX_STANDARD_V30_TKL=1
