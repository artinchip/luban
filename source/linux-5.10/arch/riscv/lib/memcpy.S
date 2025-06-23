/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2013 Regents of the University of California
 */

#include <linux/linkage.h>
#include <asm/asm.h>

#ifdef __ASSEMBLY__
#define __ASM_STR(x)	x
#else
#define __ASM_STR(x)	#x
#endif

#if __riscv_xlen == 64
#define __REG_SEL(a, b)	__ASM_STR(a)
#elif __riscv_xlen == 32
#define __REG_SEL(a, b)	__ASM_STR(b)
#else
#error "Unexpected __riscv_xlen"
#endif

#define REG_L		__REG_SEL(ld, lw)
#define REG_S		__REG_SEL(sd, sw)
#define REG_SC		__REG_SEL(sc.d, sc.w)
#define REG_AMOSWAP_AQ	__REG_SEL(amoswap.d.aq, amoswap.w.aq)
#define REG_ASM		__REG_SEL(.dword, .word)
#define SZREG		__REG_SEL(8, 4)
#define LGREG		__REG_SEL(3, 2)

# define L(label) .L ## label

#define BLOCK_SIZE (16 * SZREG)

/* void *memcpy(void *, const void *, size_t) */
ENTRY(__memcpy)
WEAK(memcpy)
	beq	a2, zero, L(ret)

	/* if LEN < SZREG jump to tail handling.  */
	li	a5, SZREG-1
	mv	a6, a0
	bleu	a2, a5, L(tail)

	/* jump to execive aligned memcpy if aligned  */
	andi a3, a0, SZREG-1
	bnez a3, L(glibc_noalign_start)

	andi a3, a2, SZREG-1
	beqz a3, L(memcpy_aligned_start)

L(glibc_noalign_start):
	/* Copy the first word, align DEST to word, and adjust DEST/SRC/LEN
	   based on the amount adjusted to align DEST.  */
	REG_L	a3, 0(a1)
	andi	a5, a0, SZREG-1
	addi	a2, a2, -SZREG
	li	a4, SZREG
	sub	a4, a4, a5
	REG_S	a3, 0(a0)
	add	a2, a5, a2

	/* If LEN < BLOCK_SIZE jump to word copy.  */
	li	a3, BLOCK_SIZE-1
	add	a5, a0, a4
	add	a1, a1, a4
	bleu	a2, a3, L(word_copy_adjust)
	addi	a7, a2, -BLOCK_SIZE
	andi	a7, a7, -BLOCK_SIZE
	addi	a7, a7, BLOCK_SIZE
	add	a3, a5, a7
	mv	a4, a1
L(block_copy):
	REG_L	a6,          0(a4)
	REG_L	t0,      SZREG(a4)
	REG_L	t1,  (2*SZREG)(a4)
	REG_L	t2,  (3*SZREG)(a4)
	REG_L	t3,  (4*SZREG)(a4)
	REG_L	t4,  (5*SZREG)(a4)
	REG_L	t5,  (6*SZREG)(a4)
	REG_L	t6,  (7*SZREG)(a4)
	REG_S	a6,          0(a5)
	REG_S	t0,      SZREG(a5)
	REG_S	t1,  (2*SZREG)(a5)
	REG_S	t2,  (3*SZREG)(a5)
	REG_S	t3,  (4*SZREG)(a5)
	REG_S	t4,  (5*SZREG)(a5)
	REG_S	t5,  (6*SZREG)(a5)
	REG_S	t6,  (7*SZREG)(a5)
	REG_L	a6,  (8*SZREG)(a4)
	REG_L	t0,  (9*SZREG)(a4)
	REG_L	t1, (10*SZREG)(a4)
	REG_L	t2, (11*SZREG)(a4)
	REG_L	t3, (12*SZREG)(a4)
	REG_L	t4, (13*SZREG)(a4)
	REG_L	t5, (14*SZREG)(a4)
	REG_L	t6, (15*SZREG)(a4)
	addi	a4, a4, BLOCK_SIZE
	REG_S	a6,  (8*SZREG)(a5)
	REG_S	t0,  (9*SZREG)(a5)
	REG_S	t1, (10*SZREG)(a5)
	REG_S	t2, (11*SZREG)(a5)
	REG_S	t3, (12*SZREG)(a5)
	REG_S	t4, (13*SZREG)(a5)
	REG_S	t5, (14*SZREG)(a5)
	REG_S	t6, (15*SZREG)(a5)
	addi	a5, a5, BLOCK_SIZE
	bne	a5, a3, L(block_copy)
	add	a1, a1, a7
	andi	a2, a2, BLOCK_SIZE-1
	/* 0 <= a2/LEN  < BLOCK_SIZE.  */
L(word_copy):
	li	a5, SZREG-1
	/* if LEN < SZREG jump to tail handling.  */
	bleu	a2, a5, L(tail_adjust)
	addi	a7, a2, -SZREG
	andi	a7, a7, -SZREG
	addi	a7, a7, SZREG
	add	a6, a3, a7
	mv	a5, a1
L(word_copy_loop):
	REG_L	a4, 0(a5)
	addi	a3, a3, SZREG
	addi	a5, a5, SZREG
	REG_S	a4, -SZREG(a3)
	bne	a3, a6, L(word_copy_loop)
	add	a1, a1, a7
	andi	a2, a2, SZREG-1

	/* Copy the last word unaligned.  */
	add	a3, a1, a2
	add	a4, a6, a2
	REG_L	t0, -SZREG(a3)
	REG_S	t0, -SZREG(a4)
	ret

L(tail):
	/* Copy 4-7 bytes.  */
	andi	a5, a2, 4
	add	a3, a1, a2
	add	a4, a6, a2
	beq	a5, zero, L(copy_0_3)
	lw	t0, 0(a1)
	lw	t1, -4(a3)
	sw	t0, 0(a6)
	sw	t1, -4(a4)
	ret

	/* Copy 0-3 bytes.  */
L(copy_0_3):
	beq	a2, zero, L(ret)
	srli    a2, a2, 1
	add     t4, a1, a2
	add     t5, a6, a2
	lbu     t0, 0(a1)
	lbu     t1, -1(a3)
	lbu     t2, 0(t4)
	sb      t0, 0(a6)
	sb      t1, -1(a4)
	sb      t2, 0(t5)
L(ret):
	ret
L(tail_adjust):
	mv	a6, a3
	j	L(tail)
L(word_copy_adjust):
	mv	a3, a5
	j	L(word_copy)
	ret

L(memcpy_aligned_start):
	move t6, a0

	andi a4, a2, ~(BLOCK_SIZE-1)
	beqz a4, L(regsize_copy_aligned)
	add a3, a1, a4

L(block_copy_aligned):
	REG_L a4,       0(a1)
	REG_L a5,   SZREG(a1)
	REG_L a6, 2*SZREG(a1)
	REG_L a7, 3*SZREG(a1)
	REG_L t0, 4*SZREG(a1)
	REG_L t1, 5*SZREG(a1)
	REG_L t2, 6*SZREG(a1)
	REG_L t3, 7*SZREG(a1)
	REG_L t4, 8*SZREG(a1)
	REG_L t5, 9*SZREG(a1)
	REG_S a4,       0(t6)
	REG_S a5,   SZREG(t6)
	REG_S a6, 2*SZREG(t6)
	REG_S a7, 3*SZREG(t6)
	REG_S t0, 4*SZREG(t6)
	REG_S t1, 5*SZREG(t6)
	REG_S t2, 6*SZREG(t6)
	REG_S t3, 7*SZREG(t6)
	REG_S t4, 8*SZREG(t6)
	REG_S t5, 9*SZREG(t6)
	REG_L a4, 10*SZREG(a1)
	REG_L a5, 11*SZREG(a1)
	REG_L a6, 12*SZREG(a1)
	REG_L a7, 13*SZREG(a1)
	REG_L t0, 14*SZREG(a1)
	REG_L t1, 15*SZREG(a1)
	addi a1, a1, BLOCK_SIZE
	REG_S a4, 10*SZREG(t6)
	REG_S a5, 11*SZREG(t6)
	REG_S a6, 12*SZREG(t6)
	REG_S a7, 13*SZREG(t6)
	REG_S t0, 14*SZREG(t6)
	REG_S t1, 15*SZREG(t6)
	addi t6, t6, BLOCK_SIZE
	bltu a1, a3, L(block_copy_aligned)

	andi a2, a2, (BLOCK_SIZE)-1
	beqz a2, L(ret_aligned)

L(regsize_copy_aligned):
	add a3, t6, a2
	bgeu t6, a3, L(ret_aligned)

L(regsize_copy_aligned_loop):
	REG_L a4, 0(a1)
	addi a1, a1, SZREG
	REG_S a4, 0(t6)
	addi t6, t6, SZREG
	bltu t6, a3, L(regsize_copy_aligned_loop)

L(ret_aligned):
	ret

END(__memcpy)
