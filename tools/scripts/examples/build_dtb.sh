#!/bin/bash

TOOLCHAIN="~gcc/riscv64-unknown-linux-gnu-"

CHIP=../target/d211
BOARD=${CHIP}/per2_spinor
DEFCONFIG=d211_per2_spinor_defconfig

function dtb_prepare_code()
{
	rm -rf dts
	mkdir -p dts

	# Prepare
	cp -rf ${CHIP}/common/*.dtsi dts/
	cp -rf ${BOARD}/board.dts dts/artinchip-board.dts
	cp -rf ${BOARD}/board-u-boot.dtsi dts/artinchip-board-u-boot.dtsi
	mkdir -p dts/include/dt-bindings/interrupt-controller/
	mkdir -p dts/include/dt-bindings/pinctrl/
	mkdir -p dts/include/dt-bindings/gpio/
	mkdir -p dts/include/dt-bindings/pwm/
	mkdir -p dts/include/dt-bindings/dma/
	mkdir -p dts/include/dt-bindings/clock/
	mkdir -p dts/include/dt-bindings/reset/
	mkdir -p dts/include/dt-bindings/display/
	cp -rf ../source/uboot-2021.10/include/dt-bindings/interrupt-controller/*.h dts/include/dt-bindings/interrupt-controller/
	cp -rf ../source/uboot-2021.10/include/dt-bindings/gpio/*.h dts/include/dt-bindings/gpio/
	cp -rf ../source/uboot-2021.10/include/dt-bindings/pwm/*.h dts/include/dt-bindings/pwm/
	cp -rf ../source/uboot-2021.10/include/dt-bindings/pinctrl/aic*.h dts/include/dt-bindings/pinctrl/
	cp -rf ../source/uboot-2021.10/include/dt-bindings/dma/aic*.h dts/include/dt-bindings/dma/
	cp -rf ../source/linux-5.10/include/dt-bindings/clock/artinchip*.h dts/include/dt-bindings/clock/
	cp -rf ../source/linux-5.10/include/dt-bindings/reset/artinchip*.h dts/include/dt-bindings/reset/
	cp -rf ../source/linux-5.10/include/dt-bindings/display/artinchip*.h dts/include/dt-bindings/display/
}

function dtb_build()
{
	CC_FLAGS="-E -nostdinc -I./dts -I./dts/include \
		-D__ASSEMBLY__ -undef -D__DTS__ -x assembler-with-cpp"

	cat dts/artinchip-board.dts                    >   dts/.artinchip-board.dtb.pre.tmp
	echo '#include "artinchip-board-u-boot.dtsi"' >>   dts/.artinchip-board.dtb.pre.tmp
	cc ${CC_FLAGS} -o dts/.artinchip-board.dtb.dts.tmp dts/.artinchip-board.dtb.pre.tmp

	DTC_FLAGS="-Wno-unit_address_vs_reg -Wno-unit_address_format \
		-Wno-avoid_unnecessary_addr_size -Wno-alias_paths -Wno-graph_child_address \
		-Wno-graph_port -Wno-unique_unit_address -Wno-simple_bus_reg \
		-Wno-pci_device_reg -Wno-pci_bridge -Wno-pci_device_bus_num \
		-R 4 -p 0x1000 -Wno-unit_address_vs_reg -Wno-unit_address_format \
		-Wno-avoid_unnecessary_addr_size -Wno-alias_paths -Wno-graph_child_address \
		-Wno-graph_port -Wno-unique_unit_address -Wno-simple_bus_reg -Wno-pci_device_reg \
		-Wno-pci_bridge -Wno-pci_device_bus_num"

	./dtc ${DTC_FLAGS} -O dtb -o dts/artinchip-board.dtb -b 0 -i dts/ dts/.artinchip-board.dtb.dts.tmp
}

function dtb_install()
{
	mkdir -p install
	cp dts/artinchip-board.dtb install/
}

if [ "x$1" == "xprepare" ]; then
	dtb_prepare_code
elif [ "x$1" == "xbuild" ]; then
	dtb_build
elif [ "x$1" == "xinstall" ]; then
	dtb_install
else
	dtb_build
	dtb_install
fi
