# SPDX-License-Identifier: GPL-2.0-only
ccflags-$(CONFIG_ARTINCHIP_DE_DEBUG) += -DDEBUG

subdir-ccflags-y += -I$(src)/panel

obj-$(CONFIG_AIC_DISP_RGB) += hw/rgb_hw.o aic_rgb.o
obj-$(CONFIG_AIC_DISP_MIPI_DBI) += hw/dbi_hw.o aic_dbi.o
obj-$(CONFIG_AIC_DISP_LVDS) += aic_lvds.o
obj-$(CONFIG_AIC_DISP_MIPI_DSI_V1_0) += hw/dsi_hw_v1.0.o aic_dsi.o

obj-$(CONFIG_ARTINCHIP_FB) += aic_disp.o  panel/
aic_disp-y += aic_fb.o hw/de_hw.o aic_de.o
