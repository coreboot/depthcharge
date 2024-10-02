// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

void dmb(void) {}
void dsb(void) {}
void dcache_clean_all(void) {}
void dcache_clean_by_mva(void const *addr, size_t len) {}
void dcache_invalidate_all(void) {}
void dcache_invalidate_by_mva(void const *addr, size_t len) {}
void dcache_clean_invalidate_all(void) {}
void dcache_clean_invalidate_by_mva(void const *addr, size_t len) {}
void cache_sync_instructions(void) {}
