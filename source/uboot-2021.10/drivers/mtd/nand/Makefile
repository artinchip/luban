# SPDX-License-Identifier: GPL-2.0+

ifdef CONFIG_SPL_BUILD
ifdef CONFIG_SPL_SPI_NAND_TINY
nandcore-objs := core_tiny.o bbt.o
else
nandcore-objs := core.o bbt.o
endif
else
nandcore-objs := core.o bbt.o
endif
obj-$(CONFIG_MTD_NAND_CORE) += nandcore.o
obj-$(CONFIG_$(SPL_TPL_)MTD_SPI_NAND) += spi/
obj-$(CONFIG_$(SPL_TPL_)NAND_SUPPORT) += raw/
