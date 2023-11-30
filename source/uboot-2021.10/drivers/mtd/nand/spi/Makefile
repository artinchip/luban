# SPDX-License-Identifier: GPL-2.0

ifdef CONFIG_SPL_BUILD
ifdef CONFIG_SPL_SPI_NAND_TINY
obj-$(CONFIG_MTD_SPI_NAND) += core_tiny.o
else
obj-$(CONFIG_MTD_SPI_NAND) += core.o
endif
else
obj-$(CONFIG_MTD_SPI_NAND) += core.o
endif
obj-$(CONFIG_MTD_SPI_NAND) += manufacturer.o
obj-$(CONFIG_SPI_NAND_MICRON) += micron.o
obj-$(CONFIG_SPI_NAND_GIGADEVICE) += gigadevice.o
obj-$(CONFIG_SPI_NAND_MACRONIX) += macronix.o
obj-$(CONFIG_SPI_NAND_WINBOND) += winbond.o
obj-$(CONFIG_SPI_NAND_TOSHIBA) += toshiba.o
obj-$(CONFIG_SPI_NAND_FMSH) += fmsh.o
obj-$(CONFIG_SPI_NAND_FORESEE) += foresee.o
obj-$(CONFIG_SPI_NAND_ZBIT) += zbit.o
obj-$(CONFIG_NAND_BBT_MANAGE) += aic_bbt.o
obj-$(CONFIG_SPI_NAND_ELITE) += elite.o
obj-$(CONFIG_SPI_NAND_ESMT) += esmt.o
obj-$(CONFIG_SPI_NAND_UMTEK) += umtek.o
obj-$(CONFIG_SPI_NAND_BYTE) += byte.o

ccflags-$(CONFIG_ARTINCHIP_DEBUG_SPINAND_DRIVER) += -DDEBUG -DLOG_DEBUG
ccflags-$(CONFIG_ARTINCHIP_DEBUG_SPINAND_DRIVER) += -O0
