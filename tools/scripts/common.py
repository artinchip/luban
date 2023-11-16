#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+

# Copyright (C) 2022, ArtInChip Technology Co., Ltd
# Author: Matteo <duanmt@artinchip.com>

import os, sys, argparse, subprocess
import datetime
import json

global VERBOSE
VERBOSE = False

COLOR_BEGIN = "\033["
COLOR_RED = COLOR_BEGIN + "41;37m"
COLOR_YELLOW = COLOR_BEGIN + "43;30m"
COLOR_WHITE = COLOR_BEGIN + "47;30m"
COLOR_END = "\033[0m"

def cur_date():
    now = datetime.datetime.now()
    tm_str = ('%d-%02d-%02d') % (now.year, now.month, now.day)
    return tm_str

def cur_time():
    now = datetime.datetime.now()
    tm_str = ('%02d:%02d:%02d') % (now.hour, now.minute, now.second)
    return tm_str

def pr_err(string):
    if VERBOSE:
        print(COLOR_RED + '*** '+ cur_time() + ' ' + string + COLOR_END)
    else:
        print(COLOR_RED + '*** '+ string + COLOR_END)

def pr_info(string):
    if VERBOSE:
        print(COLOR_WHITE + '>>> ' + cur_time() + ' ' + string + COLOR_END)
    else:
        print(COLOR_WHITE + '>>> ' + string + COLOR_END)

def pr_warn(string):
    if VERBOSE:
        print(COLOR_YELLOW + '!!! ' + cur_time() + ' ' + string + COLOR_END)
    else:
        print(COLOR_YELLOW + '!!! ' + string + COLOR_END)

def log_verbose(verbose):
    global VERBOSE
    VERBOSE = verbose

def do_system(cmd):
    if VERBOSE:
        print(f'$ {cmd}')
    os.system(cmd)

def stage_log(msg):
    global STAGE_CNT

    STAGE_CNT += 1
    print('\n---------------------------------------------------------------')
    print(str(STAGE_CNT) + '. ' + msg)
    print('---------------------------------------------------------------\n')

def load_json(cfgfile):
    """ Load image configuration file
    Args:
        cfgfile: Configuration file name
    """
    with open(cfgfile, "r") as f:
        lines = f.readlines()
        jsonstr = ""
        for line in lines:
            sline = line.strip()
            if sline.startswith("//") or "// " in sline:
                continue
            jsonstr += sline

        jsonstr = jsonstr.replace(",}", "}").replace(",]", "]")
        try:
            cfg = json.loads(jsonstr)
        except Exception as e:
            pr_err('Failed to parse json file: ' + cfgfile)
            sys.exit(1)

    return cfg
