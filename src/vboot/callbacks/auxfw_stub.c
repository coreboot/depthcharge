// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC.
 *
 * Stub functions for when auxfw.c is not compiled in.
 */

#include <libpayload.h>
#include <vboot_api.h>
#include <vb2_api.h>

static void no_auxfw_soft_sync(void) __attribute__((noreturn));
static void no_auxfw_soft_sync(void)
{
	printf("AUXFW software sync not supported.\n");
	halt();
}

vb2_error_t vb2ex_auxfw_check(enum vb2_auxfw_update_severity *severity)
{
	no_auxfw_soft_sync();
}

vb2_error_t vb2ex_auxfw_update(void)
{
	no_auxfw_soft_sync();
}

vb2_error_t vb2ex_auxfw_vboot_done(int in_recovery)
{
	no_auxfw_soft_sync();
}
