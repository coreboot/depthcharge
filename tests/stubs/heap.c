// SPDX-License-Identifier: GPL-2.0

#include <stddef.h>

/* Heap for libpayload libc. It should suffice for internals.
   Buffers for testing purposes should be allocades using test_malloc()
   or test_calloc() and freed using test_free(). */
char _heap[16 * MiB];
char *_eheap = _heap + sizeof(_heap);
