#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+

# Copyright (C) 2023 ArtInChip

import os, sys, argparse, subprocess

UBOOT_DIR = 'source/uboot-2021.10/'
LINUX_DIR = 'source/linux-5.10/'

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

def RunShellCommand(cmd):
    ret = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if ret.returncode:
        print(indent_str('Failed to run shell command:' + cmd, 1))
        sys.exit(1)
    return str(ret.stdout, encoding='utf-8')

def indent_str(text, indent):
    itext = text
    while indent > 0:
        itext = '\t' + itext
        indent -= 1
    return itext

def GetReferenceBoardDefconfig(targetcfg):
    cmd = 'find {} -maxdepth 1 -type f -name "*_defconfig"'.format(targetcfg)
    defconfigs = RunShellCommand(cmd)
    return defconfigs.split()

def GetChipNameOfDefconfig(dotcfg):
    f = open(dotcfg, 'r')
    lines = f.readlines()
    f.close()

    name = None
    for ln in lines:
        if 'LUBAN_CHIP_NAME' in ln:
            name = ln.replace('LUBAN_CHIP_NAME=', '').replace('"', '').strip()
            break
    return name

def GetChipOptionOfDefconfig(dotcfg, chip):
    f = open(dotcfg, 'r')
    lines = f.readlines()
    f.close()

    name = None
    for ln in lines:
        if 'LUBAN_CHIP_{}'.format(chip.upper()) in ln and '=y' in ln:
            name = ln.replace('=y', '').strip()
            break
    return name

def GetBoardNameOfDefconfig(dotcfg):
    f = open(dotcfg, 'r')
    lines = f.readlines()
    f.close()
    name = None
    for ln in lines:
        if 'LUBAN_BOARD_NAME' in ln:
            name = ln.replace('LUBAN_BOARD_NAME=', '').replace('"', '').strip()
            break
    return name

def GetBoardOptionNameOfDefconfig(dotcfg, board_name):
    f = open(dotcfg, 'r')
    lines = f.readlines()
    f.close()
    opt_name = None
    for ln in lines:
        if 'LUBAN_BOARD_' in ln and board_name.upper() in ln and '=y' in ln:
            opt_name = ln.replace('=y', '').strip()
            break
    return opt_name

def GetArchOfRefDefconfig(dotcfg):
    f = open(dotcfg, 'r')
    lines = f.readlines()
    f.close()
    name = None
    for ln in lines:
        if 'BR2_ARCH=' in ln:
            name = ln.replace('BR2_ARCH=', '').replace('"', '').strip()
            break
    return name

def SelectReferenceDefconfig(defconfigs, dotconfigs):
    reflist = []
    for cfg in defconfigs:
        for dotcfg in dotconfigs:
            if os.path.basename(cfg) in dotcfg:
                break
        reflist.append(os.path.basename(cfg))
    if len(reflist) == 0:
        print(indent_str('Error, cannot find reference defconfig', 1))
        sys.exit(1)
    print(indent_str('Reference defconfig:(Delete one board base on selected defconfig)', 1))
    num = 1
    for ref in reflist:
        print(indent_str('{}: {}'.format(num, ref), 2))
        num += 1
    inputstr = input(indent_str('Select reference defconfig for detele board(number): ', 1)).strip()
    if inputstr.isdigit() == False:
        print(indent_str('Error, please input number'), 1)
        sys.exit(1)
    num = int(inputstr)
    if num not in range(1, len(reflist) + 1):
        print(indent_str('Error, input number not in list', 1))
        sys.exit(1)

    selected = reflist[num - 1]
    print(indent_str(selected + '\n', 2))
    return selected

def GetBoardNameForConfig(name):
    return name.replace('\t', '_').replace(' ', '_')

def GenDotConfig(args, cfg_list):
    root_cfg = args.topdir + 'package/Config.in'
    dotconfigs = []
    tmpdir = os.path.dirname(args.conf) + '/tmp/'
    cmd = 'rm -rf {} && mkdir -p {}'.format(tmpdir, tmpdir)
    RunShellCommand(cmd)
    for cfg in cfg_list:
        dotcfg = tmpdir + os.path.basename(cfg) + '.config'
        dotconfigs.append(dotcfg)
        cmd = 'HOSTARCH={} BR2_CONFIG={} BASE_DIR={} BR2_DEFCONFIG={} {} --defconfig={} {}'.format(args.arch, dotcfg, args.outdir, cfg, args.conf, cfg, root_cfg)
        RunShellCommand(cmd)
    return dotconfigs

def DeleteOneBoardDirectory(topdir, refcfg, dotconfigs):

    for ref_dotcfg in dotconfigs:
        if refcfg in ref_dotcfg:
            break

    refboardname = GetBoardNameOfDefconfig(ref_dotcfg)
    refchipname = GetChipNameOfDefconfig(ref_dotcfg)
    refboard_dir = topdir + 'target/{}/{}'.format(refchipname, refboardname)

    # copy new board directory
    cmd = 'rm -rdf {}'.format(refboard_dir)
    RunShellCommand(cmd)
    print(indent_str('Deleted: ' + refboard_dir.replace(topdir, ''), 1))

def DeleteOneDefconfig(topdir, refcfg, dotconfigs):
    global UBOOT_DIR
    global LINUX_DIR

    for ref_dotcfg in dotconfigs:
        if refcfg in ref_dotcfg:
            break

    arch = GetArchOfRefDefconfig(ref_dotcfg)
    refboardname = GetBoardNameOfDefconfig(ref_dotcfg)
    refboard_opt = GetBoardOptionNameOfDefconfig(ref_dotcfg, refboardname)
    refchipname = GetChipNameOfDefconfig(ref_dotcfg)

    target_cfgdir = topdir + 'target/configs/'
    f = open(target_cfgdir + refcfg, 'r')
    lines = f.readlines()
    f.close()
    # Create U-Boot/Kernel/Busybox defconfig for new board
    ref_uboot_cfg = ''
    ref_linux_cfg = ''
    ref_busybox_cfg = ''
    for ln in lines:
        if 'BR2_TARGET_UBOOT_BOARD_DEFCONFIG' in ln:
            ref_uboot_cfg = ln.replace('BR2_TARGET_UBOOT_BOARD_DEFCONFIG', '').replace('=', '').replace('"', '').strip() + '_defconfig'
        if 'BR2_LINUX_KERNEL_DEFCONFIG' in ln:
            ref_linux_cfg = ln.replace('BR2_LINUX_KERNEL_DEFCONFIG', '').replace('=', '').replace('"', '').strip() + '_defconfig'
        if 'BR2_PACKAGE_BUSYBOX_CONFIG' in ln:
            ref_busybox_cfg = ln.replace('BR2_PACKAGE_BUSYBOX_CONFIG', '').replace('=', '').replace('"', '').strip()

    # Delete defconfig for u-boot
    refcfg_path = '{}{}configs/{}'.format(topdir, UBOOT_DIR, ref_uboot_cfg)
    cmd = 'rm {}'.format(refcfg_path)
    RunShellCommand(cmd)
    print(indent_str('deleted: ' + refcfg_path.replace(topdir, ''), 1))

    # Delete defconfig for linux kernel
    if arch == 'riscv64':
        arch = 'riscv'
    refcfg_path = '{}{}arch/{}/configs/{}'.format(topdir, LINUX_DIR, arch, ref_linux_cfg)
    cmd = 'rm {}'.format(refcfg_path)
    RunShellCommand(cmd)
    print(indent_str('Deleted: ' + refcfg_path.replace(topdir, ''), 1))

    # Delete config for busybox
    refcfg_path = '{}{}'.format(topdir, ref_busybox_cfg)
    cmd = 'rm {}'.format(refcfg_path)
    RunShellCommand(cmd)
    print(indent_str('Deleted: ' + refcfg_path.replace(topdir, ''), 1))

    # Delete SDK defconfig
    refcfg_path = '{}{}'.format(target_cfgdir, refcfg)
    cmd = 'rm {}'.format(refcfg_path)
    RunShellCommand(cmd)
    print(indent_str('Deleted: ' + refcfg_path.replace(topdir, ''), 1))

    # Add new board to Config.in
    cfg_in = '{}target/{}/Config.in'.format(topdir, refchipname)
    f = open(cfg_in, 'r')
    lines = f.readlines()
    f.close()

    board_option = 'LUBAN_BOARD_{}_{}'.format(refchipname.upper(), refboardname.upper())
    f = open(cfg_in, 'w')

    chip_option = GetChipOptionOfDefconfig(ref_dotcfg, refchipname)
    delete_depends = False 
    for ln in lines:
        if 'config' in ln and board_option in ln:
            delete_depends = True
            continue
        if 'bool' in ln and refboardname in ln:
            continue
        if delete_depends and 'depends on' in ln and chip_option in ln:
            delete_depends = False 
            continue
        if 'default' in ln and board_option in ln:
            continue
        f.write(ln)
    f.close()
    print(indent_str('Updated: ' + cfg_in.replace(topdir, ''), 1))

def DeleteOneBoard(args):
    target_cfgdir = args.topdir + 'target/configs/'
    defconfigs = GetReferenceBoardDefconfig(target_cfgdir)

    defconfigs.sort()
    if len(defconfigs) == 0:
        print(indent_str("Error: no defconfig is found.", 1));
        sys.exit(1)
    dotconfigs = GenDotConfig(args, defconfigs)

    refcfg = SelectReferenceDefconfig(defconfigs, dotconfigs)

    DeleteOneBoardDirectory(args.topdir, refcfg, dotconfigs)
    DeleteOneDefconfig(args.topdir, refcfg, dotconfigs)

    return 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--conf", type=str, help="conf executable")
    parser.add_argument("-t", "--topdir", type=str, help="Top root directory")
    parser.add_argument("-o", "--outdir", type=str, help="Output root directory")
    args = parser.parse_args()

    if args.conf == None:
        print(indent_str('Error, please provide path of application "conf"', 1))
        sys.exit(1)
    if os.path.exists(args.conf) == False:
        print(indent_str('Error, "conf" is not exist', 1))
        sys.exit(1)

    if args.topdir == None:
        print(indent_str('Error, please provide path of top directory', 1))
        sys.exit(1)

    args.topdir = os.path.abspath(args.topdir)
    if args.topdir.endswith('/') == False:
        args.topdir = args.topdir + '/'
    if args.outdir == None:
        args.outdir = args.topdir
    else:
        args.outdir = os.path.abspath(args.outdir)
    if args.outdir.endswith('/') == False:
        args.outdir = args.outdir + '/'
    cmd = 'arch'
    args.arch = RunShellCommand(cmd)
    DeleteOneBoard(args)

