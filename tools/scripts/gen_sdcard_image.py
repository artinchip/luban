#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+

# Copyright (C) 2020 ArtInChip
#
# Creates binary images from ArtInChip partition table for SDCard boot.
#

import os
import sys
import argparse
import subprocess

class PartitionEntry:
    def __init__(self):
        self.part = '';
        self.name = '';
        self.offset = 0;
        self.storage = '';
        self.type = '';
        self.file = '';
        self.size = 0;

def Trans2PartEntry(line):
    """ Translate line content to PartitionEntry
    Args:
        line: Partition table line content
    """
    ent = PartitionEntry()
    params = line.split(' ')
    i = len(params)

    while i > 0:
        if params[i - 1] == '':
            del params[i - 1]
        i -= 1
    if len(params) != 6:
        print('Error, Partition entry parameter is not enough.')
        return None
    ent.part = params[0].upper()
    ent.name = params[1]
    ent.offset = int(params[2], 16)
    ent.storage = params[3].upper()
    ent.type = params[4].upper()
    ent.file = params[5]

    return ent

def ParsePartitionTable(ptable):
    """ Parse partition table, and return PartitionEntry list
    Args:
        ptable: Partition talbe file name
    """
    plist = []
    with open(ptable, 'r') as f:
        lines = f.readlines()
        for ln in lines:
            ln = ln.expandtabs().strip().replace('\n', '').replace('\r', '')
            if ln.startswith('#') or len(ln) == 0:
                continue
            ent = Trans2PartEntry(ln)
            plist.append(ent)
    return plist 

def GetSizeWithUnit(siz):
    """Translate size in appropriate unit:B/K/M/G
    Args:
        siz: integer size value
    Return:
        String of size with unit.
    """
    K = 1024
    M = 1024 * K
    G = 1024 * M
    ret = ''
    if siz >= G:
        ret = '{:.2f} GB'.format(float(siz)/G)
    elif siz >= M:
        ret = '{:.2f} MB'.format(float(siz)/M)
    elif siz >= K:
        ret = '{:.2f} KB'.format(float(siz)/K)
    else:
        ret = '{} B'.format(siz)
    return ret

def GetTotalSize(args):
    """Args:
        args: arguments Namespace object
    """
    totalsiz = 0
    if args.size.isdigit():
        totalsiz = int(args.size)
    else:
        strsiz = args.size.upper()
        if strsiz[-1] == 'B':
            totalsiz = int(strsiz[0:-1])
        elif strsiz[-1] == 'K':
            totalsiz = int(strsiz[0:-1]) * 1024
        elif strsiz[-1] == 'M':
            totalsiz = int(strsiz[0:-1]) * 1024 * 1024
        elif strsiz[-1] == 'G':
            totalsiz = int(strsiz[0:-1]) * 1024 * 1024 * 1024
        else:
            print('Error, option --size {} is invalid.'.format(args.size))
            return -1
    return totalsiz

def CheckAndSetPartitionSize(total, plist, v=False):
    """ Check Partition size valid or not.
    Args:
        total: Total image size
        plist: Partition list
        v: Verbose
    Return:
        True or False
    """
    ssiz = 512
    psiz = 0
    pcnt = len(plist)
    for i in range(0, pcnt):
        if (i + 1) != pcnt:
            psiz = plist[i+1].offset - plist[i].offset
            if psiz <= 0:
                print('Size of {} is {} invalid.'.format(plist[i].name, psiz))
                return False
            if (psiz % ssiz) != 0:
                print('Size of {} is {} invalid, should be 512 bytes alignment.'.format(plist[i].name, psiz))
                return False
            plist[i].size = psiz
            if v:
                print('Partition {}, name {}, size {}'.format(i, plist[i].name, psiz))
        else:
            psiz = total - plist[i].offset
            if psiz <= (34 * ssiz):
		# Reserve 34 blocks for GPT header at the end of the image.
                print('Size of {} is {}, invalid.'.format(plist[i].name, psiz))
                return False
            if (psiz % ssiz) != 0:
                print('Size of {} is {} invalid, should be 512 bytes alignment.'.format(plist[i].name, psiz))
                return False
            plist[i].size = psiz - (33 * ssiz)
            if v:
                print('Partition {}, name {}, size {}'.format(i, plist[i].name, psiz))
    return True

def ValidatePartitionForSD(plist):
    """ Validate Partition setting for sd card is valid or not.
    Args:
        plist: Partition list
    Return:
        True or False
    """
    for ent in plist:
        if ent.part != 'GPT':
            return False
        elif ent.storage != 'SDCARD':
            return False
        elif ent.type not in ['RAW', 'FAT32', 'EXT4']:
            return False
    return True

def RunCommand(cmd, dumpmsg=False, verbose=False):
    """Execute command
    Args:
        cmd: cmd line content
        dumpmsg: display message during cmd execute
    Return:
        0 if success, -1 if failure.
    """
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=-1)
    ret = p.wait()
    if ret != 0:
        print('Run commmand error:')
        print('    ' + cmd)
        print('stdout: {}\nstderr: {}'.format(p.stdout.read(), p.stderr.read()))
        return -1
    if verbose:
        print(cmd)
    if dumpmsg or verbose:
        print(p.stdout.read())
    if verbose:
        print(p.stderr.read())

    return 0

def GenSdcardImage(args):
    """Generate Image for SDcard boot
    Args:
        args: arguments Namespace object
    """

    # Sector size 512 bytes
    ssiz = 512
    totalsiz = GetTotalSize(args)
    if totalsiz < 0:
        return -1
    if totalsiz % ssiz != 0:
        print('Error, Image size should be 512 bytes alignment')
        return -1

    plist = ParsePartitionTable(args.table)
    if CheckAndSetPartitionSize(totalsiz, plist, args.verbose) != True:
        print('Partition size validate failed.')
        return -1
    if ValidatePartitionForSD(plist) != True:
        print('Partition validate for SD Card failed.')
        return -1

    # Create empty image
    print('Creating image file...')
    cmd = 'truncate -s {} {}'.format(totalsiz, args.output)
    ret = RunCommand(cmd, verbose=args.verbose)
    if ret != 0:
        return ret

    # Make GPT format header
    print('Creating GPT...')
    cmd = 'sgdisk -a 1 -og {}'.format(args.output)
    ret = RunCommand(cmd, verbose=args.verbose)
    if ret != 0:
        return ret

    part_num = 1
    for ent in plist:
        print('    making partition {} ...'.format(ent.name))
        scnt = ent.size/ssiz
        start_sector = ent.offset/ssiz
        end_sector = start_sector + scnt - 1
        if ent.type == 'FAT32':
            type_code = '0700' # Microsoft basic data
        elif ent.type == 'EXT4':
            type_code = '8300' # Linux filesystem
        else:
            type_code = '8301' # Linux reserved
        # Create new partition
        cmd = 'sgdisk -a 1 -n {}:{}:{} -c {}:{} -t {}:{} {}'.format(part_num,
                start_sector, end_sector, part_num, ent.name, part_num, type_code, args.output)
        ret = RunCommand(cmd, verbose=args.verbose)
        if ret != 0:
            return ret
        part_num += 1
        # Copy binary file to partition, skip this if it is set to none
        dfile = ent.file
        if dfile.upper() == 'NONE':
            continue
        cmd = 'dd if={} of={} bs={} seek={} conv=notrunc'.format(args.datadir + ent.file,
                args.output, ssiz, start_sector)
        ret = RunCommand(cmd, verbose=args.verbose)
        if ret != 0:
            return ret

    print('Image created.\n')
    cmd = 'sgdisk -p {}'.format(args.output)
    RunCommand(cmd, dumpmsg=True, verbose=args.verbose)

    return 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-t", "--table", type=str, help="partition table file name")
    parser.add_argument("-s", "--size", type=str,
                        help="whole image size, if end withcharacter B/K/M/G,"
                        "means the uint is Byte/Kilobytes/Megabytes/Gigabytes, default is B.")
    parser.add_argument("-d", "--datadir", type=str, help="input image data directory")
    parser.add_argument("-o", "--output", type=str, help="output image file name")
    parser.add_argument("-v", "--verbose", action='store_true', help="show detail information")
    args = parser.parse_args()
    if args.size == None:
        print('Error, option --size is required.')
        sys.exit(1)
    if args.table == None:
        print('Error, option --table is required.')
        sys.exit(1)
    if args.output == None:
        print('Error, option --output is required.')
        sys.exit(1)
    # If user not specified data directory, use current directory as default
    if args.datadir == None:
        args.datadir = './'
    if args.datadir.endswith('/') == False:
        args.datadir = args.datadir + '/'

    GenSdcardImage(args)
