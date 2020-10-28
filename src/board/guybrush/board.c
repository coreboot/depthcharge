// SPDX-License-Identifier: GPL-2.0
/* Copyright 2020 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"


static int board_setup(void)
{
	return 0;
}

INIT_FUNC(board_setup);
