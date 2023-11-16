#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2021 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>
#
# This script is a helper utility to get configuration value from image_cfg.json

import os
import sys
import argparse
import image_cfg_parser

def get_cfg_from_json(cfgfile, nodepath):
    cfg = image_cfg_parser.parse_image_cfg(cfgfile)
    if cfg == None:
        sys.exit(1)

    nodes = nodepath.split('/')
    if len(nodes) <= 1:
        print('Please provide node path.')
        sys.exit(1)
    dnode = cfg
    for n in nodes:
        if n not in dnode:
            print(n + " is not exist")
            sys.exit(1)
        dnode = dnode[n]

    ret = ''
    if isinstance(dnode, dict):
        print('Not support to get dict object')
        sys.exit(1)
    elif isinstance(dnode, list):
        ret = ','.join(dnode)
    elif isinstance(dnode, str):
        ret = dnode
    else:
        ret = str(dnode)

    return ret


# The purpose of this script is get configuration value from image_cfg.json,
# It is useful for bash shell.
# 
# The script accepts two arguments:
#   --config: image_cfg.json file path
#   --path: the configuration path, e.g. "image/info/media/type"
# Return value will be printed to shell in string

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", type=str,
                        help="image configuration file name")
    parser.add_argument("-p", "--path", type=str,
                        help="configuration node path")
    args = parser.parse_args()
    if args.config == None:
        print('Error, option --config is required.')
        sys.exit(1)
    # If user not specified data directory, use current directory as default
    if args.path== None:
        print('Error, option --path is required.')
        sys.exit(1)

    ret = get_cfg_from_json(args.config, args.path)
    print(ret)

