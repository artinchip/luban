#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

# Copyright (C) 2023 ArtInChip Technology Co., Ltd
# Wu Dehuang <dehuang.wu@artinchip.com>

# This tool can be used to calculate linked function/symbol's size in ELF

import os
import sys
import argparse


TYPE_SECTION_TOTAL = 0
TYPE_SECTION_FILE = 1
TYPE_SECTION_FUNC = 2


def parse_column_info(line, line_next, col_fmt):
    info = {}
    info['type'] = -1
    info['break'] = False
    info['line'] = line.replace('\r', '').replace('\n', '')

    line_size = len(line)
    line_nsize = len(line_next)

    if line_size < col_fmt[3] and line_nsize < col_fmt[3]:
        return info

    if line_size >= col_fmt[3]:
        # All information in one line, not break in two lines
        sect = line[col_fmt[0]:col_fmt[1]].strip()
        addr = line[col_fmt[1]:col_fmt[2]].strip().lower()
        size = line[col_fmt[2]:col_fmt[3]].strip().lower()
        attr = line[col_fmt[3]:].strip()

        if addr.startswith('0x') and size.startswith('0x'):
            if len(sect) <= 0:
                # unknown line
                return info
            else:
                info['sect'] = sect
                info['addr'] = addr
                info['size'] = size
                info['attr'] = attr
                if line[0].isspace():
                    info['type'] = TYPE_SECTION_FILE
                else:
                    info['type'] = TYPE_SECTION_TOTAL
        elif len(sect) == 0 and len(size) == 0 and addr.startswith('0x'):
            info['sect'] = sect
            info['addr'] = addr
            info['size'] = size
            info['attr'] = attr
            info['type'] = TYPE_SECTION_FUNC
    else:
        # Information maybe break in two lines, need to check
        items = line.strip().split()
        if len(items) > 1:
            # Unknown line
            return info
        sect2 = line_next[col_fmt[0]:col_fmt[1]].strip()
        if len(sect2) > 0:
            # Not one line break into two case
            return info
        sect = line.strip()
        addr = line_next[col_fmt[1]:col_fmt[2]].strip().lower()
        size = line_next[col_fmt[2]:col_fmt[3]].strip().lower()
        attr = line_next[col_fmt[3]:].strip()
        if addr.startswith('0x') and size.startswith('0x'):
            info['sect'] = sect
            info['addr'] = addr
            info['size'] = size
            info['attr'] = attr
            info['break'] = True
            info['line'] += line_next.replace('\r', '').replace('\n', '')
            if line[0].isspace():
                info['type'] = TYPE_SECTION_FILE
            else:
                info['type'] = TYPE_SECTION_TOTAL
    return info


def get_linked_size(maplines):
    linecnt = len(maplines)
    sect_total = False
    if linecnt == 0:
        return None

    # Goto Linker script and memory map
    idx = 0
    while True:
        if idx >= linecnt:
            print('Map file not include Linker script and memory map')
            return None
        if 'Linker script and memory map' in maplines[idx]:
            break
        idx = idx + 1
    # Find first section
    line = ''
    c1_addr = 'not found'
    c2_len = 'not found'

    while True:
        if idx >= linecnt:
            print('Cannot find the section start in map file')
            return None
        line = maplines[idx]
        if line[0].isspace() is False:
            cols = line.split()
            if len(cols) < 3:
                # Not section start
                idx = idx + 1
                continue
            c1_addr = cols[1].lower()
            c2_len = cols[2].lower()
            if c1_addr.startswith('0x') and c2_len.startswith('0x'):
                # Found the first section
                break
        idx = idx + 1

    # Column format
    c0_start = 0
    c1_start = line.find(c1_addr)
    if c1_start <= 0:
        print('Parse column format error')
    c2_start = c1_start + len(c1_addr)
    line_left = line[c2_start:]
    c3_start = line_left.find(c2_len)
    if c3_start <= 0:
        print('Parse column format error2')
    c3_start += c2_start
    c3_start += len(c2_len)
    cols = (c0_start, c1_start, c2_start, c3_start)

    cur_sect = 'unknown'
    stat = {}
    while True:
        if (idx + 1) >= linecnt:
            break
        line = maplines[idx]
        line_next = maplines[idx + 1]
        info = parse_column_info(line, line_next, cols)
        if info['type'] == TYPE_SECTION_TOTAL:
            cur_sect = info['sect'].strip()
            if cur_sect not in stat:
                stat[cur_sect] = {}
                stat[cur_sect]['addr'] = info['addr']
                stat[cur_sect]['size'] = int(info['size'], 16)
                stat[cur_sect]['detail'] = {}
            else:
                print('It make me confused, one section should not begin twice.')
                sys.exit(1)
            if info['break']:
                idx += 1
        elif info['type'] == TYPE_SECTION_FILE:
            linkedfile = info['attr'].strip()
            if len(linkedfile) == 0:
                linkedfile = 'unknown/' + info['sect'].strip()
            else:
                abspath = os.path.abspath('/' + linkedfile)
                linkedfile = abspath[1:]
            newsize = int(info['size'], 16)
            if linkedfile not in stat[cur_sect]['detail']:
                stat[cur_sect]['detail'][linkedfile] = {}
                stat[cur_sect]['detail'][linkedfile]['size'] = newsize
                stat[cur_sect]['detail'][linkedfile]['isfile'] = True
            else:
                stat[cur_sect]['detail'][linkedfile]['size'] += newsize

            # stat by directory
            dirname = os.path.dirname(linkedfile)
            while len(dirname) > 0:
                if dirname not in stat[cur_sect]['detail']:
                    stat[cur_sect]['detail'][dirname] = {}
                    stat[cur_sect]['detail'][dirname]['size'] = newsize
                    stat[cur_sect]['detail'][dirname]['isfile'] = False
                else:
                    stat[cur_sect]['detail'][dirname]['size'] += newsize
                dirname = os.path.dirname(dirname)
                if dirname == '/':
                    break
            if info['break']:
                idx += 1
        else:
            if info['break']:
                idx += 1
        idx += 1
    return stat


def check_is_skip_section(s):
    skip_list = ['.note', '.debug', '.comment']
    skip = False
    for sk in skip_list:
        if s.startswith(sk):
            skip = True
            break
    return skip


def gen_csv_summary(csv_sm, stat):
    f_by_sm = open(csv_sm, 'w+')
    f_by_sm.write('Section,Size,Unused\n')

    sects = stat.keys()
    total = 0
    total_u = 0
    for s in sects:
        skip = check_is_skip_section(s)
        if skip is False and stat[s]['size'] > 0:
            unused = 0
            if 'unknown' in stat[s]['detail']:
                unused = stat[s]['detail']['unknown']['size']
            f_by_sm.write('{},{},{}\n'.format(s, stat[s]['size'], unused))
            total += stat[s]['size']
            total_u += unused
    f_by_sm.write('Total,{},{}\n'.format(total, total_u))
    f_by_sm.close()


def gen_csv_detail(filename, stat, dir_only):
    f_detail = open(filename, 'w+')

    # Generate title/header
    f_detail.write('Folder/File,Summary')
    sects = stat.keys()
    for s in sects:
        skip = check_is_skip_section(s)
        if skip is False and stat[s]['size'] > 0:
            f_detail.write(',{}'.format(s))
    f_detail.write('\n')

    total = 0
    for s in sects:
        skip = check_is_skip_section(s)
        if skip is False and stat[s]['size'] > 0:
            total += stat[s]['size']

    f_detail.write('Total,{}'.format(total))
    for s in sects:
        skip = check_is_skip_section(s)
        if skip is False and stat[s]['size'] > 0:
            f_detail.write(',{}'.format(stat[s]['size']))
    f_detail.write('\n')

    # By Folder/File
    # Get all Folder/File first
    items = []
    for s in sects:
        skip = check_is_skip_section(s)
        if skip is False and stat[s]['size'] > 0:
            keys = stat[s]['detail']
            [items.append(x) for x in keys if x not in items]

    items.sort()
    for i in items:
        sumval = 0
        vals = []
        isfile = False
        for s in sects:
            skip = check_is_skip_section(s)
            if skip is False and stat[s]['size'] > 0:
                itemsize = 0
                if i in stat[s]['detail']:
                    itemsize = stat[s]['detail'][i]['size']
                    isfile = stat[s]['detail'][i]['isfile']
                sumval += itemsize
                vals.append(itemsize)
        if dir_only and isfile:
            continue
        f_detail.write('{}'.format(i))
        f_detail.write(',{}'.format(sumval))
        for v in vals:
            f_detail.write(',{}'.format(v))
        f_detail.write('\n')
    f_detail.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-m', '--map', type=str,
                        help='elf\'s map file, e.g. d21x.map')
    args = parser.parse_args()
    if args.map is None:
        print('Error, option --map is required.')
        print('e.g.:')
        print('  {} -m d21x.map'.format(sys.argv[0]))
        sys.exit(1)

    mapfile = args.map
    lines = []
    with open(mapfile, 'r+') as fm:
        lines = fm.readlines()

    stat = get_linked_size(lines)

    csv_sm = '{}.csv'.format(mapfile.replace('map', 'summary'))
    gen_csv_summary(csv_sm, stat)
    csv_detail = '{}.csv'.format(mapfile.replace('map', 'detail'))
    gen_csv_detail(csv_detail, stat, False)
    csv_detail = '{}.csv'.format(mapfile.replace('map', 'dironly'))
    gen_csv_detail(csv_detail, stat, True)
