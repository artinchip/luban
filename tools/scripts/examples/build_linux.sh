#!/bin/bash

TOOLCHAIN="~gcc/riscv64-unknown-linux-gnu-"
DEFCONFIG=d211_per2_spinor_defconfig

function linux_prepare_code()
{
	rm -rf linux-5.10/
	cp -rf ../source/linux-5.10/ linux-5.10/
}


function linux_configure()
{
	make ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} ${DEFCONFIG} -C linux-5.10
}

function linux_menuconfig()
{
	make ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} menuconfig -C linux-5.10
}

function linux_build()
{
	make -j21 ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} all Image.lzma -C linux-5.10
}

function linux_install()
{
	mkdir -p install
	cp linux-5.10/arch/riscv/boot/Image* install/
}

function linux_clean()
{
	make ARCH=riscv CROSS_COMPILE=${TOOLCHAIN} distclean -C linux-5.10
}

if [ "x$1" == "xprepare" ]; then
	linux_prepare_code
elif [ "x$1" == "xconfig" ]; then
	linux_configure
elif [ "x$1" == "xbuild" ]; then
	linux_build
elif [ "x$1" == "xclean" ]; then
	linux_clean
elif [ "x$1" == "xmenuconfig" ]; then
	linux_menuconfig
elif [ "x$1" == "xinstall" ]; then
	linux_install
else
	linux_configure
	linux_build
	linux_install
fi
