#include <linux/linkage.h>
#include <asm-generic/export.h>
#include <asm/asm.h>
#include <asm/csr.h>

	.macro fixup op reg addr lbl
100:
	\op \reg, \addr
	.section __ex_table,"a"
	.balign RISCV_SZPTR
	RISCV_PTR 100b, \lbl
	.previous
	.endm

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


ENTRY(__asm_copy_to_user)
ENTRY(__asm_copy_from_user)

	/* Enable access to user memory */
	li t6, SR_SUM
	csrs CSR_STATUS, t6

	/* if LEN < SZREG jump to tail handling.  */
	li a5, SZREG-1
	mv a6, a0
	mv a0, a2	/* a0 as the wrong copy size if Page Fault happened */
	bleu a2, a5, L(tail)

	/* jump to execive aligned memcpy if aligned  */
	andi a3, a6, SZREG-1
	bnez a3, L(glibc_noalign_start)
	andi a3, a2, SZREG-1
	beqz a3, L(memcpy_aligned_start)

L(glibc_noalign_start):
	/* Copy the first word, align DEST to word, and adjust DEST/SRC/LEN
	   based on the amount adjusted to align DEST.  */
	fixup REG_L, a3, 0(a1), L(fixup)
	andi a5, a6, SZREG-1
	addi a2, a2, -SZREG
	li a4, SZREG
	sub a4, a4, a5
	fixup REG_S, a3, 0(a6), L(fixup)
	add a2, a5, a2

	/* If LEN < BLOCK_SIZE jump to word copy.  */
	li a3, BLOCK_SIZE-1
	add a5, a6, a4
	add a1, a1, a4
	bleu a2, a3, L(word_copy_adjust)
	addi a7, a2, -BLOCK_SIZE
	andi a7, a7, -BLOCK_SIZE
	addi a7, a7, BLOCK_SIZE
	add a3, a5, a7
	mv a4, a1
L(block_copy):
	fixup REG_L, a6,          0(a4), L(fixup)
	fixup REG_L, t0,      SZREG(a4), L(fixup)
	fixup REG_L, t1,  (2*SZREG)(a4), L(fixup)
	fixup REG_L, t2,  (3*SZREG)(a4), L(fixup)
	fixup REG_L, t3,  (4*SZREG)(a4), L(fixup)
	fixup REG_L, t4,  (5*SZREG)(a4), L(fixup)
	fixup REG_L, t5,  (6*SZREG)(a4), L(fixup)
	fixup REG_L, t6,  (7*SZREG)(a4), L(fixup)
	fixup REG_S, a6,          0(a5), L(fixup)
	fixup REG_S, t0,      SZREG(a5), L(fixup)
	fixup REG_S, t1,  (2*SZREG)(a5), L(fixup)
	fixup REG_S, t2,  (3*SZREG)(a5), L(fixup)
	fixup REG_S, t3,  (4*SZREG)(a5), L(fixup)
	fixup REG_S, t4,  (5*SZREG)(a5), L(fixup)
	fixup REG_S, t5,  (6*SZREG)(a5), L(fixup)
	fixup REG_S, t6,  (7*SZREG)(a5), L(fixup)
	fixup REG_L, a6,  (8*SZREG)(a4), L(fixup)
	fixup REG_L, t0,  (9*SZREG)(a4), L(fixup)
	fixup REG_L, t1, (10*SZREG)(a4), L(fixup)
	fixup REG_L, t2, (11*SZREG)(a4), L(fixup)
	fixup REG_L, t3, (12*SZREG)(a4), L(fixup)
	fixup REG_L, t4, (13*SZREG)(a4), L(fixup)
	fixup REG_L, t5, (14*SZREG)(a4), L(fixup)
	fixup REG_L, t6, (15*SZREG)(a4), L(fixup)
	addi a4, a4, BLOCK_SIZE
	fixup REG_S, a6,  (8*SZREG)(a5), L(fixup)
	fixup REG_S, t0,  (9*SZREG)(a5), L(fixup)
	fixup REG_S, t1, (10*SZREG)(a5), L(fixup)
	fixup REG_S, t2, (11*SZREG)(a5), L(fixup)
	fixup REG_S, t3, (12*SZREG)(a5), L(fixup)
	fixup REG_S, t4, (13*SZREG)(a5), L(fixup)
	fixup REG_S, t5, (14*SZREG)(a5), L(fixup)
	fixup REG_S, t6, (15*SZREG)(a5), L(fixup)
	addi a5, a5, BLOCK_SIZE
	bne a5, a3, L(block_copy)
	add a1, a1, a7
	andi a2, a2, BLOCK_SIZE-1

	/* 0 <= a2/LEN  < BLOCK_SIZE.  */
L(word_copy):
	li a5, SZREG-1
	/* if LEN < SZREG jump to tail handling.  */
	bleu a2, a5, L(tail_adjust)
	addi a7, a2, -SZREG
	andi a7, a7, -SZREG
	addi a7, a7, SZREG
	add a6, a3, a7
	mv a5, a1
L(word_copy_loop):
	fixup REG_L, a4, 0(a5), L(fixup)
	addi a3, a3, SZREG
	addi a5, a5, SZREG
	fixup REG_S, a4, -SZREG(a3), L(fixup)
	bne a3, a6, L(word_copy_loop)
	add a1, a1, a7
	andi a2, a2, SZREG-1

	/* Copy the last word unaligned.  */
	add a3, a1, a2
	add a4, a6, a2
	fixup REG_L, t0, -SZREG(a3), L(fixup)
	fixup REG_S, t0, -SZREG(a4), L(fixup)
	j L(ret_clear)

L(tail):
	/* Copy 4-7 bytes.  */
	andi a5, a2, 4
	add a3, a1, a2
	add a4, a6, a2
	beq a5, zero, L(copy_0_3)
	fixup lw, t0, 0(a1), L(fixup)
	fixup lw, t1, -4(a3), L(fixup)
	fixup sw, t0, 0(a6), L(fixup)
	fixup sw, t1, -4(a4), L(fixup)
	j L(ret_clear)

	/* Copy 0-3 bytes.  */
L(copy_0_3):
	beq a2, zero, L(ret)
	srli a2, a2, 1
	add t4, a1, a2
	add t5, a6, a2
	fixup lbu, t0, 0(a1), L(fixup)
	fixup lbu, t1, -1(a3), L(fixup)
	fixup lbu, t2, 0(t4), L(fixup)
	fixup sb,  t0, 0(a6), L(fixup)
	fixup sb,  t1, -1(a4), L(fixup)
	fixup sb,  t2, 0(t5), L(fixup)
L(ret):
	j L(ret_clear)
L(tail_adjust):
	mv a6, a3
	j L(tail)
L(word_copy_adjust):
	mv a3, a5
	j L(word_copy)
	j L(ret_clear)

L(ret_clear):
	/* Disable access to user memory */
	li t6, SR_SUM
	csrc CSR_STATUS, t6
	li a0, 0
	ret


L(memcpy_aligned_start):
	move t6, a6

	andi a4, a2, ~(BLOCK_SIZE-1)
	beqz a4, L(regsize_copy_aligned)
	add a3, a1, a4

L(block_copy_aligned):
	fixup REG_L, a4,       0(a1), L(fixup)
	fixup REG_L, a5,   SZREG(a1), L(fixup)
	fixup REG_L, a6, 2*SZREG(a1), L(fixup)
	fixup REG_L, a7, 3*SZREG(a1), L(fixup)
	fixup REG_L, t0, 4*SZREG(a1), L(fixup)
	fixup REG_L, t1, 5*SZREG(a1), L(fixup)
	fixup REG_L, t2, 6*SZREG(a1), L(fixup)
	fixup REG_L, t3, 7*SZREG(a1), L(fixup)
	fixup REG_L, t4, 8*SZREG(a1), L(fixup)
	fixup REG_L, t5, 9*SZREG(a1), L(fixup)
	fixup REG_S, a4,       0(t6), L(fixup)
	fixup REG_S, a5,   SZREG(t6), L(fixup)
	fixup REG_S, a6, 2*SZREG(t6), L(fixup)
	fixup REG_S, a7, 3*SZREG(t6), L(fixup)
	fixup REG_S, t0, 4*SZREG(t6), L(fixup)
	fixup REG_S, t1, 5*SZREG(t6), L(fixup)
	fixup REG_S, t2, 6*SZREG(t6), L(fixup)
	fixup REG_S, t3, 7*SZREG(t6), L(fixup)
	fixup REG_S, t4, 8*SZREG(t6), L(fixup)
	fixup REG_S, t5, 9*SZREG(t6), L(fixup)
	fixup REG_L, a4, 10*SZREG(a1), L(fixup)
	fixup REG_L, a5, 11*SZREG(a1), L(fixup)
	fixup REG_L, a6, 12*SZREG(a1), L(fixup)
	fixup REG_L, a7, 13*SZREG(a1), L(fixup)
	fixup REG_L, t0, 14*SZREG(a1), L(fixup)
	fixup REG_L, t1, 15*SZREG(a1), L(fixup)
	addi a1, a1, BLOCK_SIZE
	fixup REG_S, a4, 10*SZREG(t6), L(fixup)
	fixup REG_S, a5, 11*SZREG(t6), L(fixup)
	fixup REG_S, a6, 12*SZREG(t6), L(fixup)
	fixup REG_S, a7, 13*SZREG(t6), L(fixup)
	fixup REG_S, t0, 14*SZREG(t6), L(fixup)
	fixup REG_S, t1, 15*SZREG(t6), L(fixup)
	addi t6, t6, BLOCK_SIZE
	bltu a1, a3, L(block_copy_aligned)

	andi a2, a2, (BLOCK_SIZE)-1
	beqz a2, L(ret_aligned_clear)

L(regsize_copy_aligned):
	add a3, t6, a2
	bgeu t6, a3, L(ret_aligned_clear)

L(regsize_copy_aligned_loop):
	fixup REG_L, a4, 0(a1), L(fixup)
	addi a1, a1, SZREG
	fixup REG_S, a4, 0(t6), L(fixup)
	addi t6, t6, SZREG
	bltu t6, a3, L(regsize_copy_aligned_loop)

L(ret_aligned_clear):
	/* Disable access to user memory */
	li t6, SR_SUM
	csrc CSR_STATUS, t6
	li a0, 0
	ret

ENDPROC(__asm_copy_to_user)
ENDPROC(__asm_copy_from_user)
EXPORT_SYMBOL(__asm_copy_to_user)
EXPORT_SYMBOL(__asm_copy_from_user)


ENTRY(__clear_user)

	/* Enable access to user memory */
	li t6, SR_SUM
	csrs CSR_STATUS, t6

	add a3, a0, a1
	addi t0, a0, SZREG-1
	andi t1, a3, ~(SZREG-1)
	andi t0, t0, ~(SZREG-1)
	/*
	 * a3: terminal address of target region
	 * t0: lowest doubleword-aligned address in target region
	 * t1: highest doubleword-aligned address in target region
	 */
	bgeu t0, t1, 2f
	bltu a0, t0, 4f
1:
	fixup REG_S, zero, (a0), 11f
	addi a0, a0, SZREG
	bltu a0, t1, 1b
2:
	bltu a0, a3, 5f

3:
	/* Disable access to user memory */
	csrc CSR_STATUS, t6
	li a0, 0
	ret
4: /* Edge case: unalignment */
	fixup sb, zero, (a0), 11f
	addi a0, a0, 1
	bltu a0, t0, 4b
	j 1b
5: /* Edge case: remainder */
	fixup sb, zero, (a0), 11f
	addi a0, a0, 1
	bltu a0, a3, 5b
	j 3b
ENDPROC(__clear_user)
EXPORT_SYMBOL(__clear_user)

	.section .fixup,"ax"
	.balign 4
	/* Fixup code for __copy_user(10) and __clear_user(11) */
10:
	/* Disable access to user memory */
	csrs CSR_STATUS, t6
	mv a0, a2
	ret
11:
	csrs CSR_STATUS, t6
	mv a0, a1
	ret

L(fixup):
	/* Disable access to user memory */
	li t6, SR_SUM
	csrs CSR_STATUS, t6
	/* dont have to organize return values cause a0 has been the copy size */
	ret
	.previous

