# SPDX-License-Identifier: GPL-2.0+

obj-$(CONFIG_ARTINCHIP_VE) += ve_drv.o

ve_drv-y := aic_ve.o ve-uclass.o

ifeq ($(CONFIG_LOGO_DECODE_ARTINCHIP),y)
ve_drv-y += aic_ve_png.o
ve_drv-$(CONFIG_JPEG_LOGO_IMAGE) += aic_ve_jpeg.o ge-uclass.o
endif

