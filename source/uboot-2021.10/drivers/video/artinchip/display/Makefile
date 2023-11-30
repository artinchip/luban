# SPDX-License-Identifier: GPL-2.0+
subdir-ccflags-y += -I$(src)/panel

obj-$(CONFIG_AIC_DISP_RGB) += aic_rgb.o hw/rgb_hw.o
obj-$(CONFIG_AIC_DISP_MIPI_DBI) += hw/dbi_hw.o aic_dbi.o
obj-$(CONFIG_AIC_DISP_LVDS) += aic_lvds.o
obj-$(CONFIG_AIC_DISP_MIPI_DSI_V1_0) += hw/dsi_hw_v1.0.o aic_dsi.o

obj-$(CONFIG_VIDEO_ARTINCHIP) += aic_disp.o panel/
ifeq ($(CONFIG_SPL_BUILD),y)
aic_disp-y += aic_fb.o hw/de_hw.o aic_de.o
else
aic_disp-y += aic_fb.o hw/de_hw.o aic_de.o aic_display.o
endif
