#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2021 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>
#
# Get information from U-Boot ENV

import os
import sys
import argparse

def decode_size(size_str):
    size_str = size_str.strip()
    size_str = size_str.upper()
    if size_str == '-':
        return -1
    if size_str.endswith('K') or size_str.endswith('KIB'):
        size_str = size_str.replace('KIB', '')
        size_str = size_str.replace('K', '')
        size = int(size_str) * 1024
    elif size_str.endswith('M') or size_str.endswith('MIB'):
        size_str = size_str.replace('MIB', '')
        size_str = size_str.replace('M', '')
        size = int(size_str) * 1024 * 1024
    elif size_str.endswith('G') or size_str.endswith('GIB'):
        size_str = size_str.replace('GIB', '')
        size_str = size_str.replace('G', '')
        size = int(size_str) * 1024 * 1024 * 1024
    else:
        size = int(size_str)

    return size

def device_partition_parser(parts_table, total_size):
    dpart = {}
    table = parts_table.split(':')
    dpart['device_name'] = table[0]
    dpart['total_size'] = total_size

    parts = table[1].split(',')
    used_size = 0
    for part in parts:
        part = part.split(')')[0]
        part_info = part.split('(')
        size = decode_size(part_info[0])
        if size < 0:
            size = total_size - used_size
        dpart[part_info[1].strip()] = size
        used_size += size
    return dpart

def get_mmc_partition_size(f, total_size, part):
    return size

def get_nand_partition_size(f, total_size, part):
    mtdpart_val = ''
    ubivols_val = ''
    size = -1

    lines = f.readlines()
    for line in lines:
        sline = line.strip()
        if sline.startswith('#'):
            continue
        if sline.startswith('parts_nand'):
            mtdpart_val = sline.split('=')[1]
        if sline.startswith('ubivols_nand'):
            ubivols_val = sline.split('=')[1]

    if len(mtdpart_val) == 0:
        print('Get parts_nand failed.')
        sys.exit(1)
    if len(ubivols_val) == 0:
        print('Get ubivols_nand failed.')
        sys.exit(1)

    mtdpart_d = device_partition_parser(mtdpart_val, total_size)
    parts = part.split(':')
    if len(parts) == 1:
        # Get MTD partition size
        size = mtdpart_d[parts[0].strip()]
    else:
        # Get UBI volume size
        ubidevs = ubivols_val.split(';')
        for ubi in ubidevs:
            ubi_info = ubi.split(':')
            ubipart = ubi_info[0].strip()
            if ubipart == parts[0].strip():
                ubipart_size = mtdpart_d[ubipart]
                ubipart_d = device_partition_parser(ubi, ubipart_size)
                size = ubipart_d[parts[1]]

    return size

def get_nor_partition_size(f, total_size, part):
    return size

def get_partition_size(env, media, total_size, part):
    if not os.path.exists(env):
        print(env + 'is not exist')
        sys.exit(1)
    size = -1
    with open(env, 'r')  as f:
        if media == 'spi-nand' or media == 'raw-nand':
            size = get_nand_partition_size(f, total_size, part)
        if media == 'spi-nor':
            size = get_nor_partition_size(f, total_size, part)
        if media == 'mmc':
            size = get_mmc_partition_size(f, total_size, part)
    if size < 0:
        print('Get size failed.')
        sys.exit(1)
    return size

# Partition table is defined in env.txt
# This script tool is used to parse and get partition information from env.txt
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--op", type=str,
            help="Operation name, value can be: 'get_part_size'")
    parser.add_argument("-f", "--file", type=str,
                        help="U-Boot ENV file name")
    parser.add_argument("-p", "--part", type=str,
                        help="Partition name")
    parser.add_argument("-m", "--media", type=str,
                        help="Media type")
    parser.add_argument("-s", "--media_size", type=str,
                        help="Media total size")
    args = parser.parse_args()
    if args.file== None:
        print('Error, option --file is required.')
        sys.exit(1)
    if args.op == 'get_part_size':
        envfile = args.file
        part = args.part
        media = args.media
        total_size = decode_size(args.media_size)
        size = get_partition_size(envfile, media, total_size, part)
        # Print to shell
        print(size)
        sys.exit(0)
    print('Error, no operation is performed.\n')
    sys.exit(1)
