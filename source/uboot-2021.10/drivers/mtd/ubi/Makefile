# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

obj-y += attach.o build.o vtbl.o vmt.o upd.o kapi.o eba.o io.o wl.o crc32.o
obj-$(CONFIG_MTD_UBI_FASTMAP) += fastmap.o
obj-y += misc.o
obj-y += debug.o

ccflags-$(CONFIG_ARTINCHIP_DEBUG_UBI) += -DDEBUG -DLOG_DEBUG
ccflags-$(CONFIG_ARTINCHIP_DEBUG_UBI) += -O0
