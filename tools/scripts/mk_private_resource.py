#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2021 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>

import os, sys, subprocess, math, re, zlib, json, struct, argparse
from collections import namedtuple
from collections import OrderedDict

DATA_ALIGNED_SIZE = 2048
META_ALIGNED_SIZE = 512
VERBOSE = False

DATA_SECT_TYPE_DRAM = int("0x41490001",16)
DATA_SECT_TYPE_SYS_UART  = int("0x41490002",16)
DATA_SECT_TYPE_SYS_JTAG  = int("0x41490003",16)
DATA_SECT_TYPE_SYS_UPGMODE  = int("0x41490004",16)
DATA_SECT_TYPE_END  = int("0x4149FFFF",16)

def parse_private_data_cfg(cfgfile):
    """ Load configuration file
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
            slash_start = sline.find("//")
            if slash_start > 0:
                jsonstr += sline[0:slash_start].strip()
            else:
                jsonstr += sline
        # Use OrderedDict is important, we need to iterate FWC in order.
        jsonstr = jsonstr.replace(",}", "}").replace(",]", "]")
        cfg = json.loads(jsonstr, object_pairs_hook=OrderedDict)
    return cfg

def int_to_u32_bytes(n):
    return n.to_bytes(4, byteorder='little', signed=False)

def param_str_to_int(strval):
    val = 0
    if "0x" in strval or "0X" in strval:
        val = int(strval, 16)
    else:
        val = int(strval, 10)
    return val

def param_str_to_u32_bytes(strval):
    return int_to_u32_bytes(param_str_to_int(strval))

"""
struct ddr {
    u32 ddr_type   ;
    u32 ddr_size   ;
    u32 ddr_freq   ;
    u32 ddr_zq     ;
    u32 ddr_odt_en ;
    u32 ddr_para1  ;
    u32 ddr_para2  ;
    u32 ddr_mr0    ;
    u32 ddr_mr1    ;
    u32 ddr_mr2    ;
    u32 ddr_mr3    ;
    u32 ddr_mr4    ;
    u32 ddr_mr5    ;
    u32 ddr_mr6    ;
    u32 ddr_tpr0   ;
    u32 ddr_tpr1   ;
    u32 ddr_tpr2   ;
    u32 ddr_tpr3   ;
    u32 ddr_tpr4   ;
    u32 ddr_tpr5   ;
    u32 ddr_tpr6   ;
    u32 ddr_tpr7   ;
    u32 ddr_tpr8   ;
    u32 ddr_tpr9   ;
    u32 ddr_tpr10  ;
    u32 ddr_tpr11  ;
    u32 ddr_tpr12  ;
    u32 ddr_tpr13  ;
    u32 ddr_tpr14  ;
    u32 ddr_tpr15  ;
    u32 ddr_tpr16  ;
    u32 ddr_tpr17  ;
    u32 ddr_tpr18  ;
};
struct dram_data {
    u32 data_type;
    u32 data_len; // length of rest of this structure
    u32 entry_cnt;
    struct ddr param[entry_cnt];
};

"""
def gen_ddr_init_data(dram):
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_DRAM)
    entry_cnt = len(dram)
    data = int_to_u32_bytes(entry_cnt)
    for entry_name in dram.keys():
        entry = dram[entry_name]
        data += param_str_to_u32_bytes(entry["type"])
        data += param_str_to_u32_bytes(entry["memsize"])
        data += param_str_to_u32_bytes(entry["freq"])
        data += param_str_to_u32_bytes(entry["zq"])
        data += param_str_to_u32_bytes(entry["odt"])
        data += param_str_to_u32_bytes(entry["para1"])
        data += param_str_to_u32_bytes(entry["para2"])
        data += param_str_to_u32_bytes(entry["mr0"])
        data += param_str_to_u32_bytes(entry["mr1"])
        data += param_str_to_u32_bytes(entry["mr2"])
        data += param_str_to_u32_bytes(entry["mr3"])
        data += param_str_to_u32_bytes(entry["mr4"])
        data += param_str_to_u32_bytes(entry["mr5"])
        data += param_str_to_u32_bytes(entry["mr6"])
        data += param_str_to_u32_bytes(entry["tpr0"])
        data += param_str_to_u32_bytes(entry["tpr1"])
        data += param_str_to_u32_bytes(entry["tpr2"])
        data += param_str_to_u32_bytes(entry["tpr3"])
        data += param_str_to_u32_bytes(entry["tpr4"])
        data += param_str_to_u32_bytes(entry["tpr5"])
        data += param_str_to_u32_bytes(entry["tpr6"])
        data += param_str_to_u32_bytes(entry["tpr7"])
        data += param_str_to_u32_bytes(entry["tpr8"])
        data += param_str_to_u32_bytes(entry["tpr9"])
        data += param_str_to_u32_bytes(entry["tpr10"])
        data += param_str_to_u32_bytes(entry["tpr11"])
        data += param_str_to_u32_bytes(entry["tpr12"])
        data += param_str_to_u32_bytes(entry["tpr13"])
        data += param_str_to_u32_bytes(entry["tpr14"])
        data += param_str_to_u32_bytes(entry["tpr15"])
        data += param_str_to_u32_bytes(entry["tpr16"])
        data += param_str_to_u32_bytes(entry["tpr17"])
        data += param_str_to_u32_bytes(entry["tpr18"])
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data

"""
struct system_uart {
    u32 uart_id;
    u32 uart_tx_pin_cfg_reg;
    u32 uart_tx_pin_cfg_val;
    u32 uart_rx_pin_cfg_reg;
    u32 uart_rx_pin_cfg_val;
};
struct system_uart_data {
    u32 data_type;
    u32 data_len; // length of rest of this structure
    struct system_uart param[entry_cnt];
};

"""
def gen_system_uart_data(uart):
    data = param_str_to_u32_bytes(uart["uart_id"])
    data += param_str_to_u32_bytes(uart["uart_tx_pin_cfg_reg"])
    data += param_str_to_u32_bytes(uart["uart_tx_pin_cfg_val"])
    data += param_str_to_u32_bytes(uart["uart_rx_pin_cfg_reg"])
    data += param_str_to_u32_bytes(uart["uart_rx_pin_cfg_val"])
    return data

def gen_system_uart(sys_uart):
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_SYS_UART)
    data = bytes()
    for uarti in sys_uart.keys():
        data += gen_system_uart_data(sys_uart[uarti])
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data

"""
struct system_jtag {
    u32 jtag_id;
    u32 uart_do_pin_cfg_reg;
    u32 uart_do_pin_cfg_val;
    u32 uart_di_pin_cfg_reg;
    u32 uart_di_pin_cfg_val;
    u32 uart_ms_pin_cfg_reg;
    u32 uart_ms_pin_cfg_val;
    u32 uart_ck_pin_cfg_reg;
    u32 uart_ck_pin_cfg_val;
};
struct system_jtag_data {
    u32 data_type;
    u32 data_len; // length of rest of this structure
    u32 jtag_only;
    struct system_jtag param[entry_cnt];
};

"""
def gen_system_jtag_data(jtag):
    data = param_str_to_u32_bytes(jtag["jtag_id"])
    data += param_str_to_u32_bytes(jtag["jtag_do_pin_cfg_reg"])
    data += param_str_to_u32_bytes(jtag["jtag_do_pin_cfg_val"])
    data += param_str_to_u32_bytes(jtag["jtag_di_pin_cfg_reg"])
    data += param_str_to_u32_bytes(jtag["jtag_di_pin_cfg_val"])
    data += param_str_to_u32_bytes(jtag["jtag_ms_pin_cfg_reg"])
    data += param_str_to_u32_bytes(jtag["jtag_ms_pin_cfg_val"])
    data += param_str_to_u32_bytes(jtag["jtag_ck_pin_cfg_reg"])
    data += param_str_to_u32_bytes(jtag["jtag_ck_pin_cfg_val"])
    return data

def gen_system_jtag(sys_jtag):
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_SYS_JTAG)
    data = bytes()
    jtag_only = param_str_to_u32_bytes(sys_jtag["jtag_only"])
    data += jtag_only

    for jtagi in sys_jtag.keys():
        if isinstance(sys_jtag[jtagi], OrderedDict):
            data += gen_system_jtag_data(sys_jtag[jtagi])
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data
"""
struct system_upgmode {
    u32 upgmode_pin_cfg_reg;
    u32 upgmode_pin_cfg_val;
    u32 upgmode_pin_input_reg;
    u32 upgmode_pin_input_msk;
    u32 upgmode_pin_input_val;
    u32 upgmode_pin_pullup_dly;
};
struct system_jtag_data {
    u32 data_type;
    u32 data_len; // length of rest of this structure
    struct system_upgmode;
};
"""
def gen_system_upgmode_data(upgmode):
    data  = param_str_to_u32_bytes(upgmode["upgmode_pin_cfg_reg"])
    data += param_str_to_u32_bytes(upgmode["upgmode_pin_cfg_val"])
    data += param_str_to_u32_bytes(upgmode["upgmode_pin_input_reg"])
    data += param_str_to_u32_bytes(upgmode["upgmode_pin_input_msk"])
    data += param_str_to_u32_bytes(upgmode["upgmode_pin_input_val"])

    # upgmode_pin_pullup_dly is new add element, need to compatible with json
    # without upgmode_pin_pullup_dly
    if "upgmode_pin_pullup_dly" in upgmode:
        data += param_str_to_u32_bytes(upgmode["upgmode_pin_pullup_dly"])
    else:
        data += param_str_to_u32_bytes("500")
    return data
def gen_system_upgmode(sys_upgmode):
    data = bytes()
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_SYS_UPGMODE)
    data += gen_system_upgmode_data(sys_upgmode)
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data

def gen_end_flag():
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_END)
    data_len = int_to_u32_bytes(0)
    return data_type + data_len

def gen_private_data(cfg):
    data = bytes()
    for item in cfg.keys():
        # Currently support to gen ddr init data only.
        if item == "dram":
            data += gen_ddr_init_data(cfg[item])
        if item == "system":
            for sysi in cfg[item].keys():
                if sysi == "upgmode":
                    data += gen_system_upgmode(cfg[item][sysi])
                if sysi == "uart":
                    data += gen_system_uart(cfg[item][sysi])
                if sysi == "jtag":
                    data += gen_system_jtag(cfg[item][sysi])
    data += gen_end_flag()
    return data

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", type=str,
                        help="resource private data configuration file name")
    parser.add_argument("-o", "--output", type=str,
                        help="output file name")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="show detail information")
    args = parser.parse_args()
    if args.config == None:
        print('Error, option --config is required.')
        sys.exit(1)
    if args.output == None:
        args.output = os.path.splitext(args.config)[0] + ".bin"
    if args.verbose:
        VERBOSE = True

    cfg = parse_private_data_cfg(args.config)

    data = gen_private_data(cfg)
    if data != None:
        f = open(args.output, 'wb')
        f.write(data)
        f.flush()
        f.close()

