# SPDX-License-Identifier:	GPL-2.0

obj-y += cpu.o
obj-y += lowlevel_init.o

ifdef CONFIG_TARGET_D211
obj-y += d211/boot_param.o
obj-$(CONFIG_SPL_BUILD)	+= spl.o
obj-$(CONFIG_CMD_AICUPG) += upg_detect.o
endif

