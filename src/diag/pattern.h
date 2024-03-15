/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google LLC
 */

#ifndef __DIAG_PATTERN_H__
#define __DIAG_PATTERN_H__

#include <commonlib/list.h>

typedef struct Pattern {
	const char *name;

	const uint32_t *data;
	size_t len;

	struct list_node list_node;
} Pattern;

const struct list_node *DiagGetSimpleTestPatterns(void);
const struct list_node *DiagGetTestPatterns(void);

#endif
