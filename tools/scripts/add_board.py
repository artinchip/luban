#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+

# Copyright (C) 2022 ArtInChip
# Wu Dehuang

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

def GetChipList(targetdir):
    cmd = 'grep "source *target/" {}'.format(targetdir + 'Config.in')
    matchlist = RunShellCommand(cmd)
    matchlist = matchlist.replace('source', '').replace('target/', '').replace('/Config.in', '')
    matchlist = matchlist.split()
    chiplist = []
    for c in matchlist:
        if os.path.isdir(targetdir + c):
            chiplist.append(c)
    return chiplist

def SelectChip(chiplist):
    num = 1
    print(indent_str('Chip list:', 1))
    for c in chiplist:
        print(indent_str('{}: {}'.format(num, chiplist[num - 1]), 2))
        num += 1
    inputstr = input(indent_str('Select chip for new board(number): ', 1)).strip()
    if inputstr.isdigit() == False:
        print(indent_str('Error, please input number'), 1)
        sys.exit(1)
    num = int(inputstr)
    if num not in range(1, len(chiplist) + 1):
        print(indent_str('Error, input number not in list', 1))
        sys.exit(1)

    selected = chiplist[num - 1]
    print(indent_str(selected + '\n', 2))
    return selected

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

def SelectReferenceDefconfig(chip, defconfigs, dotconfigs):
    reflist = []
    for cfg in defconfigs:
        for dotcfg in dotconfigs:
            if os.path.basename(cfg) in dotcfg:
                break
        chipname = GetChipNameOfDefconfig(dotcfg)
        if chipname == chip:
            reflist.append(os.path.basename(cfg))
    if len(reflist) == 0:
        print(indent_str('Error, cannot find reference defconfig', 1))
        sys.exit(1)
    print(indent_str('Reference defconfig:(Create new board base on selected defconfig)', 1))
    num = 1
    for ref in reflist:
        print(indent_str('{}: {}'.format(num, ref), 2))
        num += 1
    inputstr = input(indent_str('Select reference defconfig for new board(number): ', 1)).strip()
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

def CheckBoardNameIsValid(name):
    invalid_name_char = '`~!@#$%^&*()-+=[]\\{}|;\':",./<>?'
    for c in invalid_name_char:
        if c in name:
            return False
    return True

def GetBoardNameForConfig(name):
    return name.replace('\t', '_').replace(' ', '_')

def GetNewBoardName():
    inputstr = input(indent_str("Input new board's name: ", 1)).strip()
    if len(inputstr.strip()) == 0:
        print(indent_str('Invalid name', 1))
        sys.exit(1)
    name = inputstr.strip()
    if CheckBoardNameIsValid(name) == False:
        print(indent_str('Invalid name', 1))
        sys.exit(1)
    print(indent_str(name + '\n', 2))
    return name

def GetNewManufacturerName():
    inputstr = input(indent_str("Input manufacturer's name: ", 1)).strip()
    if len(inputstr.strip()) == 0:
        print(indent_str('Invalid name', 1))
        sys.exit(1)
    name = inputstr.strip()
    print(indent_str(name + '\n', 2))
    return name

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

def CreateNewBoardDirectory(topdir, refcfg, dotconfigs, chip, boardname):

    for ref_dotcfg in dotconfigs:
        if refcfg in ref_dotcfg:
            break

    boardname_cfg = GetBoardNameForConfig(boardname)

    newboard_dir = topdir + 'target/{}/{}'.format(chip, boardname_cfg)
    if os.path.exists(newboard_dir):
        print(indent_str("board {} already exist.".format(boardname_cfg), 1));
        sys.exit(1)

    refboardname = GetBoardNameOfDefconfig(ref_dotcfg)
    refboard_dir = topdir + 'target/{}/{}'.format(chip, refboardname)

    # copy new board directory
    cmd = 'cp -rdf {} {}'.format(refboard_dir, newboard_dir)
    RunShellCommand(cmd)
    print(indent_str('Created: ' + newboard_dir.replace(topdir, ''), 1))

    # update product name
    cmd = 'find {} -type f -name "*.json" | xargs -I [] sed -i \'s/"product": *"[a-z,0-9,A-Z,_, ]*"/"product": "{}"/g\' []'.format(newboard_dir, boardname_cfg)
    RunShellCommand(cmd)

def CreateNewDefconfig(topdir, refcfg, dotconfigs, chip, boardname, manu_name):
    global UBOOT_DIR
    global LINUX_DIR

    for ref_dotcfg in dotconfigs:
        if refcfg in ref_dotcfg:
            break

    arch = GetArchOfRefDefconfig(ref_dotcfg)
    refboardname = GetBoardNameOfDefconfig(ref_dotcfg)
    refboard_opt = GetBoardOptionNameOfDefconfig(ref_dotcfg, refboardname)
    boardname_cfg = GetBoardNameForConfig(boardname)

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

    # Create defconfig for u-boot
    newcfg_name = '{}_{}_defconfig'.format(chip, boardname_cfg)
    refcfg_path = '{}{}configs/{}'.format(topdir, UBOOT_DIR, ref_uboot_cfg)
    newcfg_path = '{}{}configs/{}'.format(topdir, UBOOT_DIR, newcfg_name)
    cmd = 'cp {} {}'.format(refcfg_path, newcfg_path)
    RunShellCommand(cmd)
    print(indent_str('Created: ' + newcfg_path.replace(topdir, ''), 1))

    # Create defconfig for linux kernel
    if arch == 'riscv64':
        arch = 'riscv'
    newcfg_name = '{}_{}_defconfig'.format(chip, boardname_cfg)
    refcfg_path = '{}{}arch/{}/configs/{}'.format(topdir, LINUX_DIR, arch, ref_linux_cfg)
    newcfg_path = '{}{}arch/{}/configs/{}'.format(topdir, LINUX_DIR, arch, newcfg_name)
    cmd = 'cp {} {}'.format(refcfg_path, newcfg_path)
    RunShellCommand(cmd)
    print(indent_str('Created: ' + newcfg_path.replace(topdir, ''), 1))

    # Create config for busybox
    newcfg_name = '{}_{}_defconfig'.format(chip, boardname_cfg)
    refcfg_path = '{}{}'.format(topdir, ref_busybox_cfg)
    newbusybox_cfg = '{}/{}'.format(os.path.dirname(ref_busybox_cfg), newcfg_name)
    newcfg_path = '{}{}'.format(topdir, newbusybox_cfg)
    cmd = 'cp {} {}'.format(refcfg_path, newcfg_path)
    RunShellCommand(cmd)
    print(indent_str('Created: ' + newcfg_path.replace(topdir, ''), 1))

    # Create SDK defconfig
    newdefcfg_name = '{}_{}_defconfig'.format(chip, boardname_cfg)
    newcfg_path = '{}{}'.format(target_cfgdir, newdefcfg_name)
    f = open(newcfg_path, 'w')

    f.write('LUBAN_BOARD_{}_{}=y\n'.format(chip.upper(), boardname_cfg.upper()))
    f.write('BR2_TARGET_GENERIC_ISSUE="Welcome to {} linux"\n'.format(manu_name))
    for ln in lines:
        if refboard_opt in ln:
            # Skip ref board name
            continue
        if 'BR2_TARGET_GENERIC_HOSTNAME=' in ln:
            f.write('BR2_TARGET_GENERIC_HOSTNAME="{}"\n'.format(boardname))
            continue
        if 'BR2_TARGET_GENERIC_ISSUE=' in ln:
            # Skip old name
            continue
        if 'BR2_TARGET_UBOOT_BOARD_DEFCONFIG=' in ln:
            f.write('BR2_TARGET_UBOOT_BOARD_DEFCONFIG="{}_{}"\n'.format(chip, boardname_cfg))
            continue
        if 'BR2_LINUX_KERNEL_DEFCONFIG=' in ln:
            f.write('BR2_LINUX_KERNEL_DEFCONFIG="{}_{}"\n'.format(chip, boardname_cfg))
            continue
        rename_board = False
        if 'BR2_PACKAGE_BUSYBOX_CONFIG=' in ln:
            f.write('BR2_PACKAGE_BUSYBOX_CONFIG="{}"\n'.format(newbusybox_cfg))
            continue
        if 'BR2_ROOTFS_POST_BUILD_SCRIPT=' in ln:
            rename_board = True
        if 'BR2_ROOTFS_POST_FAKEROOT_SCRIPT=' in ln:
            rename_board = True
        if 'BR2_ROOTFS_POST_IMAGE_SCRIPT=' in ln:
            rename_board = True
        if 'BR2_ROOTFS_OVERLAY=' in ln:
            rename_board = True
        if 'BR2_TARGET_USERFS1_OVERLAY=' in ln:
            rename_board = True
        if 'BR2_TARGET_USERFS2_OVERLAY=' in ln:
            rename_board = True
        if 'BR2_TARGET_USERFS3_OVERLAY=' in ln:
            rename_board = True
        if rename_board:
            f.write(ln.replace('{}/{}'.format(chip, refboardname), '{}/{}'.format(chip, boardname_cfg)))
            continue
        f.write(ln)
    f.close()
    print(indent_str('Created: ' + newcfg_path.replace(topdir, ''), 1))

    # Add new board to Config.in
    cfg_in = '{}target/{}/Config.in'.format(topdir, chip)
    cmd = 'cp {} {}.old'.format(cfg_in, cfg_in)
    RunShellCommand(cmd)
    f = open(cfg_in, 'r')
    lines = f.readlines()
    f.close()

    board_option = 'LUBAN_BOARD_{}_{}'.format(chip.upper(), refboardname.upper())
    nboard_option = 'LUBAN_BOARD_{}_{}'.format(chip.upper(), boardname_cfg.upper())
    f = open(cfg_in, 'w')

    chip_option = GetChipOptionOfDefconfig(ref_dotcfg, chip)
    insert_config = False
    for ln in lines:
        if 'config' in ln and board_option in ln:
            insert_config = True
        if insert_config and 'endchoice' in ln:
            f.write('config {}\n'.format(nboard_option))
            f.write('\tbool "{}"\n'.format(boardname))
            f.write('\tdepends on {}\n\n'.format(chip_option))
            insert_config = False
        if 'default' in ln and board_option in ln:
            f.write('\tdefault "{}" if {}\n'.format(boardname_cfg, nboard_option))
        f.write(ln)
    f.close()
    print(indent_str('Updated: ' + cfg_in.replace(topdir, ''), 1))

def CreateNewBoard(args):
    target_cfgdir = args.topdir + 'target/configs/'
    defconfigs = GetReferenceBoardDefconfig(target_cfgdir)
    defconfigs.sort()
    if len(defconfigs) == 0:
        print(indent_str("Error: no defconfig is found.", 1));
        sys.exit(1)
    dotconfigs = GenDotConfig(args, defconfigs)

    chiplist = GetChipList(args.topdir + 'target/')
    if len(chiplist) == 0:
        print(indent_str("Error: no chip is found.", 1));
        sys.exit(1)
    chip = SelectChip(chiplist)
    refcfg = SelectReferenceDefconfig(chip, defconfigs, dotconfigs)
    boardname = GetNewBoardName()
    manu_name = GetNewManufacturerName()

    CreateNewBoardDirectory(args.topdir, refcfg, dotconfigs, chip, boardname)
    CreateNewDefconfig(args.topdir, refcfg, dotconfigs, chip, boardname, manu_name)

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
    CreateNewBoard(args)

