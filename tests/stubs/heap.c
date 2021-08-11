// SPDX-License-Identifier: GPL-2.0

#include <stddef.h>
#include <tests/test.h>

/* Heap for libpayload libc. It should suffice for internals.
   Buffers for testing purposes should be allocades using test_malloc()
   or test_calloc() and freed using test_free(). */
TEST_REGION(heap, 16 * MiB);
