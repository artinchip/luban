# SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
#
# Copyright (c) 2020, ArtInChip Technology Co., Ltd
#

ifndef CONFIG_SPL_BUILD
INPUTS-y += u-boot.its kernel.its
endif

AIC_UBOOT_ITS :=
ifeq ($(wildcard $(AIC_UBOOT_ITS)),)
	AIC_UBOOT_ITS := $(srctree)/arch/$(ARCH)/dts/u-boot.its.dtsi
endif
ifeq ($(wildcard $(AIC_UBOOT_ITS)),)
$(AIC_UBOOT_ITS):
	@echo "could not find u-boot.its.dtsi"
	@exit 1
endif

# Pre-Process and generate AIC u-boot.its
aicdtc_cpp_flags  = -Wp,-MD,$(depfile).pre.tmp -nostdinc                 \
		 -I$(srctree)/arch/$(ARCH)/dts                           \
		 -I$(srctree)/arch/$(ARCH)/dts/include                   \
		 -Iinclude                                               \
		 -I$(srctree)/include                                    \
		 -I$(srctree)/arch/$(ARCH)/include                       \
		 -include $(srctree)/include/linux/kconfig.h             \
		 -D__ASSEMBLY__                                          \
		 -undef -D__DTS__

quiet_cmd_cpp_its = ITS     $@
cmd_cpp_its = $(HOSTCC) -E $(aicdtc_cpp_flags) -x assembler-with-cpp -o $(depfile).tmp $< ; \
	      sed '/\# /d' $(depfile).tmp > $@

$(obj)/u-boot.its: $(AIC_UBOOT_ITS) FORCE
	$(call if_changed,cpp_its)

AIC_KERNEL_ITS :=
ifeq ($(wildcard $(AIC_KERNEL_ITS)),)
	AIC_KERNEL_ITS := $(srctree)/arch/$(ARCH)/dts/kernel.its.dtsi
endif
ifeq ($(wildcard $(AIC_KERNEL_ITS)),)
$(AIC_KERNEL_ITS):
	@echo "could not find kernel.its.dtsi"
	@exit 1
endif

# Pre-Process and generate AIC kernel.its
aicdtc_cpp_flags  = -Wp,-MD,$(depfile).pre.tmp -nostdinc                 \
		 -I$(srctree)/arch/$(ARCH)/dts                           \
		 -I$(srctree)/arch/$(ARCH)/dts/include                   \
		 -Iinclude                                               \
		 -I$(srctree)/include                                    \
		 -I$(srctree)/arch/$(ARCH)/include                       \
		 -include $(srctree)/include/linux/kconfig.h             \
		 -D__ASSEMBLY__                                          \
		 -undef -D__DTS__

quiet_cmd_cpp_its = ITS     $@
cmd_cpp_its = $(HOSTCC) -E $(aicdtc_cpp_flags) -x assembler-with-cpp -o $(depfile).tmp $< ; \
	      sed '/\# /d' $(depfile).tmp > $@

$(obj)/kernel.its: $(AIC_KERNEL_ITS) FORCE
	$(call if_changed,cpp_its)

ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
make_part = python3 $(srctree)/tools/get_part_table_config.py

AIC_ENV_IMAGE := $(srctree)/include/configs/image_cfg.json
AIC_ENV_PART_ADDR := include/generated/image_cfg_part_config.h
AIC_ENV_PART := $(obj)/$(AIC_ENV_PART_ADDR)

INPUTS-y += $(AIC_ENV_PART_ADDR)

cmd_touch_part_config = $(make_part) -c $< -d $@

$(AIC_ENV_PART): $(AIC_ENV_IMAGE) FORCE
	$(call if_changed,touch_part_config)
endif