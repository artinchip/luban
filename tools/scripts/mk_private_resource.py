#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2021-2024 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>

import os
import sys
import subprocess
import math
import re
import zlib
import json
import struct
import argparse
from collections import namedtuple
from collections import OrderedDict

DATA_ALIGNED_SIZE = 2048
META_ALIGNED_SIZE = 512
VERBOSE = False

DATA_SECT_TYPE_DRAM = int("0x41490001", 16)
DATA_SECT_TYPE_SYS_UART = int("0x41490002", 16)
DATA_SECT_TYPE_SYS_JTAG = int("0x41490003", 16)
DATA_SECT_TYPE_SYS_UPGMODE = int("0x41490004", 16)
DATA_SECT_TYPE_PARTITION = int("0x41490005", 16)
DATA_SECT_TYPE_PSRAM = int("0x41490006", 16)
DATA_SECT_TYPE_END = int("0x4149FFFF", 16)


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


def get_bytes_by_str(cfg, name, defval_str="0"):
    if name in cfg:
        data = param_str_to_u32_bytes(cfg[name])
    else:
        data = param_str_to_u32_bytes(defval_str)
    return data


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
        data += get_bytes_by_str(entry, "type")
        data += get_bytes_by_str(entry, "memsize")
        data += get_bytes_by_str(entry, "freq")
        data += get_bytes_by_str(entry, "zq")
        data += get_bytes_by_str(entry, "odt")
        data += get_bytes_by_str(entry, "para1")
        data += get_bytes_by_str(entry, "para2")
        data += get_bytes_by_str(entry, "mr0")
        data += get_bytes_by_str(entry, "mr1")
        data += get_bytes_by_str(entry, "mr2")
        data += get_bytes_by_str(entry, "mr3")
        data += get_bytes_by_str(entry, "mr4")
        data += get_bytes_by_str(entry, "mr5")
        data += get_bytes_by_str(entry, "mr6")
        data += get_bytes_by_str(entry, "tpr0")
        data += get_bytes_by_str(entry, "tpr1")
        data += get_bytes_by_str(entry, "tpr2")
        data += get_bytes_by_str(entry, "tpr3")
        data += get_bytes_by_str(entry, "tpr4")
        data += get_bytes_by_str(entry, "tpr5")
        data += get_bytes_by_str(entry, "tpr6")
        data += get_bytes_by_str(entry, "tpr7")
        data += get_bytes_by_str(entry, "tpr8")
        data += get_bytes_by_str(entry, "tpr9")
        data += get_bytes_by_str(entry, "tpr10")
        data += get_bytes_by_str(entry, "tpr11")
        data += get_bytes_by_str(entry, "tpr12")
        data += get_bytes_by_str(entry, "tpr13")
        data += get_bytes_by_str(entry, "tpr14")
        data += get_bytes_by_str(entry, "tpr15")
        data += get_bytes_by_str(entry, "tpr16")
        data += get_bytes_by_str(entry, "tpr17")
        data += get_bytes_by_str(entry, "tpr18")
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data


"""
struct psram {
    struct {
        u32 clock;
        u32 cs0_pins;
        u32 cs1_pins;
        u32 xspi_ctl;
        u32 xspi_tcr;
        u32 xspi_cfg;
        u32 xspi_ldo;
        u32 psram_cfg0;
        u32 psram_cfg1;
        u32 xspi_cs0_iocfg1;
        u32 xspi_cs0_iocfg2;
        u32 xspi_cs0_iocfg3;
        u32 xspi_cs0_iocfg4;
        u32 xspi_cs1_iocfg1;
        u32 xspi_cs1_iocfg2;
        u32 xspi_cs1_iocfg3;
        u32 xspi_cs1_iocfg4;
    } common;
    struct {
        u32 proto;
        u32 buf;
    } reset;
    struct {
        u32 proto;
        u32 id;
        u32 buf;
    } getid;
    struct {
        u32 proto0;
        u32 buf0;
        u32 proto1;
        u32 buf1;
        u32 proto2;
        u32 buf2;
        u32 proto3;
        u32 buf3;
    } init;
    struct {
        u32 wr_proto;
        u32 wr_buf;
        u32 rd_proto;
        u32 rd_buf;
    } xip_cfg;
    struct {
        u32 buf0;
        u32 buf1;
        u32 buf2;
        u32 buf3;
        u32 buf4;
        u32 buf5;
        u32 buf6;
        u32 buf7;
        u32 buf8;
        u32 buf9;
    } backup;
};
struct psram_data {
    u32 data_type;
    u32 data_len; // length of rest of this structure
    u32 entry_cnt;
    struct psram param[entry_cnt];
};

"""


def gen_psram_init_data(psram):
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_PSRAM)
    entry_cnt = len(psram)
    data = int_to_u32_bytes(entry_cnt)
    for entry_name in psram.keys():
        entry = psram[entry_name]
        common = entry["common"]
        data += get_bytes_by_str(common, "clock")
        data += get_bytes_by_str(common, "cs0_pins")
        data += get_bytes_by_str(common, "cs1_pins")
        data += get_bytes_by_str(common, "xspi_ctl")
        data += get_bytes_by_str(common, "xspi_tcr")
        data += get_bytes_by_str(common, "xspi_cfg")
        data += get_bytes_by_str(common, "xspi_ldo")
        data += get_bytes_by_str(common, "psram_cfg0")
        data += get_bytes_by_str(common, "psram_cfg1")
        data += get_bytes_by_str(common, "xspi_cs0_iocfg1")
        data += get_bytes_by_str(common, "xspi_cs0_iocfg2")
        data += get_bytes_by_str(common, "xspi_cs0_iocfg3")
        data += get_bytes_by_str(common, "xspi_cs0_iocfg4")
        data += get_bytes_by_str(common, "xspi_cs1_iocfg1")
        data += get_bytes_by_str(common, "xspi_cs1_iocfg2")
        data += get_bytes_by_str(common, "xspi_cs1_iocfg3")
        data += get_bytes_by_str(common, "xspi_cs1_iocfg4")
        reset = entry["reset"]
        data += get_bytes_by_str(reset, "proto", "0xFFFFFFFF")
        data += get_bytes_by_str(reset, "buf",   "0xFFFFFFFF")
        getid = entry["getid"]
        data += get_bytes_by_str(getid, "proto", "0xFFFFFFFF")
        data += get_bytes_by_str(getid, "id",    "0xFFFFFFFF")
        data += get_bytes_by_str(getid, "buf",   "0xFFFFFFFF")
        init = entry["init"]
        data += get_bytes_by_str(init, "proto0", "0xFFFFFFFF")
        data += get_bytes_by_str(init, "buf0",   "0xFFFFFFFF")
        data += get_bytes_by_str(init, "proto1", "0xFFFFFFFF")
        data += get_bytes_by_str(init, "buf1",   "0xFFFFFFFF")
        data += get_bytes_by_str(init, "proto2", "0xFFFFFFFF")
        data += get_bytes_by_str(init, "buf2",   "0xFFFFFFFF")
        data += get_bytes_by_str(init, "proto3", "0xFFFFFFFF")
        data += get_bytes_by_str(init, "buf3",   "0xFFFFFFFF")
        xip_cfg = entry["xip_cfg"]
        data += get_bytes_by_str(xip_cfg, "wr_proto", "0xFFFFFFFF")
        data += get_bytes_by_str(xip_cfg, "wr_buf",   "0xFFFFFFFF")
        data += get_bytes_by_str(xip_cfg, "rd_proto", "0xFFFFFFFF")
        data += get_bytes_by_str(xip_cfg, "rd_buf",   "0xFFFFFFFF")
        backup = entry["backup"]
        data += get_bytes_by_str(backup, "buf0", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf1", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf2", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf3", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf4", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf5", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf6", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf7", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf8", "0xFFFFFFFF")
        data += get_bytes_by_str(backup, "buf9", "0xFFFFFFFF")

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
    data = get_bytes_by_str(uart, "uart_id")
    data += get_bytes_by_str(uart, "uart_tx_pin_cfg_reg")
    data += get_bytes_by_str(uart, "uart_tx_pin_cfg_val")
    data += get_bytes_by_str(uart, "uart_rx_pin_cfg_reg")
    data += get_bytes_by_str(uart, "uart_rx_pin_cfg_val")
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
    data = get_bytes_by_str(jtag, "jtag_id")
    data += get_bytes_by_str(jtag, "jtag_do_pin_cfg_reg")
    data += get_bytes_by_str(jtag, "jtag_do_pin_cfg_val")
    data += get_bytes_by_str(jtag, "jtag_di_pin_cfg_reg")
    data += get_bytes_by_str(jtag, "jtag_di_pin_cfg_val")
    data += get_bytes_by_str(jtag, "jtag_ms_pin_cfg_reg")
    data += get_bytes_by_str(jtag, "jtag_ms_pin_cfg_val")
    data += get_bytes_by_str(jtag, "jtag_ck_pin_cfg_reg")
    data += get_bytes_by_str(jtag, "jtag_ck_pin_cfg_val")
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
    data = get_bytes_by_str(upgmode, "upgmode_pin_cfg_reg")
    data += get_bytes_by_str(upgmode, "upgmode_pin_cfg_val")
    data += get_bytes_by_str(upgmode, "upgmode_pin_input_reg")
    data += get_bytes_by_str(upgmode, "upgmode_pin_input_msk")
    data += get_bytes_by_str(upgmode, "upgmode_pin_input_val")
    data += get_bytes_by_str(upgmode, "upgmode_pin_pullup_dly", "500")
    return data


def gen_system_upgmode(sys_upgmode):
    data = bytes()
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_SYS_UPGMODE)
    data += gen_system_upgmode_data(sys_upgmode)
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data


"""
struct system_jtag_data {
    u32 data_type;
    u32 data_len; // length of rest of this structure
    u8  part_str[];
};
"""


def gen_bytes_of_part_str(parts):
    part_str = ""
    if "type" not in parts:
        return part_str

    part_types = parts["type"]
    for t in part_types:
        if t in parts:
            part_str += "{}={};".format(t, parts[t])
    return bytes(part_str, encoding="utf-8")


def gen_partition_table(parts):
    data = bytes()
    pad_len = 4
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_PARTITION)
    data += gen_bytes_of_part_str(parts)
    if len(data) % 4:
        pad_len = 4 - len(data) % 4
    data += bytearray(pad_len)
    data_len = int_to_u32_bytes(len(data))
    return data_type + data_len + data


def gen_end_flag():
    data_type = int_to_u32_bytes(DATA_SECT_TYPE_END)
    data_len = int_to_u32_bytes(0)
    return data_type + data_len


def gen_private_data(cfg):
    data = bytes()
    for item in cfg.keys():
        if item == "dram":
            data += gen_ddr_init_data(cfg[item])
        if item == "psram":
            data += gen_psram_init_data(cfg[item])
        if item == "partitions":
            data += gen_partition_table(cfg[item])
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


def list_of_strings(arg):
    return arg.split(',')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", type=str,
                        help="resource private data configuration file name")
    parser.add_argument("-l", "--config_list", type=list_of_strings,
                        help="resource private data configuration file list, concat with commas")
    parser.add_argument("-o", "--output", type=str,
                        help="output file name")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="show detail information")
    args = parser.parse_args()
    if args.config is None and args.config_list is None:
        print('Error, option --config or --config_list is required.')
        sys.exit(1)
    if args.output is None:
        args.output = os.path.splitext(args.config)[0] + ".bin"
    if args.verbose:
        VERBOSE = True

    cfg = {}
    if args.config is not None:
        ncfg = parse_private_data_cfg(args.config)
        cfg.update(ncfg)
    if args.config_list is not None:
        for c in args.config_list:
            ncfg = parse_private_data_cfg(c)
            cfg.update(ncfg)

    data = gen_private_data(cfg)
    if data is not None:
        f = open(args.output, 'wb')
        f.write(data)
        f.flush()
        f.close()
