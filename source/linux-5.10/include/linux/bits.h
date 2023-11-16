/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_BITS_H
#define __LINUX_BITS_H

#include <linux/const.h>
#include <vdso/bits.h>
#include <asm/bitsperlong.h>

#define BIT_ULL(nr)		(ULL(1) << (nr))
#define BIT_MASK(nr)		(UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr)	(ULL(1) << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)
#define BITS_PER_BYTE		8

/*
 * Create a contiguous bitmask starting at bit position @l and ending at
 * position @h. For example
 * GENMASK_ULL(39, 21) gives us the 64bit vector 0x000000ffffe00000.
 */
#if !defined(__ASSEMBLY__)
#include <linux/build_bug.h>
#define GENMASK_INPUT_CHECK(h, l) \
	(BUILD_BUG_ON_ZERO(__builtin_choose_expr( \
		__is_constexpr((l) > (h)), (l) > (h), 0)))
#else
/*
 * BUILD_BUG_ON_ZERO is not available in h files included from asm files,
 * disable the input check if that is the case.
 */
#define GENMASK_INPUT_CHECK(h, l) 0
#endif

#define __GENMASK(h, l) \
	(((~UL(0)) - (UL(1) << (l)) + 1) & \
	 (~UL(0) >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK(h, l) \
	(GENMASK_INPUT_CHECK(h, l) + __GENMASK(h, l))

#define __GENMASK_ULL(h, l) \
	(((~ULL(0)) - (ULL(1) << (l)) + 1) & \
	 (~ULL(0) >> (BITS_PER_LONG_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) \
	(GENMASK_INPUT_CHECK(h, l) + __GENMASK_ULL(h, l))

/* Some common bit operation of a (long type) register */
#define writel_clrbits(mask, addr)	writel(readl(addr) & ~(mask), addr)
#define writel_clrbit(bit, addr)	writel_clrbits(bit, addr)
#define writel_bits(val, mask, shift, addr) \
	({ \
		if (val) \
			writel((readl(addr) & ~(mask)) | ((val) << (shift)), \
			       addr); \
		else \
			writel_clrbits(mask, addr); \
	})
#define writel_bit(bit, addr)		writel(readl(addr) | bit, addr)
#define readl_bits(mask, shift, addr)	((readl(addr) & (mask)) >> (shift))
#define readl_bit(bit, addr)		((readl(addr) & bit) ? 1 : 0)

/* Some common bit operation of a variable */
#define clrbits(mask, cur)		((cur) &= ~(mask))
#define clrbit(bit, cur)		clrbits(bit, cur)
#define setbits(val, mask, shift, cur) \
	({ \
		if (val) \
			(cur = (cur & ~(mask)) | ((val) << (shift))); \
		else \
			clrbits(mask, cur); \
	})
#define setbit(bit, cur)		((cur) |= bit)
#define getbits(mask, shift, cur)	(((cur) & (mask)) >> (shift))
#define getbit(bit, cur)		((cur) & (bit) ? 1 : 0)

#endif	/* __LINUX_BITS_H */
