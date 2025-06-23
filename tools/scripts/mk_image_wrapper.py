#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2024 ArtInChip Technology Co., Ltd
# Author: ArtInChip
import sys
import os

# 该脚本的主要目的是去掉命令行的第一个参数 sys.argv[1]，这个参数是 buildroot 强制传入的
cmd = str(sys.argv[0])
#print("cmd: " + cmd)
cmd_str = cmd.replace('mk_image_wrapper', 'mk_image')
#print("cmd_str: " + cmd_str)
cmd_str = "python3 " + cmd_str + " " + " ".join(sys.argv[2:])
#print("cmd_str: " + cmd_str)

p = os.system(cmd_str)
