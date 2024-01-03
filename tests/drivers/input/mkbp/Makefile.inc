## SPDX-License-Identifier: GPL-2.0

tests-y += mkbp-test

mkbp-test-srcs += src/drivers/input/mkbp/keymatrix/standard.c
mkbp-test-srcs += src/drivers/input/mkbp/layout/us.c
mkbp-test-srcs += tests/drivers/input/mkbp/mkbp-test.c
