#!/usr/bin/env python3
#coding=utf-8
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2024 ArtInChip Technology Co., Ltd
# Author: ArtInChip
import sys
import os


def print_error(string):
    print("\t\033[1;31;40m" + string + "\033[0m")


def print_pass(string):
    print("\t\033[1;32;40m" + string + "\033[0m")


def print_warning(string):
    print("\t\033[1;33;40m" + string + "\033[0m")


def check_pinmux(dtbname):
    if not os.path.isfile(dtbname):
        return

    try:
        import fdt
    except ImportError:
        print_warning("The fdt package is not installed, skip pinmux conflict check.")
        sys.exit()

    pinmux_conflict = False
    # Each node referenced has a phandle value. The value of pinctrl-0
    # indicates which node it refers to. So, we can find the pinmux node uses
    # through pinctrl-0 value. The key of phandle_dict is phandle value,
    # the value of phandle_dict is node path.
    phandle_dict = {}
    # The key of pinmux_dict is pin index, the value of pinmux_dict is the node
    # which use the pin
    pinmux_dict = {}

    # open dtb file and parse it
    with open(dtbname, "rb") as f:
        dtb_data = f.read()
    dt1 = fdt.parse_dtb(dtb_data)

    # Traversing all of the phandle property of DTS, and add it to phandle_dict
    for path, nodes, props in dt1.walk():
        node = dt1.get_node(path)
        phandle = node.get_property("phandle")
        if phandle:
            phandle_dict[phandle.value] = path

    for path, nodes, props in dt1.walk():
        node = dt1.get_node(path)
        pinctrl = node.get_property("pinctrl-0")
        status = node.get_property("status")

        if not status or status.value == "okay":
            # check -gpios pinmux
            for i in range(0, len(props)):
                if props[i].name.find("-gpios") != -1:
                    gpio_node = dt1.get_node(phandle_dict[props[i].data[0]])
                    port = gpio_node.get_property("artinchip,bank-port")
                    pin = props[i].data[1]
                    pin_index = port.value << 8 | pin

                    if pin_index in pinmux_dict:
                        if not pinmux_conflict:
                                pinmux_conflict = True
                        print_error(props[i].name +\
                              " pinmux conflicts with " + \
                              pinmux_dict[pin_index])
                        # if is PN port, then should set port.value to 13
                        if port.value == 6:
                            port.value = 13
                        print_error("\tThe conflicting pin: P"+ \
                                    chr(ord('A') + port.value) + str(pin))
                    else:
                        pinmux_dict[pin_index] = path

            # check pinctrl-0 pinmux
            if pinctrl:
                for i in range(0, len(pinctrl.data)):
                    func_node = dt1.get_node(phandle_dict[pinctrl.data[i]])

                    pinmux = []

                    for pins in func_node.nodes:
                        if pins.get_property('pinmux') is not None:
                            pinmux += pins.get_property('pinmux')

                    for j in range(0, len(pinmux)):
                        port = pinmux[j] >> 16
                        pin = pinmux[j] >> 8 & 0xff
                        pin_index = pinmux[j] >> 8

                        if pin_index in pinmux_dict:
                            if path == pinmux_dict[pin_index]:
                                continue
                            pinmux_conflict = True
                            print_error(node.name + \
                                        " pinmux conflicts with " + \
                                        pinmux_dict[pin_index])
                            print_error("\tThe conflicting pin: P" + \
                                        chr(ord('A') + port) + str(pin))
                        else:
                            pinmux_dict[pin_index] = path

    if not pinmux_conflict:
        print_pass("No conflict in pinmux")


if __name__ == "__main__":
    # check dtb file is exist or not
    if not os.path.isfile(sys.argv[1]):
        print_error(sys.argv[1] + " not exist")
        sys.exit()

    check_pinmux(sys.argv[1])
