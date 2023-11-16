# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Artinchip SoC clk
#

obj-y	+= clk-pll.o
obj-y	+= clk-fixed-parent-mod.o
obj-y	+= clk-multi-parent-mod.o
obj-y	+= clk-disp.o
obj-$(CONFIG_CLK_ARTINCHIP) += aic-cmu.o
ifdef CONFIG_CLK_ARTINCHIP
obj-$(CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP) += clk-aic-fpga.o
endif
