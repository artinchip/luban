#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2023 ArtInChip Technology Co., Ltd
# Authors: xuan.wen <xuan.wen@artinchip.com>
#

import os, sys, subprocess, math, re, zlib, json, struct, argparse, filecmp
from collections import namedtuple
from collections import OrderedDict

spinand_manufacturer = ["userid", "bbt", "env", "env_r", "falcon", "kernel"]
mmc_manufacturer = ["userid", "uboot", "env", "falcon", "kernel"]
spinor_manufacturer = ["userid", "env", "falcon", "kernel"]

# part + "config_offset"
undef_userid_config_offset = "#ifdef CONFIG_USERID_OFFSET\n    #undef CONFIG_USERID_OFFSET\n#endif\n"
undef_userid_config_size = "#ifdef CONFIG_USERID_SIZE\n    #undef CONFIG_USERID_SIZE\n#endif\n"

userid_config_offset = "#define CONFIG_USERID_OFFSET num"
userid_config_size = "#define CONFIG_USERID_SIZE num"

undef_mmc_uboot_config_offset = "#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR\n    #undef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR\n#endif\n"
mmc_uboot_config_offset = "#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR num"

undef_bbt_config_offset = "#ifdef CONFIG_NAND_BBT_OFFSET\n    #undef CONFIG_NAND_BBT_OFFSET\n#endif\n"
undef_bbt_config_size = "#ifdef CONFIG_NAND_BBT_RANGE\n    #undef CONFIG_NAND_BBT_RANGE\n#endif\n"

bbt_config_offset = "#define CONFIG_NAND_BBT_OFFSET num"
bbt_config_size = "#define CONFIG_NAND_BBT_RANGE num"

undef_env_config_offset  = "#ifdef CONFIG_ENV_OFFSET\n    #undef CONFIG_ENV_OFFSET\n#endif\n"
undef_env_r_config_offset = "#ifdef CONFIG_ENV_OFFSET_REDUND\n    #undef CONFIG_ENV_OFFSET_REDUND\n#endif\n"
env_config_offset = "#define CONFIG_ENV_OFFSET num"
env_r_config_offset = "#define CONFIG_ENV_OFFSET_REDUND num"

# part +  media_type + "config_offset"
undef_falcon_spinand_config_offset  = "#ifdef CONFIG_SYS_SPL_NAND_OFS\n    #undef CONFIG_SYS_SPL_NAND_OFS\n#endif\n"
undef_falcon_spinand_config_size = "#ifdef CONFIG_SYS_SPL_WRITE_SIZE\n    #undef CONFIG_SYS_SPL_WRITE_SIZE\n#endif\n"
undef_kernel_spinand_config_offset = "#ifdef CONFIG_SYS_NAND_SPL_KERNEL_OFFS\n    #undef CONFIG_SYS_NAND_SPL_KERNEL_OFFS\n#endif\n"

falcon_spinand_config_offset = "#define CONFIG_SYS_SPL_NAND_OFS num"
falcon_spinand_config_size = "#define CONFIG_SYS_SPL_WRITE_SIZE num"
kernel_spinand_config_offset = "#define CONFIG_SYS_NAND_SPL_KERNEL_OFFS num"

undef_falcon_spinor_config_offset  = "#ifdef CONFIG_SYS_SPI_ARGS_OFFS\n    #undef CONFIG_SYS_SPI_ARGS_OFFS\n#endif\n"
undef_falcon_spinor_config_size = "#ifdef CONFIG_SYS_SPI_ARGS_SIZE\n    #undef CONFIG_SYS_SPI_ARGS_SIZE\n#endif\n"
undef_kernel_spinor_config_offset = "#ifdef CONFIG_SYS_SPI_KERNEL_OFFS\n    #undef CONFIG_SYS_SPI_KERNEL_OFFS\n#endif\n"

falcon_spinor_config_offset = "#define CONFIG_SYS_SPI_ARGS_OFFS num"
falcon_spinor_config_size = "#define CONFIG_SYS_SPI_ARGS_SIZE num"
kernel_spinor_config_offset = "#define CONFIG_SYS_SPI_KERNEL_OFFS num"

undef_falcon_mmc_config_offset  = "#ifdef CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR\n    #undef CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR\n#endif\n"
undef_falcon_mmc_config_size = "#ifdef CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTORS\n    #undef CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTORS\n#endif\n"
undef_kernel_mmc_config_offset = "#ifdef CONFIG_SYS_MMCSD_RAW_MODE_KERNEL_SECTOR\n    #undef CONFIG_SYS_MMCSD_RAW_MODE_KERNEL_SECTOR\n#endif\n"

falcon_mmc_config_offset = "#define CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR num"
falcon_mmc_config_size = "#define CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTORS num"
kernel_mmc_config_offset = "#define CONFIG_SYS_MMCSD_RAW_MODE_KERNEL_SECTOR num"

def load_part_config(media_type, fp, pt_name, pt_off, pt_size):
    if pt_name == "userid":
        partstr = undef_userid_config_offset
        fp.writelines(partstr)
        partstr = userid_config_offset
        partstr = partstr.replace('num', '%#x\n'%pt_off)
        fp.writelines(partstr)
        partstr = undef_userid_config_size
        fp.writelines(partstr)
        partstr = userid_config_size
        partstr = partstr.replace('num', '%#x\n'%pt_size)
        fp.writelines(partstr)
    if pt_name == "env":
        partstr = undef_env_config_offset
        fp.writelines(partstr)
        partstr = env_config_offset
        partstr = partstr.replace('num', '%#x\n'%pt_off)
        fp.writelines(partstr)
    if pt_name == "env_r":
        partstr = undef_env_r_config_offset
        fp.writelines(partstr)
        partstr = env_r_config_offset
        partstr = partstr.replace('num', '%#x\n'%pt_off)
        fp.writelines(partstr)
    if media_type == "spi-nand":
        if pt_name == "bbt":
            partstr = undef_bbt_config_offset
            fp.writelines(partstr)
            partstr = bbt_config_offset
            partstr = partstr.replace('num', '%#x\n'%pt_off)
            fp.writelines(partstr)
            partstr = undef_bbt_config_size
            fp.writelines(partstr)
            partstr = bbt_config_size
            partstr = partstr.replace('num', '%#x\n'%pt_size)
            fp.writelines(partstr)
# be related to falcon
        if pt_name == "falcon":
            partstr = undef_falcon_spinand_config_offset
            fp.writelines(partstr)
            partstr = falcon_spinand_config_offset
            partstr = partstr.replace('num', '%#x\n'%pt_off)
            fp.writelines(partstr)
            partstr = undef_falcon_spinand_config_size
            fp.writelines(partstr)
            partstr = falcon_spinand_config_size
            partstr = partstr.replace('num', '%#x\n'%pt_size)
            fp.writelines(partstr)
        if pt_name == "kernel":
            partstr = undef_kernel_spinand_config_offset
            fp.writelines(partstr)
            partstr = kernel_spinand_config_offset
            partstr = partstr.replace('num', '%#x\n'%pt_off)
            fp.writelines(partstr)
    if media_type == "mmc":
        if pt_name == "falcon":
            partstr = undef_falcon_mmc_config_offset
            fp.writelines(partstr)
            partstr = falcon_mmc_config_offset
            pt_sector_off = int(pt_off / 512)
            partstr = partstr.replace('num', '%#x\n'%pt_sector_off)
            fp.writelines(partstr)
            partstr = undef_falcon_mmc_config_size
            fp.writelines(partstr)
            partstr = falcon_mmc_config_size
            pt_sector_size = int(pt_size / 512)
            partstr = partstr.replace('num', '%#x\n'%pt_sector_size)
            fp.writelines(partstr)
        if pt_name == "kernel":
            partstr = undef_kernel_mmc_config_offset
            fp.writelines(partstr)
            partstr = kernel_mmc_config_offset
            pt_sector_off = int(pt_off / 512)
            partstr = partstr.replace('num', '%#x\n'%pt_sector_off)
            fp.writelines(partstr)
        if pt_name == "uboot":
            partstr = undef_mmc_uboot_config_offset
            fp.writelines(partstr)
            partstr = mmc_uboot_config_offset
            pt_sector_off = int(pt_off / 512)
            partstr = partstr.replace('num', '%#x\n'%pt_sector_off)
            fp.writelines(partstr)
    if media_type == "spi-nor":
        if pt_name == "falcon":
            partstr = undef_falcon_spinor_config_offset
            fp.writelines(partstr)
            partstr = falcon_spinor_config_offset
            partstr = partstr.replace('num', '%#x\n'%pt_off)
            fp.writelines(partstr)

            partstr = undef_falcon_spinor_config_size
            fp.writelines(partstr)
            partstr = falcon_spinor_config_size
            partstr = partstr.replace('num', '%#x\n'%pt_size)
            fp.writelines(partstr)
        if pt_name == "kernel":
            partstr = undef_kernel_spinor_config_offset
            fp.writelines(partstr)
            partstr = kernel_spinor_config_offset
            partstr = partstr.replace('num', '%#x\n'%pt_off)
            fp.writelines(partstr)

def part_is_in_manufacturer(media_type, part):
    if media_type == "spi-nand":
        manufacturer = spinand_manufacturer
    if media_type == "mmc":
        manufacturer = mmc_manufacturer
    if media_type == "spi-nor":
        manufacturer = spinor_manufacturer
    if part in manufacturer:
        return 0
    else:
        return -1

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
                jsonstr += sline[0:slash_start].strip()
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

def aic_auto_calculate_part_config(cfg, datadir):
    mtd = ""
    ubi = ""
    gpt = ""
    pt_size_str = ""
    pt_name = ""
    pt_off = 0
    pt_size = 0
    total_siz = 0

    print("Building:{}".format(datadir))
    fp = open(datadir, "w+")
    part_str = "#ifndef __ENV_PART_CONFIG_H__\n#define __ENV_PART_CONFIG_H__\n"
    fp.writelines(part_str)
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
                    pt_off += vol_size
            else:
                if part_is_in_manufacturer(media_type, pt_name) == 0:
                    # print(pt_name,'%#x'%pt_off,'%#x'%pt_size)
                    load_part_config(media_type, fp, pt_name, pt_off, pt_size)
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
            if part_is_in_manufacturer(media_type, pt_name) == 0:
                # print(pt_name,'%#x'%pt_off,'%#x'%pt_size)
                load_part_config(media_type, fp, pt_name, pt_off, pt_size)
            pt_off += pt_size
        # parts_mmc will be deleted later, keep it just for old version AiBurn tool
    else:
        print("Not supported media type: {}".format(media_type))
        sys.exit(1)
    part_str = "#endif\n"
    fp.writelines(part_str)
    fp.close()
#   with open(datadir, 'r') as file:
#       print(file.read())
    return part_str

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", type=str,
                        help="image configuration file name")
    parser.add_argument("-d", "--datadir", type=str,
                        help="input image data directory")
    args = parser.parse_args()
    if args.config == None:
        print('Error, option --config is required.')
        sys.exit(1)
    cfg = parse_image_cfg(args.config)

    part_str = aic_auto_calculate_part_config(cfg, args.datadir)
