#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

# Copyright (C) 2022-2024 ArtInChip
# Li Siyao <siyao.li@artinchip.com>

import os
import sys
import argparse
import subprocess
import re


def print_error(string):
    print("\t\033[1;31;40m" + string + "\033[0m")


def print_pass(string):
    print("\t\033[1;32;40m" + string + "\033[0m")


def print_warning(string):
    print("\t\033[1;33;40m" + string + "\033[0m")


def list_module(dtbname):
    try:
        import fdt
    except Exception as e:
        print_warning("The fdt package is not installed, so module list cannot be executed")
        sys.exit()

    list_tit = ["Module", "Version", "Device"]
    whitelist_mod = ['syscfg', 'dma-controller', 'reset', 'clock', 'dramc']
    dict_nodes = {}
    dis_dict_nodes = {}

    # Open dtb file and parse it
    try:
        with open(dtbname, "rb") as file:
            dtb_data = file.read()
            dt = fdt.parse_dtb(dtb_data)
            file.close()
    except Exception as e:
        print_warning("Failed to open the U-boot.dtb file.")
        sys.exit()

    # Get module alias
    dict_alia = {}
    alia = dt.get_node("/aliases")
    for i in range(len(alia.props)):
        dict_alia[alia.props[i].data[0]] = alia.props[i].name

    for path, nodes, props in dt.walk():
        node = dt.get_node(path)
        pinctrl = node.get_property("pinctrl-0")
        status = node.get_property("status")
        compatible = node.get_property("compatible")
        # Get nodes with status okay
        if status and status.value == "okay":
            dict_nodes[path] = node.name
        elif compatible:
            dis_dict_nodes[path] = node.name

    all_node_arr = [dict_nodes, dis_dict_nodes]

    for type_dict_node in all_node_arr:
        # Replace unclear node names in the node with aliases
        for key in list(type_dict_node.keys()):
            if key in dict_alia.keys():
                type_dict_node[key] = dict_alia[key]

        for key in list(type_dict_node.keys()):
            # Delete nodes that are not modules but have status okay,
            # such as bus_xxx
            if not re.match(r'/soc/.*@\d+', key):
                del type_dict_node[key]
            # Delete nodes that are in the whitelist
            elif re.match(r'/soc/(.*)@\d+', key).groups()[0] in whitelist_mod:
                del type_dict_node[key]

    # Delete modules that are open on the device but closed on the module
    for dis_node in dis_dict_nodes.keys():
        disnode_to_remove = [key for key in dict_nodes.keys() if key.startswith(dis_node)]
        for disnode_id in disnode_to_remove:
            del dict_nodes[disnode_id]

    dict_chan = {}
    dict_dev = {}
    dict_mod = {}
    # Sequentially match the name: device selected by the module,
    # channel selected by the module, module.
    for key in list(dict_nodes.keys()):
        if re.match(r'(/soc/.*@\d+)(/.*@\d+)', key):
            dict_dev[key] = dict_nodes[key]
        elif re.match(r'(/soc/.*@\d+)(/.*)', key):
            dict_chan[key] = dict_nodes[key]
        elif re.match(r'/soc/.*@\d+', key):
            dict_mod[key] = dict_nodes[key]
        del dict_nodes[key]

    # Append the same module to the same key in the dictionary
    # and match compatible.
    tag_mod = {}
    for key in list(dict_mod.keys()):
        matched = re.match(r'(.*)(\d+)', dict_mod[key])
        if matched:
            if matched.groups()[0] not in tag_mod:
                node = dt.get_node(key)
                compatible = node.get_property("compatible").data[0]
                tag_mod[matched.groups()[0]] = [compatible, dict_mod[key]]
            else:
                tag_mod[matched.groups()[0]].append(dict_mod[key])
        else:
            node = dt.get_node(key)
            compatible = node.get_property("compatible").data[0]
            tag_mod[dict_mod[key]] = [compatible]

    for key in list(dict_chan.keys()):
        if re.match(r'(.*)(\d+)', dict_chan[key]):
            matched = re.match(r'(.*)(\d+)', dict_chan[key])
            if matched.groups()[0] not in tag_mod:
                tag_mod[matched.groups()[0]] = [dict_chan[key]]
            else:
                tag_mod[matched.groups()[0]].append(dict_chan[key])
        else:
            tag_mod[dict_chan[key]] = []

    # Sort in ascending order based on the first value of each element
    tag_mod = dict(sorted(tag_mod.items(), key=lambda x: x[0]))
    for key, value in tag_mod.items():
        value[1:] = list(reversed(value[1:]))

    # Get the module corresponding to the device as the key,
    # and the device name as the value.
    dict_mod_dev = {}
    for key_dev in list(dict_dev.keys()):
        matched = re.match(r'(/soc/.*@\d+)(/.*)', key_dev)
        if matched.groups()[0] in dict_mod.keys():
            dev_mod = re.sub(r'/soc/.*@\d+/.*', dict_mod[matched.groups()[0]],
                             key_dev)
            dict_mod_dev[dev_mod] = dict_dev[key_dev]

    # step: Interval between each title
    step = 1
    len_ver = 4
    len_total_str = 0
    max_len_dev_str = 0
    num_tit = len(list_tit)
    min_len_item = [len(s) for s in list_tit]

    # Get the length occupied by the longest device string
    # in the device column.
    sublists = [value[1:] for key, value in tag_mod.items()]
    for i in range(len(sublists)):
        len_dev_str = 0
        list_dev_cnt = 0
        for j in sublists[i]:
            list_dev_cnt += step
            len_dev_str += len(j)
            if j in dict_mod_dev.keys():
                len_dev_str += len(dict_mod_dev[j])
        len_dev_str += list_dev_cnt - step
        if len_dev_str > max_len_dev_str:
            max_len_dev_str = len_dev_str
    len_dev = max_len_dev_str
    len_mod = max(len(key) for key in tag_mod.keys())

    list_len = [len_mod, len_ver, len_dev]

    for i in range(num_tit):
        if list_len[i] < min_len_item[i]:
            list_len[i] = min_len_item[i]
        len_total_str += list_len[i]
    len_total = len_total_str + (num_tit - 1) * step
    print("=" * len_total)

    # Print the title
    for i in range(num_tit):
        print("{:<{}}".format(list_tit[i], list_len[i] + step), end='')
    print("")

    for i in range(num_tit):
        print("-" * list_len[i], end='')
        if i != num_tit:
            print(" ", end='')
    print("")

    # Print the module, version number, and device
    for key in tag_mod.keys():
        print("{:<{}}".format(key, list_len[0] + step), end='')
        if len(tag_mod[key]) > 0:
            cnt = 0
            for value in tag_mod[key]:
                cnt += 1
                if cnt == 1:
                    pattern = r"v\d+\.\d+"
                    result = re.search(pattern, value)
                    if result:
                        print("{:<{}}".format(result.group(), list_len[1]),
                              end='')
                    else:
                        print("{:<{}}".format('', list_len[1]), end='')
                else:
                    print(value, end='')
                if value in dict_mod_dev.keys():
                    print("[" + dict_mod_dev[value] + "]", end='')
                print(" ", end='')
        print("")
    print("=" * len_total)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-d", "--datadir", type=str,
                       help="input image data directory")
    args = parser.parse_args()
    list_module(args.datadir + "/u-boot.dtb")
