// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
 */

#include <stdint.h>
#include <libpayload.h>

#include "diag/pattern.h"

typedef void (*PatternDataGenerator)(uint32_t **ptr, size_t *len);

/*
static uint32_t walking_ones_data[] = {
	0x00000001, 0x00000002, 0x00000004, 0x00000008,
	0x00000010, 0x00000020, 0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800,
	0x00001000, 0x00002000, 0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000,
	0x00100000, 0x00200000, 0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000,
	0x10000000, 0x20000000, 0x40000000, 0x80000000,
	0x40000000, 0x20000000, 0x10000000, 0x08000000,
	0x04000000, 0x02000000, 0x01000000, 0x00800000,
	0x00400000, 0x00200000, 0x00100000, 0x00080000,
	0x00040000, 0x00020000, 0x00010000, 0x00008000,
	0x00004000, 0x00002000, 0x00001000, 0x00000800,
	0x00000400, 0x00000200, 0x00000100, 0x00000080,
	0x00000040, 0x00000020, 0x00000010, 0x00000008,
	0x00000004, 0x00000002, 0x00000001, 0x00000000
};
*/
static void walking_ones_generator(uint32_t **buf_ret, size_t *len_ret)
{
	int len = 64;
	uint32_t *buf = malloc(len * sizeof(uint32_t));
	for (int i = 0; i < 32; i++) {
		buf[i] = 1u << i;
		buf[64 - i - 2] = 1u << i;
	}
	buf[64 - 1] = 0x00000000;

	*len_ret = len;
	*buf_ret = buf;
}


static uint32_t one_zero_data[] = {0x00000000, 0xffffffff};
static uint32_t zero_data[] = {0x00000000};
static uint32_t one_data[] = {0xffffffff};
static uint32_t five_data[] = {0x55555555};
static uint32_t a_data[] = {0xaaaaaaaa};
static uint32_t five_a_data[] = {0x55555555, 0xaaaaaaaa};
static uint32_t five_a_8_data[] = {
	0x5aa5a55a, 0xa55a5aa5, 0xa55a5aa5, 0x5aa5a55a
};
static uint32_t long8b10b_data[] = {0x16161616};
static uint32_t short8b10b_data[] = {0xb5b5b5b5};
static uint32_t checker8b10b_data[] = {0xb5b5b5b5, 0x4a4a4a4a};
static uint32_t five7_data[] = {0x55555557, 0x55575555};
static uint32_t zero2_fd_data[] = {0x00020002, 0xfffdfffd};

static inline void add_pattern(ListNode *head, const char *name,
			       const uint32_t *buf, const size_t len)
{
	Pattern *pattern = xzalloc(sizeof(*pattern));

	pattern->name = name;
	pattern->data = buf;
	pattern->len = len;

	list_insert_after(&pattern->list_node, head);
}

#define ADD_PATTERN_BY_FUNC(head, name)                                        \
	do {                                                                   \
		uint32_t *ptr;                                                 \
		size_t len;                                                    \
		name##_generator(&ptr, &len);                                  \
		add_pattern(&(head), #name, ptr, len);                         \
		free(ptr);                                                     \
	} while (0)

#define ADD_PATTERN_BY_ARRAY(head, name)                                       \
	add_pattern(&(head), #name, name##_data, ARRAY_SIZE(name##_data))

const ListNode *DiagGetSimpleTestPatterns(void)
{
	static ListNode pattern_list;

	if (!pattern_list.next)
		ADD_PATTERN_BY_ARRAY(pattern_list, five_a_8);

	return &pattern_list;
}

const ListNode *DiagGetTestPatterns(void)
{
	static ListNode pattern_list;

	if (!pattern_list.next) {
		ADD_PATTERN_BY_FUNC(pattern_list, walking_ones);
		ADD_PATTERN_BY_ARRAY(pattern_list, one_zero);
		ADD_PATTERN_BY_ARRAY(pattern_list, zero);
		ADD_PATTERN_BY_ARRAY(pattern_list, one);
		ADD_PATTERN_BY_ARRAY(pattern_list, five);
		ADD_PATTERN_BY_ARRAY(pattern_list, a);
		ADD_PATTERN_BY_ARRAY(pattern_list, five_a);
		ADD_PATTERN_BY_ARRAY(pattern_list, five_a_8);
		ADD_PATTERN_BY_ARRAY(pattern_list, long8b10b);
		ADD_PATTERN_BY_ARRAY(pattern_list, short8b10b);
		ADD_PATTERN_BY_ARRAY(pattern_list, checker8b10b);
		ADD_PATTERN_BY_ARRAY(pattern_list, five7);
		ADD_PATTERN_BY_ARRAY(pattern_list, zero2_fd);
		ADD_PATTERN_BY_ARRAY(pattern_list, one_zero);
	}

	return &pattern_list;
}
