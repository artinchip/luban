#!/usr/bin/env python3
#coding=utf-8
import sys
import os
import json

def print_error(string):
    print("\t\033[1;31;40m" + string + "\033[0m")

def print_pass(string):
    print("\t\033[1;32;40m" + string + "\033[0m")

def print_warning(string):
    print("\t\033[1;33;40m" + string + "\033[0m")

def check_package():
    try:
        import fdt
    except:
        print_warning("package_check: The fdt package not installed, "\
                      "so package check is not performed")
        sys.exit()

    phandle_dict = {}
    package_error = False
    packageName = ''
    checkModuleDict = {}
    checkModuleList = []

    # check dtb file is exist or not
    if not os.path.isfile(sys.argv[1]):
        print_error(sys.argv[1] + " not exist")
        sys.exit()

    # open dtb file and parse it
    with open(sys.argv[1], "rb") as f:
        dtb_data = f.read()
    dt1 = fdt.parse_dtb(dtb_data)

    # obtain the chip package type
    root_node = dt1.get_node("/")
    package_prop = root_node.get_property("package")
    if package_prop:
        package = package_prop.value.lower()
    else:
        sys.exit()

    chip_path = os.environ["TARGET_CHIP_DIR"]
    package_file = chip_path + "/common/" + "package-" + package + ".json"

    # Check package file is exist or not.
    # If package-*.json not exist, this package is considered to
    # support all modules.
    if not os.path.isfile(package_file):
        sys.exit()

    with open(package_file, 'r') as f:
        packageData = json.load(f)
        packageName = packageData['package-type']
        checkModuleDict = packageData['module']

    checkModuleList = checkModuleDict.keys()

    # Traversing all of the phandle property of DTS, and add it to phandle_dict
    for path, nodes, props in dt1.walk():
        node = dt1.get_node(path)
        phandle = node.get_property("phandle")
        if phandle:
            phandle_dict[phandle.value] = path

    for path, nodes, props in dt1.walk():
        pin_num = 0
        node = dt1.get_node(path)
        pinctrl = node.get_property("pinctrl-0")
        status = node.get_property("status")

        if not status or status.value == "okay":
            if node.name in checkModuleList:
                pin_num = checkModuleDict[node.name]
                if pinctrl:
                    func_node = dt1.get_node(phandle_dict[pinctrl.value])
                    pins_node = func_node.get_subnode("pins")
                    pinmux = pins_node.get_property("pinmux")

                if pin_num:
                    if node.name.find("sdmc") != -1 and len(pinmux) > 8:
                        if not package_error:
                            print_error(packageName + " not support the following modules:")
                        package_error = True
                        print_error("\t" + node.name + " only support 4 lines")
                else:
                    if not package_error:
                        print_error(packageName + " not support the following modules:")
                    package_error = True
                    print_error("\t" + node.name)

if __name__ == "__main__":
    check_package()

