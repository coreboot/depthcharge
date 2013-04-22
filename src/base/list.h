/*
 * Copyright 2012 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __BASE_LIST_H__
#define __BASE_LIST_H__

#include <stddef.h>
#include <stdint.h>

#define container_of(ptr, type, member) ({                         \
	const typeof(((type *)0)->member) *__mptr = (ptr);         \
	(type *)((uint8_t *)__mptr - offsetof(type, member) );})   \

typedef struct ListNode {
	struct ListNode *next;
	struct ListNode *prev;
} ListNode;

// Remove ListNode node from the doubly linked list it's a part of.
void list_remove(ListNode *node);
// Insert ListNode node after ListNode after in a doubly linked list.
void list_insert_after(ListNode *node, ListNode *after);
// Insert ListNode node before ListNode before in a doubly linked list.
void list_insert_before(ListNode *node, ListNode *before);

#define list_for_each(ptr, head, member)                                \
	for ((ptr) = container_of((head).next, typeof(*(ptr)), member); \
		&((ptr)->member);                                       \
		(ptr) = container_of((ptr)->member.next,                \
			typeof(*(ptr)), member))

#endif /* __BASE_LIST_H__ */
