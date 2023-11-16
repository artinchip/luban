#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_TOUCHSCREEN_ICN81XX)	+= touch_icn81xx.o 
touch_icn81xx-objs :=  cts_i2c_driver.o cts_core.o cts_platform.o  cts_firmware.o icnt8xxx_flash.o init-input.o
