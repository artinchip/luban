#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2021 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>

import os
import sys
import json
import argparse
from collections import OrderedDict

def parse_image_cfg(cfgfile):
    """ Load image configuration file
    Args:
        cfgfile: Configuration file name
    """
    with open(cfgfile, "r") as f:
        lines = f.readlines()
        jsonstr = ""
        for line in lines:
            sline = line.strip()
            if sline.startswith("//"):
                continue
            jsonstr += sline
        # Use OrderedDict is important, we need to iterate FWC in order.
        jsonstr = jsonstr.replace(",}", "}")
        cfg = json.loads(jsonstr, object_pairs_hook=OrderedDict)
    return cfg
