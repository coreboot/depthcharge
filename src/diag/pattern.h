/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __DIAG_PATTERN_H__
#define __DIAG_PATTERN_H__

#include "base/list.h"

typedef struct Pattern {
	const char *name;

	const uint32_t *data;
	size_t len;

	ListNode list_node;
} Pattern;

const ListNode *DiagGetSimpleTestPatterns(void);
const ListNode *DiagGetTestPatterns(void);

#endif
