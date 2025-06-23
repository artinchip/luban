/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024, ArtInChip Technology Co., Ltd
 *
 * Authors:
 *   dwj <weijie.ding@artinchip.com>
 */
#include <sbi/riscv_asm.h>

#define DDRC_BASE               0x18400000
#define DDR_PHY_BASE            0x18500000
#define GTC_CNTV                0x19050008
#define CMU_DDR_REG             0x18020210
#define CMU_PLL_INT1_REG        0x18020004
#define CMU_PLL_FRA0_REG        0x18020020
#define CMU_PLL_IN_REG          0x180200A4
#define SYSCFG_LDO25_REG        0x18000024

.macro delay_200us
        li      a0, GTC_CNTV
        ld      a1, (a0)        //start value 64bit
1:
        ld      a2, (a0)        //current value 64bit
        sub     a3, a2, a1      //t3 <--- t2 - t1
        li      a4, 800         //GTC frequency is 4000000Hz, 200us counter 800
        bltu    a3, a4, 1b
2:
.endm

        .section .entry, "ax", %progbits
        .align 3
        .option pic
        .global aic_suspend_resume
aic_suspend_resume:
        //ddr self-refresh flow
        li      t0, DDRC_BASE
        li      t1, DDR_PHY_BASE

        //check port status
check_port_status:
        lw      t2, 0x3fc(t0)
        bne     t2, zero, check_port_status

        //disable DDR port
        li      t2, 0
        li      t3, 0
        li      t5, 5
        addi    t0, t0, 0x490
port_close_loop:
        add     t0, t0, t2
        sw      zero, (t0)
        addi    t2, t2, 0xb0
        addi    t3, t3, 1
        bltu    t3, t5, port_close_loop

        delay_200us

        //enter self refresh
        li      t0, DDRC_BASE
        lw      t2, 0x30(t0)
        ori     t2, t2, 0x21
        sw      t2, 0x30(t0)
check_ddr_enter_sr_status:
        lw      t2, 0x4(t0)
        andi    t2, t2, 0x3
        li      t3, 0x3
        bne     t2, t3, check_ddr_enter_sr_status

        delay_200us

        //en_dfi_dram_clk_disable
        lw      t2, 0x30(t0)
        ori     t2, t2, 0x8
        sw      t2, 0x30(t0)

        //disable PGCR0 reg CKEN bitfiled
        lw      t2, 0x8(t1)
        li      t3, 0x3ffffff
        and     t2, t2, t3
        sw      t2, 0x8(t1)

        delay_200us

        //disable DX0GCR reg DQ, DM, DQS
        lw      t2, 0x1C0(t1)
        ori     t2, t2, 0x30
        li      t3, 0xffff3fff
        and     t2, t2, t3
        li      t3, 0x8000
        or      t2, t2, t3
        sw      t2, 0x1C0(t1)

        //disable DX1GCR reg DQ, DM, DQS
        lw      t2, 0x200(t1)
        ori     t2, t2, 0x30
        li      t3, 0xffff3fff
        and     t2, t2, t3
        li      t3, 0x8000
        or      t2, t2, t3
        sw      t2, 0x200(t1)

        //disable DDR phy/bus clock
        li      t1, CMU_DDR_REG
        lw      t2, (t1)
        li      t3, 0xfffaabff
        and     t2, t2, t3
        sw      t2, (t1)

        delay_200us

        //disable DDR phy update request
        li      t1, DDR_PHY_BASE
        lw      t2, 0x40(t1)
        li      t3, 0xfffffffe
        and     t2, t2, t3
        sw      t2, 0x40(t1)

        //disable DDR core controller clock
        li      t1, CMU_DDR_REG
        lw      t2, (t1)
        li      t3, 0xfffffeff
        and     t2, t2, t3
        sw      t2, (t1)

        delay_200us

        //disable PLL_FRA0
        li      t1, CMU_PLL_FRA0_REG
        lw      t2, (t1)
        li      t3, 0xfffaffff
        and     t2, t2, t3
        sw      t2, (t1)

        //disable PLL_INT1
        li      t1, CMU_PLL_INT1_REG
        lw      t2, (t1)
        li      t3, 0xfffaffff
        and     t2, t2, t3
        sw      t2, (t1)

        //disable XTAL_GM and XTAL_START
        li      t1, CMU_PLL_IN_REG
        lw      t2, (t1)
        li      t3, 0x1fffffff
        and     t2, t2, t3
        sw      t2, (t1)

        //disable LDO25
        li      t1, SYSCFG_LDO25_REG
        lw      t2, (t1)
        li      t3, 0xfffffff7
        and     t2, t2, t3
        sw      t2, (t1)

        wfi

        //wakeup flow
        //enable LDO25
        li      t1, SYSCFG_LDO25_REG
        lw      t2, (t1)
        li      t3, 0x8
        or      t2, t2, t3
        sw      t2, (t1)

        //enable XTAL_GM and XTAL_START
        li      t1, CMU_PLL_IN_REG
        lw      t2, (t1)
        li      t3, 0xe0000000
        or      t2, t2, t3
        sw      t2, (t1)

        //enable PLL_INT1
        li      t1, CMU_PLL_INT1_REG
        lw      t2, (t1)
        li      t3, 0x50000
        or      t2, t2, t3
        sw      t2, (t1)

        //enable PLL_FRA0
        li      t1, CMU_PLL_FRA0_REG
        lw      t2, (t1)
        li      t3, 0x50000
        or      t2, t2, t3
        sw      t2, (t1)

        delay_200us

        //enable DDR phy/bus clock
        li      t1, CMU_DDR_REG
        lw      t2, (t1)
        li      t3, 0x55400
        or      t2, t2, t3
        sw      t2, (t1)

        delay_200us

        //enable DDR phy update request
        li      t1, DDR_PHY_BASE
        lw      t2, 0x40(t1)
        ori     t2, t2, 0x1
        sw      t2, 0x40(t1)

        //enable DDR core controller clock
        li      t1, CMU_DDR_REG
        lw      t2, (t1)
        ori     t2, t2, 0x100
        sw      t2, (t1)

        //must reset PLL if the DDR core clock disabled in self-refresh state
        li      t0, DDRC_BASE
        li      t1, DDR_PHY_BASE

        lw      t2, 0x18(t1)
        li      t3, 0x40000000
        or      t2, t2, t3
        sw      t2, 0x18(t1)

        delay_200us

        li      t3, 0xbfffffff
        and     t2, t2, t3
        sw      t2, 0x18(t1)

        delay_200us

        //enable dfi_dram_clk
        lw      t2, 0x30(t0)
        li      t3, 0xFFFFFFF7
        and     t2, t2, t3
        sw      t2, 0x30(t0)

        //enable PGCR0 reg CKEN bitfiled
        lw      t2, 0x8(t1)
        li      t3, 0x3ffffff
        and     t2, t2, t3
        li      t3, 0x8000000
        or      t2, t2, t3
        sw      t2, 0x8(t1)

        delay_200us

        //enable DX0GCR reg DQ, DM, DQS
        lw      t2, 0x1C0(t1)
        li      t3, 0xFFFF3FCF
        and     t2, t2, t3
        sw      t2, 0x1C0(t1)

        //enable DX1GCR reg DQ, DM, DQS
        lw      t2, 0x200(t1)
        li      t3, 0xFFFF3FCF
        and     t2, t2, t3
        sw      t2, 0x200(t1)

        delay_200us

        //DDR exit self-refresh
        lw      t2, 0x30(t0)
        li      t3, 0xDE
        and     t2, t2, t3
        sw      t2, 0x30(t0) //exit self-refresh
check_ddr_exit_sr_status:
        lw      t2, 0x4(t0)
        andi    t2, t2, 0x3
        li      t3, 0x1
        bne     t2, t3, check_ddr_exit_sr_status

        delay_200us

        li      t2, 0
        li      t3, 0
        li      t4, 1
        li      t5, 5
        addi    t0, t0, 0x490
port_open_loop:
        add     t0, t0, t2
        sw      t4, (t0)
        addi    t2, t2, 0xb0
        addi    t3, t3, 1
        bltu    t3, t5, port_open_loop

        ret
aic_suspend_resume_end:

        .data
        .align 3
        .global aic_suspend_resume_size
aic_suspend_resume_size:
        RISCV_PTR       aic_suspend_resume_end - aic_suspend_resume
