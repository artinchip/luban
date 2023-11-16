#!/usr/bin/env python3
# Copyright (C) 2023 ArtInChip Technology Co., Ltd
# Wu Dehuang <dehuang.wu@artinchip.com>

# This tool can be used to calculate linked function/symbol's size in ELF

import os
import sys
import argparse

def calc_size(maplines, outfile, outvar, sectname):
    linecnt = len(maplines)
    section = " ." + sectname
    if linecnt != 0:
        # Go to Memory Configuration
        idx = 0
        while True:
            if "Memory Configuration" in maplines[idx]:
                break
            idx = idx + 1

        line_for_file = ""
        size_stat = {}

        while idx < linecnt:
            if maplines[idx].startswith(section):
                line_for_file = maplines[idx].strip()
                if " " not in line_for_file:
                    idx += 1
                    line_for_file = line_for_file + " " + maplines[idx].strip()
                items = line_for_file.split()
                # line_for_file = sectname + "," + ",".join(items)
                line_for_file = "{},{},{},{}".format(sectname, items[0],
                        int(items[2], 16), items[3])
                outvar.write(line_for_file + "\n")
                fname = items[3]
                varsize = int(items[2], 16)
                if fname not in size_stat:
                    size_stat[fname] = varsize
                else:
                    size_stat[fname] = size_stat[fname] + varsize
            idx += 1

        total_size = 0
        for linkedfile in size_stat:
            outfile.write("{},{},{}\n".format(sectname, size_stat[linkedfile], linkedfile))
            total_size += size_stat[linkedfile]

    return total_size

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--map", type=str,
                        help="elf's map file, e.g. d21x.map")
    args = parser.parse_args()
    if args.map == None:
        print('Error, option --map is required.')
        print('e.g.:')
        print('  {} -m d21x.map'.format(sys.argv[0]))
        sys.exit(1)


    mapfile = args.map
    lines = []
    with open(mapfile, "r+")  as fm:
        lines = fm.readlines()

    fvar_name = "{}.var.csv".format(mapfile.replace('map', 'size'))
    ff_name = "{}.csv".format(mapfile.replace('map', 'size'))
    outvar = open(fvar_name, "w+")
    outfile = open(ff_name, "w+")

    outfile.write("Section,Size,File\n")
    outvar.write("Section,Symbol,Size,File\n")

    section = ['text', 'rodata','data']
    total_size = 0
    for s in section:
        total_size += calc_size(lines, outfile, outvar, s)
    outvar.write("all,,{},Total size\n".format(total_size))
    outfile.write("all,{},Total size\n".format(total_size))
    outvar.close()
    outfile.close()
