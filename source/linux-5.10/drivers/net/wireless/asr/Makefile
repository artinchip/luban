#select your borad option ,such as lega,duet,canon,alto,bass
#you need change some args in xxx_defconfig,this Makefile as comon for all boards
### this only for ubuntu pc compile 5531 usb ko
ifeq ($(TARGET),canon_usb)
CONFIG_ASR5531 := m
CONFIG_PLATFORM_I386_PC := y
CONFIG_PLATFORM_RASPI_AARCH64 := n
CONFIG_NOT_USED_DTS := y
CONFIG_ASR_SDIO := n
CONFIG_ASR_USB := y
endif
###################### 5531(canon) #######################
ifeq ($(CONFIG_ASR5531), m)
CONFIG_BOARD=canon
# as module(m) or as built-in(y)
subdir-ccflags-y += -DCONFIG_ASR5531
ifeq ($(CONFIG_ASR_SDIO), y)
	subdir-ccflags-y += -DASR_UART_DOWNLOAD_ROMCODE -DSDIO_BLOCK_SIZE=128
	MODULE_NAME ?= asr5532s
else
	MODULE_NAME ?= asr5532u
endif
obj-$(CONFIG_ASR5531) := $(MODULE_NAME).o
ccflags-y += -DASR_WIFI_CONFIG_SUPPORT
#support roaming for rrm,btm
subdir-ccflags-y += -DCFG_ROAMING
#subdir-ccflags-y += -DCFG_FWLOG_2HOST
###################### 5505(lega) #######################
else ifeq ($(CONFIG_ASR5505), m)
CONFIG_BOARD=lega
MODULE_NAME ?= asr5505
obj-$(CONFIG_ASR5505) := $(MODULE_NAME).o
subdir-ccflags-y += -DCONFIG_ASR5505 -DSDIO_BLOCK_SIZE=128
#subdir-ccflags-y += -DCFG_FOR_JICHENG_REGD
###################### 5825(duet) #######################
else ifeq ($(CONFIG_ASR5825), m)
CONFIG_BOARD=duet
# extra DEBUG config for key debug log
CONFIG_ASR_KEY_DBG := y
# as module(m) or as built-in(y)
MODULE_NAME ?= asr5825
obj-$(CONFIG_ASR5825) := $(MODULE_NAME).o
subdir-ccflags-y += -DCONFIG_ASR5825 -DSDIO_BLOCK_SIZE=32
# out-of-band interrupt.
#subdir-ccflags-y += -DOOB_INTR_ONLY
###################### 595X(alto) #######################
else ifeq ($(CONFIG_ASR595X), m)
CONFIG_BOARD=alto
# extra DEBUG config for key debug log
#CONFIG_ASR_KEY_DBG := y
# as module(m) or as built-in(y)
MODULE_NAME ?= asr595x
obj-$(CONFIG_ASR595X) := $(MODULE_NAME).o
subdir-ccflags-y += -DCONFIG_ASR595X -DSDIO_BLOCK_SIZE=32 -DCONFIG_TWT -DP2P_SUPPORT -DCONFIG_ASR_P2P_DEBUGFS
#subdir-ccflags-y += -DCONFIG_ASR595X -DCONFIG_TWT
subdir-ccflags-y += -DCFG_ROAMING
###################### 596X(bass) #######################
else ifeq ($(CONFIG_ASR596X), m)
CONFIG_BOARD=bass
CONFIG_ASR595X := m
# extra DEBUG config for key debug log
#CONFIG_ASR_KEY_DBG := y
# as module(m) or as built-in(y)
MODULE_NAME ?= asr596x
obj-$(CONFIG_ASR595X) := $(MODULE_NAME).o
subdir-ccflags-y += -DCONFIG_ASR596X -DSDIO_BLOCK_SIZE=32 -DCONFIG_TWT
#need set ASR595X MACRO to compile
subdir-ccflags-y += -DCONFIG_ASR595X
subdir-ccflags-y += -DBASS_SUPPORT
else
CONFIG_BOARD=unknow
endif
CONFIG_ABSPATH=$(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))))

EXTRA_CFLAGS += -I$(srctree)/$(src)/drv_code/include
#EXTRA_CFLAGS += -I$(src)/drv_code/include
ARCH ?= arm
PWD := $(shell pwd)
BUILD_DIR = $(PWD)/out/tmp
SRC = $(PWD)
BUILD_DIR_MAKEFILE = $(BUILD_DIR)/Makefile

#include $(CONFIG_ABSPATH)/$(CONFIG_BOARD)_defconfig.mk

# without bootroom debug,not download firmware
CONFIG_ASR_NO_BOOTROOM ?= n

CONFIG_ASRSDIO_LOOPBACK ?= n

###################### use dts or not #######################
#use CONFIG_NOT_USED_DTS to test whether need dts to compile
ifeq ($(CONFIG_ASR_USB), y)
CONFIG_ASR_SDIO := n
else ifeq ($(CONFIG_ASR_SDIO), y)
subdir-ccflags-y += -DSDIO_DEAGGR
CONFIG_ASR_USB := n
else
CONFIG_ASR_USB ?= n
CONFIG_ASR_SDIO ?= n
endif
###############################################################
#out-of-order pkt handle.
subdir-ccflags-y += -DCFG_OOO_UPLOAD

CONFIG_NOT_USED_DTS ?= n

CONFIG_ASR_USB ?= n
CONFIG_ASR_SDIO ?= n
CONFIG_ASR5531 ?= n
CONFIG_ASR5505 ?= n
CONFIG_ASR5825 ?= n
CONFIG_ASR595X ?= n
CONFIG_DEBUG_FS := y
CONFIG_ASR_DBG := y
CONFIG_DYNAMIC_DEBUG=y
#CONFIG_WIRELESS_EXT=y
#CONFIG_WEXT_PRIV=y

#confict with ASR_REDUCE_TCP_ACK, can't enable them together
CONFIG_ASR_NAPI := y
#Generic Receive Offload
CONFIG_ASR_GRO := y

###################### build info      #######################
export BUILD_MACHINE_INFO ?=$(shell whoami)@$(shell hostname)
export BUILD_DATE_INFO ?=$(shell date +"%Y%m%d")
###################### used for debug #######################
#CONFIG_OLD_USB := y
#CONFIG_FW_HAVE_NOT_MM_INFO_MSG := y
ASR_DRV_DEBUG_TIMER := n

###################### Platform Related #######################
CONFIG_PLATFORM_ARM_RPI := n
CONFIG_PLATFORM_ARM_AARCH64 := n
CONFIG_PLATFORM_I386_PC ?= n
CONFIG_PLATFORM_FULLHAN_ARM := n
CONFIG_PLATFORM_HI3516_ARM := n
CONFIG_PLATFORM_RASPI_AARCH64 ?= n

##################### Optimization Flags ##########################
#EXTRA_CFLAGS += -O2
##################### Compile-time Warnings #######################
#EXTRA_CFLAGS += -Wno-unused-variable
#EXTRA_CFLAGS += -Wno-unused-label
#EXTRA_CFLAGS += -Wno-unused-function

# Power Save
CONFIG_POWER_SAVE ?= n

# as module(m) or as built-in(y)
MODULE_NAME ?= asrunknown

ccflags-y += -DASR_WIFI_CONFIG_SUPPORT
# Enable drv_sme
CONFIG_SME = y
# Enable drv_sme
#CONFIG_SAE = y
ifeq ($(CONFIG_ASRSDIO_LOOPBACK), m)
MODULE_SDIO_LOOPBACK_NAME ?= asrsdiodrv
obj-$(CONFIG_ASRSDIO_LOOPBACK) += $(MODULE_SDIO_LOOPBACK_NAME).o
subdir-ccflags-y += -DCONFIG_ASR_SDIO_LOOPBACK
$(MODULE_SDIO_LOOPBACK_NAME)-y := drv_code/asr_sdio_loopback_test.o
endif
#==========HOST PLATFORM PARAM==========
ifeq ($(CONFIG_MACH_PXA1826_DT), y)
ccflags-y += -DASR_MACH_PXA1826_CLK_EN
ccflags-y += -DASR_MODULE_POWER_PIN_SUPPORT
ccflags-y += -DASR_HEARTBEAT_DETECT
#support custor wifi config function
#ccflags-y += -DASR_WIFI_CONFIG_SUPPORT
ccflags-y += -DASR_SDIO_HOST_SDHCI
#support reset wifi module function
ccflags-y += -DASR_MODULE_RESET_SUPPORT
endif

ifeq ($(CONFIG_MACH_FINCH), y)
ccflags-y += -DASR_FINCH_MODULE_POWER_SUPPORT
ccflags-y += -DLOAD_MAC_ADDR_FROM_NODE
#ccflags-y += -DASR_SDIO_ISR_DATAWORKER
ccflags-y += -DASR_HEARTBEAT_DETECT
ccflags-y += -DASR_SDIO_HOST_SDHCI
#support reset wifi module function
ccflags-y += -DASR_MODULE_RESET_SUPPORT

ifdef CONFIG_ASR5825
CONFIG_MODULE_AL5825_DB = y
endif
endif

ifeq ($(CONFIG_MACH_FH8632), y)
ccflags-y += -DLOAD_MAC_ADDR_FROM_FW
#support custor wifi config function
#ccflags-y += -DASR_WIFI_CONFIG_SUPPORT

CONFIG_DEBUG_FS := n
CONFIG_ASR_DBG := n
CONFIG_DYNAMIC_DEBUG=n
CONFIG_ASR_SAE = n
# Enable drv_sme
CONFIG_SME = n
# Enable drv_sme
CONFIG_SAE = n
endif

#=======================================

#==========CLIENT WIFI MODULE PARAM==========
ifdef CONFIG_MODULE_AL5825_DB
ccflags-y += -DASR_BOOT_TO_RTS_PIN_SUPPORT
endif
#=======================================


# FW VARS
ifeq ($(CONFIG_ASR595X), m)
subdir-ccflags-y += -DNX_VIRT_DEV_MAX=4
else
subdir-ccflags-y += -DNX_VIRT_DEV_MAX=2
endif
subdir-ccflags-y += -DNX_REMOTE_STA_MAX=16
subdir-ccflags-y += -DNX_CHAN_CTXT_CNT=3 -DSDIO_RXBUF_SPLIT

###################### build info      #######################
subdir-ccflags-y += -DCFG_BUILD_MACHINE=$(BUILD_MACHINE_INFO)
subdir-ccflags-y += -DCFG_BUILD_DATE=$(BUILD_DATE_INFO)
# Enable A-MSDU support (need FW support)
## Select this if FW is compiled with AMSDU support
CONFIG_ASR_SPLIT_TX_BUF ?= n
## Select this TO send AMSDU
CONFIG_ASR_AMSDUS_TX ?= n

# Enable HW queue for Broadcast/Multicast traffic (need FW support)
CONFIG_ASR_BCMC ?= y

#read MAC addr form fw efuse
ccflags-y += -DLOAD_MAC_ADDR_FROM_FW

# extra DEBUG config for function entry
#CONFIG_ASR_DBG ?= n

# extra DEBUG config for key debug log
#CONFIG_ASR_KEY_DBG ?= y

# extra DEBUG config for key debug log
CONFIG_ASR_SNIFFER_SUPPORT ?= y


# Enable 8k rx amsdu
CONFIG_ASR_AMSDU_RX_8K ?= n

# use SDIO interface
#CONFIG_ASR_SDIO ?=n

# use USB interface
#CONFIG_ASR_USB ?=y

#reduce tcp ack frequence
#ccflags-y += -DASR_REDUCE_TCP_ACK

#support hidden ssid for ap mode
ccflags-y += -DCFG_AP_HIDDEN_SSID

$(MODULE_NAME)-y := drv_code/asr_msg_tx.o     \
               drv_code/asr_msg_rx.o          \
               drv_code/asr_utils.o           \
               drv_code/asr_cmds.o            \
               drv_code/asr_irqs.o            \
               drv_code/asr_ioctl.o           \
               drv_code/asr_cfgfile.o         \
               drv_code/asr_strs.o            \
               drv_code/asr_rx.o              \
               drv_code/asr_tx.o              \
               drv_code/asr_txq.o             \
               drv_code/asr_main.o            \
               drv_code/asr_mod_params.o      \
               drv_code/asr_platform.o        \
               drv_code/asr_target_platform.o \
               drv_code/asr_ate.o             \
               drv_code/ipc_host.o            \
               drv_code/asr_pm.o

ccflags-y += -I$(src)/drv_code/include
ccflags-$(CONFIG_ASR_SPLIT_TX_BUF) += -DCONFIG_ASR_SPLIT_TX_BUF

ifeq ($(CONFIG_ASR_SDIO), y)
ccflags-$(CONFIG_ASR_SDIO) += -DCONFIG_ASR_SDIO

$(MODULE_NAME)-y += drv_code/asr_sdio.o
endif

ifeq ($(CONFIG_ASR_USB), y)
ccflags-$(CONFIG_ASR_USB) += -DCONFIG_ASR_USB
$(MODULE_NAME)-y += drv_code/asr_usb.o
endif
ifeq ($(CONFIG_OLD_USB), y)
ccflags-$(CONFIG_OLD_USB) += -DCONFIG_OLD_USB
endif

ifeq ($(CONFIG_DEBUG_FS), y)
subdir-ccflags-y += -DCONFIG_ASR_DEBUGFS
$(MODULE_NAME)-y += drv_code/asr_debugfs.o
endif

ifeq ($(CONFIG_ASR_SPLIT_TX_BUF), y)
ccflags-$(CONFIG_ASR_AMSDUS_TX) += -DCONFIG_ASR_AMSDUS_TX
endif

ifeq ($(CONFIG_ASR_SNIFFER_SUPPORT), y)
ccflags-$(CONFIG_ASR_SNIFFER_SUPPORT) += -DCFG_SNIFFER_SUPPORT
$(MODULE_NAME)-y += drv_code/asr_idle_mode.o
endif

ccflags-$(CONFIG_ASR_DBG) += -DCONFIG_ASR_DBG
ccflags-$(CONFIG_ASR_KEY_DBG) += -DCONFIG_ASR_KEY_DBG
ccflags-$(CONFIG_SME) += -DCONFIG_SME
ccflags-$(CONFIG_SAE) += -DCONFIG_SAE

ccflags-$(CONFIG_ASR_NAPI) += -DCONFIG_ASR_NAPI
ccflags-$(CONFIG_ASR_GRO) += -DCONFIG_ASR_GRO

ccflags-$(CONFIG_ASR_NO_BOOTROOM) += -DCONFIG_ASR_NO_BOOTROOM

ccflags-y += -DCONFIG_USER_MAX=1

ifeq ($(CONFIG_ASR_BCMC), y)
ccflags-y += -DNX_TXQ_CNT=5
else
ccflags-y += -DNX_TXQ_CNT=4
endif

ifeq ($(CONFIG_ASR_AMSDU_RX_8K), y)
ccflags-y += -DWLAN_AMSDU_RX_LEN=8192
else
#amsdu len 3960 = data 3839 + trans head 60 + hw info 34 reserve 27
# frm_len + pld_offseta ,  max is NX_MAX_AMSDU + host_rx_desc + machdr+llcsnap
ccflags-y += -DWLAN_AMSDU_RX_LEN=4608
endif


ifeq ($(ASR_DRV_DEBUG_TIMER), y)
ccflags-$(ASR_DRV_DEBUG_TIMER) += -DASR_DRV_DEBUG_TIMER
endif

ifeq ($(CONFIG_NOT_USED_DTS), y)
ccflags-$(CONFIG_NOT_USED_DTS) += -DCONFIG_NOT_USED_DTS
endif

ifeq ($(CONFIG_PLATFORM_I386_PC), y)
ccflags-$(CONFIG_PLATFORM_I386_PC) += -DCONFIG_PLATFORM_I386_PC -DCONFIG_APSTA_MIXED_SUPPORT -DCONFIG_WIRELESS_EXT -DCONFIG_WEXT_PRIV
endif

ifeq ($(CONFIG_FW_HAVE_NOT_MM_INFO_MSG), y)
ccflags-$(CONFIG_FW_HAVE_NOT_MM_INFO_MSG) += -DCONFIG_FW_HAVE_NOT_MM_INFO_MSG
endif


ifeq ($(CONFIG_ASR_SDIO), y)
#when enable bg scan for roaming, should not enable stats rates timer to block bg scan.
# canon sdio not use this
ifneq ($(CONFIG_BOARD), canon)
ccflags-y += -DASR_STATS_RATES_TIMER
endif
endif

ifeq ($(CONFIG_ASR_PM), y)
ccflags-y += -DCONFIG_ASR_PM
$(MODULE_NAME)-y += drv_code/asr_pm.o

ccflags-$(CONFIG_AUTO_SUSPEND_RESUME) += -DCONFIG_AUTO_SUSPEND_RESUME
ccflags-$(CONFIG_GPIO_WAKEUP_MOD) += -DCONFIG_GPIO_WAKEUP_MOD

ifeq ($(CONFIG_GPIO_WAKEUP_HOST), y)
ccflags-y += -DCONFIG_GPIO_WAKEUP_HOST

ifeq ($(CONFIG_HIGH_LEVEL), y)
ccflags-y += -DCONFIG_MOD_ACTIVE_LEVEL=1
else
ccflags-y += -DCONFIG_MOD_ACTIVE_LEVEL=0
endif
endif
endif

ccflags-$(CONFIG_ASR_USB_PM) += -DCONFIG_ASR_USB_PM

#build driver out of kernel dir
ifeq ($(KERNELRELEASE),)

ifeq ($(CONFIG_PLATFORM_I386_PC), y)
SUBARCH := $(shell uname -m | sed -e s/i.86/i386/)
ARCH := $(SUBARCH)
TOOLCHAIN_DIR ?=
KVER  := $(shell uname -r)
KERNEL_DIR=/lib/modules/$(KVER)/build
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/
STAGING_DIR := /lib/modules/$(KVER)/kernel/drivers/staging
else ifeq ($(CONFIG_PLATFORM_FULLHAN_ARM), y)
PREFIX=${PWD}/../../../..
KERNEL_DIR=${PREFIX}
STAGING_DIR=${PREFIX}
TOOLCHAIN_DIR=arm-fullhan-linux-uclibcgnueabi-
else ifeq ($(CONFIG_PLATFORM_HI3516_ARM), y)
PREFIX=${PWD}/../../../..
KERNEL_DIR=${PREFIX}/
STAGING_DIR=${PREFIX}
TOOLCHAIN_DIR=arm-himix200-linux-
else ifeq ($(CONFIG_PLATFORM_RASPI_AARCH64), y)
ARCH := arm64
PREFIX=${PWD}/../../../..
KERNEL_DIR=${PREFIX}/
STAGING_DIR=${PREFIX}
TOOLCHAIN_DIR=aarch64-linux-gnu-
else
# for 1826
PREFIX=${PWD}/../../../../../..
KERNEL_DIR=${PREFIX}/build_dir/toolchain-arm_cortex-a7+neon-vfpv4_gcc-4.8-linaro_uClibc-1.0.25_eabi/linux-3.10.33/
STAGING_DIR=${PREFIX}/staging_dir/target-arm_cortex-a7+neon-vfpv4_uClibc-1.0.25_eabi/root-mmp/lib/modules/3.10.33/
TOOLCHAIN_DIR=${PREFIX}/owtoolchain/linux64/bin/arm-openwrt-linux-

endif



ifeq ($(CONFIG_PLATFORM_I386_PC), y)
all: modules
else
all: modules drvcopy
endif

modules:$(BUILD_DIR_MAKEFILE)
	echo $(BUILD_MACHINE_INFO) $(BUILD_DATE_INFO)
	make V=s -j32 $(C_INCLUDE_PATH) -C $(KERNEL_DIR) STAGING_DIR=$(STAGING_DIR) \
	ARCH=$(ARCH) \
	CROSS_COMPILE=$(TOOLCHAIN_DIR) \
	M=$(BUILD_DIR) src=$(SRC) \
	modules
	cp $(BUILD_DIR)/*.ko $(PWD)/out/
drvcopy:
	cp $(BUILD_DIR)/*.ko $(STAGING_DIR)

$(BUILD_DIR_MAKEFILE):
	mkdir -p $(PWD)/out/tmp/drv_code
	touch $(BUILD_DIR)/Makefile
clean:
	make V=s -j32 -C $(KERNEL_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(TOOLCHAIN_DIR) M=$(BUILD_DIR) src=$(SRC) clean
	rm -rf $(PWD)/out
	rm -rf $(PWD)/drv_code/.*.cmd
	rm -rf $(PWD)/drv_code/*.o
	rm -rf $(PWD)/drv_code/*.ko
	rm -rf $(PWD)/drv_code/*.mod
	rm -rf $(PWD)/drv_code/*.mod.c
	rm -rf $(PWD)/drv_code/.*.d
	rm -rf $(PWD)/drv_code/built-in.a
	rm -rf $(PWD)/drv_code/modules.order

endif
