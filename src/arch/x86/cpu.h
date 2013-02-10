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

#ifndef __ARCH_X86_CPU_H__
#define __ARCH_X86_CPU_H__

#define MTRR_TYPE_WP		5
#define MTRRcap_MSR		0xfe
#define MTRRphysBase_MSR(reg)	(0x200 + 2 * (reg))
#define MTRRphysMask_MSR(reg)	(0x200 + 2 * (reg) + 1)

/*
 * The memory clobber prevents the GCC from reordering the read/write order
 * of CR0.
 */
static inline unsigned long read_cr0(void)
{
        unsigned long cr0;
        asm volatile ("movl %%cr0, %0" : "=r" (cr0) : : "memory");
        return cr0;
}

static inline void write_cr0(unsigned long cr0)
{
        asm volatile ("movl %0, %%cr0" : : "r" (cr0) : "memory");
}

static inline void wbinvd(void)
{
        asm volatile ("wbinvd" : : : "memory");
}

static inline void invd(void)
{
        asm volatile("invd" : : : "memory");
}

static inline void enable_cache(void)
{
        unsigned long cr0;
        cr0 = read_cr0();
        cr0 &= 0x9fffffff;
        write_cr0(cr0);
}

static inline void disable_cache(void)
{
        /* Disable and write back the cache */
        unsigned long cr0;
        cr0 = read_cr0();
        cr0 |= 0x40000000;
        wbinvd();
        write_cr0(cr0);
        wbinvd();
}

#endif /* __ARCH_X86_CPU_H__ */
