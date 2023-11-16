#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2021 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>
#
# NAND list parser

import os
import sys
import json
import argparse
from collections import namedtuple
from collections import OrderedDict

def load_nand_list(nandlist):
    """ Load NAND list json file
    Args:
        nandlist: Configuration file name
    """
    with open(nandlist, "r") as f:
        lines = f.readlines()
        jsonstr = ""
        for line in lines:
            sline = line.strip()
            if sline.startswith("//"):
                continue
            jsonstr += sline
        # Use OrderedDict is important, we need to iterate FWC in order.
        jsonstr = jsonstr.replace(",}", "}")
        nands = json.loads(jsonstr, object_pairs_hook=OrderedDict)
    return nands 

def get_formatted_id(nand_id):
    siz = len(nand_id)
    formatted_id = []
    for v in nand_id:
        formatted_id.append(v.upper())
    max_len = 32
    fill_cnt = max_len - siz
    while fill_cnt > 0:
        formatted_id.append('0X00')
        fill_cnt -= 1

    return formatted_id

def find_device_by_id(nandlist, nandid):
    keys = nandlist.keys()
    expect = get_formatted_id(nandid.split(','))
    for k in keys:
        device = nandlist[k]
        nand_id = get_formatted_id(device['nand_id'])
        if nand_id == expect:
            return device
    return None

def get_device_size(nandlist, nandid):
    device = find_device_by_id(nandlist, nandid)
    if device == None:
        print('Device is not found')
        sys.exit(1)
    total_size = 0
    page_size = device['page_size']
    pages_per_block = device['pages_per_block']
    blocks_per_lun = device['blocks_per_lun']
    planes_per_lun = device['planes_per_lun']
    luns_per_target = device['luns_per_target']
    targets = device['targets']
    total_size = page_size * pages_per_block * blocks_per_lun * luns_per_target * targets
    print(total_size)
    sys.exit(0)

def gen_param_for_ubifs(nandlist, nandid, volumesize):
    device = find_device_by_id(nandlist, nandid)
    if device == None:
        print('Device is not found')
        sys.exit(1)
    page_size = device['page_size']
    pages_per_block = device['pages_per_block']
    blocksize = page_size * pages_per_block
    if 'min_io' in device:
        min_io = device['min_io']
    else:
        min_io = page_size
    leb_size = blocksize -  2 * min_io
    leb_count = int(volumesize / leb_size)
    
    ubiparam = '-m {} -c {} -e {}'.format(min_io, leb_count, leb_size)
    print(ubiparam)
    sys.exit(0)

# NAND device list is a database which contain all supported NAND device
# This script is used to
#  - parse and get NAND device size
#  - generate parameter to mkfs.ubifs for specific NAND device
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--op", type=str,
            help="Operation name, value can be: 'get_nand_size',\
            'gen_ubifs_param'")
    parser.add_argument("-n", "--nand_list", type=str,
                        help="NAND list file name")
    parser.add_argument("-i", "--id", type=str,
                        help="NAND ID")
    parser.add_argument("-s", "--size", type=str,
                        help="Volume size")
    args = parser.parse_args()

    if args.nand_list== None:
        print('Error, option --nand_list is required.')
        sys.exit(1)
    if args.id== None:
        print('Error, option --idis required.')
        sys.exit(1)
    nandlist = load_nand_list(args.nand_list)
    if args.op == 'get_nand_size':
        get_device_size(nandlist, args.id)
    if args.op == 'gen_ubifs_param':
        volumesize = int(args.size)
        gen_param_for_ubifs(nandlist, args.id, volumesize)
    print('No operation is performed.')
    sys.exit(1)
