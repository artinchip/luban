#!/bin/bash

TOOLCHAIN="~gcc/riscv64-unknown-linux-gnu-"

CHIP=../target/d211
BOARD=${CHIP}/per2_spinor
DEFCONFIG=d211_per2_spinor_defconfig

function uboot_prepare_code()
{
	rm -rf uboot-2021.10/
	cp -rf ../source/uboot-2021.10/ uboot-2021.10/

	# Prepare
	find uboot-2021.10/arch/riscv/dts/ -type l |xargs -I {} rm {}
	cp -rf ${CHIP}/common/*.dtsi uboot-2021.10/arch/riscv/dts/
	cp -rf ${BOARD}/board.dts uboot-2021.10/arch/riscv/dts/artinchip-board.dts
	cp -rf ${BOARD}/board-u-boot.dtsi uboot-2021.10/arch/riscv/dts/artinchip-board-u-boot.dtsi
	cp -rf ${BOARD}/*.its.dtsi uboot-2021.10/arch/riscv/dts/
	find uboot-2021.10/include/dt-bindings/ -type l |xargs -I {} rm {}
	cp -rf ../source/linux-5.10/include/dt-bindings/clock/artinchip*.h uboot-2021.10/include/dt-bindings/clock/
	cp -rf ../source/linux-5.10/include/dt-bindings/reset/artinchip*.h uboot-2021.10/include/dt-bindings/reset/
	cp -rf ../source/linux-5.10/include/dt-bindings/display/artinchip*.h uboot-2021.10/include/dt-bindings/display/
}

function uboot_configure()
{
	make ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} ${DEFCONFIG} -C uboot-2021.10
}

function uboot_menuconfig()
{
	make ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} menuconfig -C uboot-2021.10
}

function uboot_build()
{
	make V=1 -d ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} all -C uboot-2021.10
}

function uboot_install()
{
	mkdir -p install

	cp uboot-2021.10/u-boot.bin install/
	cp uboot-2021.10/u-boot.dtb install/
	cp uboot-2021.10/u-boot-dtb.bin install/
	cp uboot-2021.10/u-boot.its install/
	cp uboot-2021.10/kernel.its install/
	cp uboot-2021.10/u-boot-nodtb.bin install/
	cp uboot-2021.10/spl/u-boot-spl.bin install/
	cp uboot-2021.10/spl/u-boot-spl.dtb install/
	cp uboot-2021.10/spl/u-boot-spl-dtb.bin install/
	cp uboot-2021.10/spl/u-boot-spl-nodtb.bin install/
}

function uboot_clean()
{
	make ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} distclean -C uboot-2021.10
}

if [ "x$1" == "xprepare" ]; then
	uboot_prepare_code
elif [ "x$1" == "xconfig" ]; then
	uboot_configure
elif [ "x$1" == "xbuild" ]; then
	uboot_build
elif [ "x$1" == "xclean" ]; then
	uboot_clean
elif [ "x$1" == "xmenuconfig" ]; then
	uboot_menuconfig
elif [ "x$1" == "xinstall" ]; then
	uboot_install
else
	uboot_configure
	uboot_build
	uboot_install
fi
