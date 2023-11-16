ccflags-$(CONFIG_ARTINCHIP_GE_DEBUG) += -DDEBUG

obj-$(CONFIG_ARTINCHIP_GE) += ge_drv.o

ifeq ($(CONFIG_ARTINCHIP_GE_CMDQ),y)
ge_drv-y := ge_cmdq.o
else
ge_drv-y := ge_hw.o ge_normal.o
endif
