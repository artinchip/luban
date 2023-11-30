#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2023 ArtInChip Technology Co., Ltd
# Authors: xuan.wen <xuan.wen@artinchip.com>
#

import os, sys, subprocess, math, re, zlib, json, struct, argparse, filecmp
from collections import namedtuple
from collections import OrderedDict

br2_target_rootfs_ubifs_max_size = "BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE"
br2_target_rootfs_ext2_max_size = "BR2_TARGET_ROOTFS_EXT2_SIZE"

br2_target_userfs_ubifs_max_size = [
    "BR2_TARGET_USERFS1_UBIFS_MAX_SIZE",
    "BR2_TARGET_USERFS2_UBIFS_MAX_SIZE",
    "BR2_TARGET_USERFS3_UBIFS_MAX_SIZE"
]

br2_target_userfs_ext4_max_size = [
    "BR2_TARGET_USERFS1_EXT4_SIZE",
    "BR2_TARGET_USERFS2_EXT4_SIZE",
    "BR2_TARGET_USERFS3_EXT4_SIZE"
]

userfs_num = 0
rootfs_num = 0

def load_fs_max_size(media_type, data, pt_name, pt_size):
    global userfs_num
    if media_type == "spi-nand":
        if pt_name == "ubiroot:rootfs":
            partstr = br2_target_rootfs_ubifs_max_size
            if partstr == data:
                print('%#x'%pt_size)
        else:
            partstr = br2_target_userfs_ubifs_max_size[userfs_num]
            if partstr == data:
                print('%#x'%pt_size)
            userfs_num += 1
    if media_type == "mmc":
        if pt_name == "rootfs":
            partstr = br2_target_rootfs_ext2_max_size
            if partstr == data:
                print("{}".format(parse_num_to_text(pt_size)))
        else:
            partstr = br2_target_userfs_ext4_max_size[userfs_num]
            if partstr == data:
                print("{}".format(parse_num_to_text(pt_size)))
            userfs_num += 1

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
            slash_start = sline.find("//")
            if slash_start > 0:
                jsonstr += sline[0:slash_start]
            else:
                jsonstr += sline
        # Use OrderedDict is important, we need to iterate FWC in order.
        jsonstr = jsonstr.replace(",}", "}").replace(",]", "]")
        cfg = json.loads(jsonstr, object_pairs_hook=OrderedDict)
    return cfg

def parse_text_to_num(size_str):
    if "k" in size_str or "K" in size_str:
        numstr = re.sub(r"[^0-9]", "", size_str)
        return (int(numstr) * 1024)
    if "m" in size_str or "M" in size_str:
        numstr = re.sub(r"[^0-9]", "", size_str)
        return (int(numstr) * 1024 * 1024)
    if "g" in size_str or "G" in size_str:
        numstr = re.sub(r"[^0-9]", "", size_str)
        return (int(numstr) * 1024 * 1024 * 1024)
    if "0x" in size_str or "0X" in size_str:
        return int(size_str, 16)
    return 0

# round down: 0x4800000 to 72m
def parse_num_to_text(num):
    mnum = int(num / 1024 /1024)
    mnum_str = '{}m\n'.format(mnum)
    return mnum_str

def aic_auto_calculate_part_config(cfg, data):
    global rootfs_num
    mtd = ""
    ubi = ""
    gpt = ""
    pt_size_str = ""
    pt_name = ""
    pt_off = 0
    pt_size = 0
    total_siz = 0

    media_type = cfg["image"]["info"]["media"]["type"]
    if media_type == "spi-nand" or media_type == "spi-nor":
        total_siz = parse_text_to_num(cfg[media_type]["size"])
        partitions = cfg[media_type]["partitions"]
        if len(partitions) == 0:
            print("Partition table is empty")
            sys.exit(1)
        for part in partitions:
            itemstr = ""
            if "size" not in partitions[part]:
                print("No size value for partition: {}".format(part))
            itemstr += partitions[part]["size"]
            pt_size_str = itemstr
            pt_name = "{}".format(part)
            pt_size = parse_text_to_num(pt_size_str)
            if itemstr == "-":
                pt_size = total_siz - pt_off
            if "ubi" in partitions[part]:
                volumes = partitions[part]["ubi"]
                if len(volumes) == 0:
                    print("Volume of {} is empty".format(part))
                    sys.exit(1)
                for vol in volumes:
                    if "size" not in volumes[vol]:
                        print("No size value for ubi volume: {}".format(vol))
                    vol_size = parse_text_to_num(volumes[vol]["size"])
                    if volumes[vol]["size"] == "-":
                        vol_size = pt_size
                    if "offset" in volumes[vol]:
                        pt_off = parse_text_to_num(volumes[vol]["offset"])
                    pt_name = part + ":" + vol
                    load_fs_max_size(media_type, data, pt_name, vol_size)
                    pt_off += vol_size
            else:
                pt_off += pt_size
    elif media_type == "mmc":
        total_siz = parse_text_to_num(cfg[media_type]["size"])
        partitions = cfg[media_type]["partitions"]
        if len(partitions) == 0:
            print("Partition table is empty")
            sys.exit(1)
        for part in partitions:
            itemstr = ""
            if "size" not in partitions[part]:
                print("No size value for partition: {}".format(part))
            itemstr += partitions[part]["size"]
            pt_size_str = itemstr
            pt_name = "{}".format(part)
            pt_size = parse_text_to_num(pt_size_str)
            if partitions[part]["size"] == "-":
                pt_size = total_siz - pt_off
            if "offset" in partitions[part]:
               pt_off = parse_text_to_num(partitions[part]["offset"])
            if rootfs_num == 1:
                load_fs_max_size(media_type, data, pt_name, pt_size)
            if pt_name == "rootfs":
                load_fs_max_size(media_type, data, pt_name, pt_size)
                rootfs_num = 1
            pt_off += pt_size
    else:
        print("Not supported media type: {}".format(media_type))
        sys.exit(1)
    return

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", type=str,
                        help="image configuration file name")
    parser.add_argument("-d", "--data", type=str,
                        help="input data")
    args = parser.parse_args()
    if args.config == None:
        print('Error, option --config is required.')
        sys.exit(1)
    cfg = parse_image_cfg(args.config)

    aic_auto_calculate_part_config(cfg, args.data)
