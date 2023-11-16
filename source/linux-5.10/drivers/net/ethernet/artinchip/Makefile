# SPDX-License-Identifier: GPL-2.0-or-later

ccflags-$(CONFIG_ARTINCHIP_GMAC_DEBUG) += -DDEBUG

obj-$(CONFIG_NET_ARTINCHIP_MAC) += aicmac.o
aicmac-objs := aicmac_platform.o aicmac_core.o aicmac_dma.o aicmac_dma_desc.o \
	      aicmac_dma_ring.o aicmac_dma_chain.o aicmac_mac.o \
	      aicmac_1588.o aicmac_hwtstamp.o aicmac_util.o aicmac_macaddr.o\
	      aicmac_mdio.o aicmac_napi.o aicmac_phy.o aicmac_ethtool.o \

aicmac-objs += aicmac_dma_reg.o aicmac_gmac_reg.o

