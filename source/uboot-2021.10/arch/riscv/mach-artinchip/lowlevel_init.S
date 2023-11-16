/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <config.h>
#include <linux/linkage.h>

#ifdef CONFIG_32BIT
#define LREG			lw
#define SREG			sw
#define REGBYTES		4
#define RELOC_TYPE		R_RISCV_32
#define SYM_INDEX		0x8
#define SYM_SIZE		0x10
#else
#define LREG			ld
#define SREG			sd
#define REGBYTES		8
#define RELOC_TYPE		R_RISCV_64
#define SYM_INDEX		0x20
#define SYM_SIZE		0x18
#endif

.extern	boot_params_stash

ENTRY(save_boot_params)
	la	t0, boot_params_stash
	SREG	a0, REGBYTES * 0(t0)
	SREG	a1, REGBYTES * 1(t0)
	SREG	a2, REGBYTES * 2(t0)
	SREG	a3, REGBYTES * 3(t0)
	SREG	a4, REGBYTES * 4(t0)
	SREG	a5, REGBYTES * 5(t0)
	SREG	a6, REGBYTES * 6(t0)
	SREG	a7, REGBYTES * 7(t0)
	SREG	s0, REGBYTES * 8(t0)
	SREG	s1, REGBYTES * 9(t0)
	SREG	s2, REGBYTES * 10(t0)
	SREG	s3, REGBYTES * 11(t0)
	SREG	s4, REGBYTES * 12(t0)
	SREG	s5, REGBYTES * 13(t0)
	SREG	s6, REGBYTES * 14(t0)
	SREG	s7, REGBYTES * 15(t0)
	SREG	s8, REGBYTES * 16(t0)
	SREG	s9, REGBYTES * 17(t0)
	SREG	s10, REGBYTES * 18(t0)
	SREG	s11, REGBYTES * 19(t0)
	SREG	sp, REGBYTES * 20(t0)
	SREG	ra, REGBYTES * 21(t0)
	j	save_boot_params_ret
ENDPROC(save_boot_params)

#ifdef CONFIG_SPL_BUILD

/*
 * Two cases return to boot rom:
 * - USB upgrade case, boot rom run SPL to initialize DRAM, then return to boot
 *   rom.
 * - When boot loader want to enter USB upgrade mode
 */
ENTRY(board_return_to_bootrom)
	la	t0, boot_params_stash
	LREG	a0, REGBYTES * 0(t0)
	LREG	a1, REGBYTES * 1(t0)
	LREG	a2, REGBYTES * 2(t0)
	LREG	a3, REGBYTES * 3(t0)
	LREG	a4, REGBYTES * 4(t0)
	LREG	a5, REGBYTES * 5(t0)
	LREG	a6, REGBYTES * 6(t0)
	LREG	a7, REGBYTES * 7(t0)
	LREG	s0, REGBYTES * 8(t0)
	LREG	s1, REGBYTES * 9(t0)
	LREG	s2, REGBYTES * 10(t0)
	LREG	s3, REGBYTES * 11(t0)
	LREG	s4, REGBYTES * 12(t0)
	LREG	s5, REGBYTES * 13(t0)
	LREG	s6, REGBYTES * 14(t0)
	LREG	s7, REGBYTES * 15(t0)
	LREG	s8, REGBYTES * 16(t0)
	LREG	s9, REGBYTES * 17(t0)
	LREG	s10, REGBYTES * 18(t0)
	LREG	s11, REGBYTES * 19(t0)
	LREG	sp, REGBYTES * 20(t0)
	LREG	ra, REGBYTES * 21(t0)
	ret
ENDPROC(board_return_to_bootrom)

#else

/*
 * U-Boot is not support return to bootrom.
 */
ENTRY(board_return_to_bootrom)
ENDPROC(board_return_to_bootrom)

#endif

ENTRY(lowlevel_init)
	/* nothing done */
ENDPROC(lowlevel_init)
