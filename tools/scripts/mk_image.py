#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2021-2024 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>

import os
import sys
import re
import math
import zlib
import json
import struct
import argparse
import platform
import subprocess
import pinmux_check
from pathlib import Path
from collections import namedtuple
from collections import OrderedDict
from Cryptodome.PublicKey import RSA
from Cryptodome.Hash import MD5
from Cryptodome.Hash import SHA256
from Cryptodome.Cipher import AES
from Cryptodome.Signature import PKCS1_v1_5
import binascii
import asn1crypto.core
import gmssl.sm2 as SM2
import gmssl.sm3 as SM3
import gmssl.sm4 as SM4
import gmssl.func as func

DATA_ALIGNED_SIZE = 2048
META_ALIGNED_SIZE = 512

# Whether or not to generate the image used by the burner
BURNER = False
VERBOSE = False
SIGN = False


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


def get_file_path(fpath, dirpath):
    if dirpath is not None and os.path.exists(dirpath + fpath):
        return dirpath + fpath
    if os.path.exists(fpath):
        return fpath
    return None


def aic_boot_get_resource_file_size(cfg, keydir, datadir):
    """ Get size of all resource files
    """

    files = {}
    filepath = ""
    if "resource" in cfg:
        if "private" in cfg["resource"]:
            filepath = get_file_path(cfg["resource"]["private"], keydir)
            if filepath is None:
                filepath = get_file_path(cfg["resource"]["private"], datadir)
            if filepath is None:
                print("Error, {} is not found.".format(cfg["resource"]["private"]))
                sys.exit(1)
            statinfo = os.stat(filepath)
            files["resource/private"] = statinfo.st_size
            files["round(resource/private)"] = round_up(statinfo.st_size, 32)

        if "pubkey" in cfg["resource"]:
            filepath = get_file_path(cfg["resource"]["pubkey"], keydir)
            if filepath is None:
                filepath = get_file_path(cfg["resource"]["pubkey"], datadir)
            if filepath is None:
                print("Error, {} is not found.".format(cfg["resource"]["pubkey"]))
                sys.exit(1)
            statinfo = os.stat(filepath)
            files["resource/pubkey"] = statinfo.st_size
            files["round(resource/pubkey)"] = round_up(statinfo.st_size, 32)
        if "pbp" in cfg["resource"]:
            filepath = get_file_path(cfg["resource"]["pbp"], datadir)
            if filepath is None:
                print("Error, {} is not found.".format(cfg["resource"]["pbp"]))
                sys.exit(1)
            statinfo = os.stat(filepath)
            files["resource/pbp"] = statinfo.st_size
            files["round(resource/pbp)"] = round_up(statinfo.st_size, 32)
    if "encryption" in cfg:
        if "iv" in cfg["encryption"]:
            filepath = get_file_path(cfg["encryption"]["iv"], keydir)
            if filepath is None:
                filepath = get_file_path(cfg["encryption"]["iv"], datadir)
            if filepath is None:
                print("Error, {} is not found.".format(cfg["encryption"]["iv"]))
                sys.exit(1)
            statinfo = os.stat(filepath)
            files["encryption/iv"] = statinfo.st_size
            files["round(encryption/iv)"] = round_up(statinfo.st_size, 32)
    if "loader" in cfg:
        if "file" in cfg["loader"]:
            filepath = get_file_path(cfg["loader"]["file"], datadir)
            if filepath is not None:
                statinfo = os.stat(filepath)
                if statinfo.st_size > (4 * 1024 * 1024):
                    print("Loader size is too large")
                    sys.exit(1)
                files["loader/file"] = statinfo.st_size
                files["round(loader/file)"] = round_up(statinfo.st_size, 256)
            else:
                print("File {} is not exist".format(cfg["loader"]["file"]))
                sys.exit(1)
    return files


def aic_boot_calc_image_length(filesizes, cfg):
    """ Calculate the boot image's total length
    """

    total_siz = filesizes["resource_start"]
    if "resource/pubkey" in filesizes:
        total_siz = total_siz + filesizes["round(resource/pubkey)"]
    if "encryption/iv" in filesizes:
        total_siz = total_siz + filesizes["round(encryption/iv)"]
    if "resource/private" in filesizes:
        total_siz = total_siz + filesizes["round(resource/private)"]
    if "resource/pbp" in filesizes:
        total_siz = total_siz + filesizes["round(resource/pbp)"]
    total_siz = round_up(total_siz, 256)
    if "signature" in cfg:
        # Add the length of signature
        if cfg["signature"]["algo"] == "sm2":
            total_siz = total_siz + 64
        else:
            total_siz = total_siz + 256
    else:
        # Add the length of md5
        total_siz = total_siz + 16
    return total_siz


def aic_boot_calc_image_length_for_ext(filesizes, sign):
    """ Calculate the boot image's total length
    """

    total_siz = filesizes["resource_start"]
    if "resource/pubkey" in filesizes:
        total_siz = total_siz + filesizes["round(resource/pubkey)"]
    if "encryption/iv" in filesizes:
        total_siz = total_siz + filesizes["round(encryption/iv)"]
    if "resource/private" in filesizes:
        total_siz = total_siz + filesizes["round(resource/private)"]
    total_siz = round_up(total_siz, 256)
    if sign:
        # Add the length of signature
        total_siz = total_siz + 256
    else:
        # Add the length of md5
        total_siz = total_siz + 16
    return total_siz


def check_loader_run_in_dram(cfg):
    """
        Legacy code, will be removed in later's version
    """

    if "loader" not in cfg:
        return False
    if "run in dram" in cfg["loader"]:
        if cfg["loader"]["run in dram"].upper() == "FALSE":
            return False
    return True


def aic_boot_get_loader_bytes(cfg, filesizes):
    """ Read the loader's binaray data, and perform encryption if it is needed.

        Legacy code, will be removed in later's version
    """

    loader_size = 0
    header_size = 256
    rawbytes = bytearray(0)
    if check_loader_run_in_dram(cfg):
        # No loader in first aicimg
        # Record the information to generate header and resource bytes
        filesizes["resource_start"] = header_size + loader_size
        return rawbytes

    if "round(loader/file)" in filesizes:
        loader_size = filesizes["round(loader/file)"]
        try:
            fpath = get_file_path(cfg["loader"]["file"], cfg["datadir"])
            with open(fpath, "rb") as f:
                rawbytes = f.read(loader_size)
        except IOError:
            print("Failed to open loader file: {}".format(fpath))
            sys.exit(1)

        if len(rawbytes) == 0:
            print("Read loader data failed.")
            sys.exit(1)
        if len(rawbytes) < loader_size:
            rawbytes = rawbytes + bytearray(loader_size - len(rawbytes))

    # Record the information to generate header and resource bytes
    filesizes["resource_start"] = header_size + loader_size

    # Use SSK(Symmetric Secure Key) derived AES key to encrypt it
    #
    #                SSK(128bit)
    #                 | (key)
    #                 v
    #  KM(128bit) -> AES -> HSK(128bit, in Secure SRAM)
    #                        | (key)
    #                        v
    #      SPL plaintext -> AES -> SPL ciphertext
    if "encryption" in cfg and loader_size > 0:
        if "encryption" in cfg and cfg["encryption"]["algo"] == "sm4-ecb":
            # Only encrypt loader content, if loader not exist, don't do it
            try:
                if os.path.exists(cfg["keydir"] + cfg["encryption"]["key"]):
                    fpath = cfg["keydir"] + cfg["encryption"]["key"]
                else:
                    fpath = cfg["datadir"] + cfg["encryption"]["key"]
                with open(fpath, "rb") as f:
                    keydata = f.read(16)
            except IOError:
                print('Failed to open aes key file')
                sys.exit(1)
            cipher = SM4.CryptSM4()
            cipher.set_key(keydata, SM4.SM4_ENCRYPT)
            enc_bytes = cipher.crypt_ecb(rawbytes)
            # with open('rawbytes.bin', 'wb') as f:
            #     f.write(rawbytes)
            # with open('encbytes.bin', 'wb') as f:
            #     f.write(enc_bytes[0:len(rawbytes)])
            return enc_bytes[0:len(rawbytes)]

        # Only encrypt loader content, if loader not exist, don't do it
        try:
            fpath = get_file_path(cfg["encryption"]["key"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["key"], cfg["datadir"])
            with open(fpath, "rb") as f:
                key_material = b"0123456789abcdef"
                symmetric_secure_key = f.read(16)
                cipher = AES.new(symmetric_secure_key, AES.MODE_ECB)
                hardware_secure_key = cipher.encrypt(key_material)
        except IOError:
            print('Failed to open symmetric secure key file')
            sys.exit(1)

        try:
            fpath = get_file_path(cfg["encryption"]["iv"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["iv"], cfg["datadir"])
            with open(fpath, "rb") as f:
                ivdata = f.read(16)
        except IOError:
            print('Failed to open iv file')
            sys.exit(1)
        cipher = AES.new(hardware_secure_key, AES.MODE_CBC, ivdata)
        enc_bytes = cipher.encrypt(rawbytes)
        return enc_bytes
    else:
        return rawbytes


def aic_boot_get_loader_for_ext(cfg, filesizes):
    """ Read the loader's binaray data, and perform encryption if it is needed.
    """

    loader_size = 0
    rawbytes = bytearray(0)
    if "round(loader/file)" in filesizes:
        loader_size = filesizes["round(loader/file)"]
        try:
            fpath = get_file_path(cfg["loader"]["file"], cfg["datadir"])
            with open(fpath, "rb") as f:
                rawbytes = f.read(loader_size)
        except IOError:
            print("Failed to open loader file: {}".format(fpath))
            sys.exit(1)

        if len(rawbytes) == 0:
            print("Read loader data failed.")
            sys.exit(1)
        if len(rawbytes) < loader_size:
            rawbytes = rawbytes + bytearray(loader_size - len(rawbytes))

    header_size = 256
    # Record the information to generate header and resource bytes
    filesizes["resource_start"] = header_size + loader_size

    # Use SSK(Symmetric Secure Key) derived AES key to encrypt it
    #
    #                SSK(128bit)
    #                 | (key)
    #                 v
    #  KM(128bit) -> AES -> HSK(128bit, in Secure SRAM)
    #                        | (key)
    #                        v
    #      SPL plaintext -> AES -> SPL ciphertext
    if "encryption" in cfg and loader_size > 0:
        # Only encrypt loader content, if loader not exist, don't do it
        try:
            fpath = get_file_path(cfg["encryption"]["key"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["key"], cfg["datadir"])
            with open(fpath, "rb") as f:
                key_material = b"0123456789abcdef"
                symmetric_secure_key = f.read(16)
                cipher = AES.new(symmetric_secure_key, AES.MODE_ECB)
                hardware_secure_key = cipher.encrypt(key_material)
        except IOError:
            print('Failed to open symmetric secure key file')
            sys.exit(1)

        try:
            fpath = get_file_path(cfg["encryption"]["iv"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["iv"], cfg["datadir"])
            with open(fpath, "rb") as f:
                ivdata = f.read(16)
        except IOError:
            print('Failed to open iv file')
            sys.exit(1)
        cipher = AES.new(hardware_secure_key, AES.MODE_CBC, ivdata)
        enc_bytes = cipher.encrypt(rawbytes)
        return enc_bytes
    else:
        return rawbytes


def aic_boot_get_loader_bytes_v2(cfg, filesizes):
    """ Read the loader's binaray data, and perform encryption if it is needed.
    """

    loader_size = 0
    header_size = 256
    rawbytes = bytearray(0)

    if "round(loader/file)" in filesizes:
        loader_size = filesizes["round(loader/file)"]
        try:
            fpath = get_file_path(cfg["loader"]["file"], cfg["datadir"])
            with open(fpath, "rb") as f:
                rawbytes = f.read(loader_size)
        except IOError:
            print("Failed to open loader file: {}".format(fpath))
            sys.exit(1)

        if len(rawbytes) == 0:
            print("Read loader data failed.")
            sys.exit(1)
        if len(rawbytes) < loader_size:
            rawbytes = rawbytes + bytearray(loader_size - len(rawbytes))

    # Record the information to generate header and resource bytes
    filesizes["resource_start"] = header_size + loader_size

    if "encryption" in cfg and loader_size > 0:
        if "encryption" in cfg and cfg["encryption"]["algo"] == "sm4-ecb":
            # Only encrypt loader content, if loader not exist, don't do it
            try:
                if os.path.exists(cfg["keydir"] + cfg["encryption"]["key"]):
                    fpath = cfg["keydir"] + cfg["encryption"]["key"]
                else:
                    fpath = cfg["datadir"] + cfg["encryption"]["key"]
                with open(fpath, "rb") as f:
                    keydata = f.read(16)
            except IOError:
                print('Failed to open aes key file')
                sys.exit(1)
            cipher = SM4.CryptSM4()
            cipher.set_key(keydata, SM4.SM4_ENCRYPT)
            enc_bytes = cipher.crypt_ecb(rawbytes)
            # with open('rawbytes.bin', 'wb') as f:
            #     f.write(rawbytes)
            # with open('encbytes.bin', 'wb') as f:
            #     f.write(enc_bytes[0:len(rawbytes)])
            return enc_bytes[0:len(rawbytes)]

        # Only encrypt loader content, if loader not exist, don't do it
        try:
            fpath = get_file_path(cfg["encryption"]["key"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["key"], cfg["datadir"])
            with open(fpath, "rb") as f:
                symmetric_secure_key = f.read(16)
                if aic_boot_use_ssk_derived_key(cfg):
                    # Use SSK(Symmetric Secure Key) derived AES key to encrypt it
                    #
                    #                SSK(128bit)
                    #                 | (key)
                    #                 v
                    #  KM(128bit) -> AES -> HSK(128bit, in Secure SRAM)
                    #                        | (key)
                    #                        v
                    #      SPL plaintext -> AES -> SPL ciphertext
                    key_material = b"0123456789abcdef"
                    cipher = AES.new(symmetric_secure_key, AES.MODE_ECB)
                    encrypt_key = cipher.encrypt(key_material)
                else:
                    encrypt_key = symmetric_secure_key
        except IOError:
            print('Failed to open symmetric secure key file')
            sys.exit(1)

        try:
            fpath = get_file_path(cfg["encryption"]["iv"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["iv"], cfg["datadir"])
            with open(fpath, "rb") as f:
                ivdata = f.read(16)
        except IOError:
            print('Failed to open iv file')
            sys.exit(1)
        cipher = AES.new(encrypt_key, AES.MODE_CBC, ivdata)
        enc_bytes = cipher.encrypt(rawbytes)
        return enc_bytes
    else:
        return rawbytes


def aic_boot_get_resource_bytes(cfg, filesizes):
    """ Pack all resource data into boot image's resource section
    """
    resbytes = bytearray(0)
    if "resource/pbp" in filesizes:
        pbp_size = filesizes["round(resource/pbp)"]
        try:
            fpath = get_file_path(cfg["resource"]["pbp"], cfg["datadir"])
            with open(fpath, "rb") as f:
                pbp_data = f.read(pbp_size)
        except IOError:
            print('Failed to open pbp file')
            sys.exit(1)
        resbytes = resbytes + pbp_data + bytearray(pbp_size - len(pbp_data))
    if "resource/private" in filesizes:
        priv_size = filesizes["round(resource/private)"]
        try:
            fpath = get_file_path(cfg["resource"]["private"], cfg["datadir"])
            with open(fpath, "rb") as f:
                privdata = f.read(priv_size)
        except IOError:
            print('Failed to open private file')
            sys.exit(1)
        resbytes = resbytes + privdata + bytearray(priv_size - len(privdata))
    if "resource/pubkey" in filesizes:
        pubkey_size = filesizes["round(resource/pubkey)"]
        try:
            fpath = get_file_path(cfg["resource"]["pubkey"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["resource"]["pubkey"], cfg["datadir"])
            with open(fpath, "rb") as f:
                pkdata = f.read(pubkey_size)
        except IOError:
            print('Failed to open pubkey file')
            sys.exit(1)
        # Add padding to make it alignment
        resbytes = resbytes + pkdata + bytearray(pubkey_size - len(pkdata))
    if "encryption/iv" in filesizes:
        iv_size = filesizes["round(encryption/iv)"]
        try:
            fpath = get_file_path(cfg["encryption"]["iv"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["iv"], cfg["datadir"])
            with open(fpath, "rb") as f:
                ivdata = f.read(iv_size)
        except IOError:
            print('Failed to open iv file')
            sys.exit(1)
        resbytes = resbytes + ivdata + bytearray(iv_size - len(ivdata))
    if len(resbytes) > 0:
        res_size = round_up(len(resbytes), 256)
        if len(resbytes) != res_size:
            resbytes = resbytes + bytearray(res_size - len(resbytes))
    return resbytes


def aic_boot_get_resource_for_ext(cfg, filesizes):
    """ Pack all resource data into boot image's resource section
    """

    resbytes = bytearray(0)
    if "resource/private" in filesizes:
        priv_size = filesizes["round(resource/private)"]
        try:
            fpath = get_file_path(cfg["resource"]["private"], cfg["datadir"])
            with open(fpath, "rb") as f:
                privdata = f.read(priv_size)
        except IOError:
            print('Failed to open private file')
            sys.exit(1)
        resbytes = resbytes + privdata + bytearray(priv_size - len(privdata))
    if "resource/pubkey" in filesizes:
        pubkey_size = filesizes["round(resource/pubkey)"]
        try:
            fpath = get_file_path(cfg["resource"]["pubkey"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["resource"]["pubkey"], cfg["datadir"])
            with open(fpath, "rb") as f:
                pkdata = f.read(pubkey_size)
        except IOError:
            print('Failed to open pubkey file')
            sys.exit(1)
        # Add padding to make it alignment
        resbytes = resbytes + pkdata + bytearray(pubkey_size - len(pkdata))
    if "encryption/iv" in filesizes:
        iv_size = filesizes["round(encryption/iv)"]
        try:
            fpath = get_file_path(cfg["encryption"]["iv"], cfg["keydir"])
            if fpath is None:
                fpath = get_file_path(cfg["encryption"]["iv"], cfg["datadir"])
            with open(fpath, "rb") as f:
                ivdata = f.read(iv_size)
        except IOError:
            print('Failed to open iv file')
            sys.exit(1)
        resbytes = resbytes + ivdata + bytearray(iv_size - len(ivdata))
    if len(resbytes) > 0:
        res_size = round_up(len(resbytes), 256)
        if len(resbytes) != res_size:
            resbytes = resbytes + bytearray(res_size - len(resbytes))
    return resbytes


def aic_boot_checksum(bootimg):
    length = len(bootimg)
    offset = 0
    total = 0
    while offset < length:
        val = int.from_bytes(bootimg[offset: offset + 4], byteorder='little', signed=False)
        total = total + val
        offset = offset + 4
    return (~total) & 0xFFFFFFFF


def aic_calc_checksum(start, size):
    offset = 0
    total = 0
    while offset < size:
        val = int.from_bytes(start[offset: offset + 4], byteorder='little', signed=False)
        total = total + val
        offset = offset + 4
    return (~total) & 0xFFFFFFFF


def aic_boot_add_header(h, n):
    return h + n.to_bytes(4, byteorder='little', signed=False)


def aic_boot_with_ext_loader(cfg):
    if "with_ext" in cfg and cfg["with_ext"].upper() == "TRUE":
        return True
    return False


def aic_boot_use_ssk_derived_key(cfg):
    # A special case
    if "ssk_derived_key" in cfg and cfg["ssk_derived_key"].upper() == "TRUE":
        return True
    return False


def aic_boot_gen_header_bytes(cfg, filesizes):
    """ Generate header bytes

        Legacy code, will be removed in later's version
    """
    # Prepare header information
    magic = "AIC "
    checksum = 0
    header_ver = int("0x00010001", 16)
    if "head_ver" in cfg:
        header_ver = int(cfg["head_ver"], 16)

    img_len = aic_boot_calc_image_length(filesizes, cfg)
    fw_ver = 0
    if "anti-rollback counter" in cfg:
        fw_ver = cfg["anti-rollback counter"]

    loader_length = 0
    if "loader/file" in filesizes:
        loader_length = filesizes["loader/file"]

    loader_ext_offset = 0
    if check_loader_run_in_dram(cfg):
        loader_length = 0
        loader_ext_offset = img_len
        # ensure ext loader start position is aligned to 512
        loader_ext_offset = round_up(img_len, META_ALIGNED_SIZE)

    load_address = 0
    entry_point = 0
    if "loader" in cfg:
        load_address = int(cfg["loader"]["load address"], 16)
        entry_point = int(cfg["loader"]["entry point"], 16)
    sign_algo = 0
    sign_offset = 0
    sign_length = 0
    sign_key_offset = 0
    sign_key_length = 0
    next_res_offset = filesizes["resource_start"]
    pbp_data_offset = 0
    pbp_data_length = 0
    if "resource" in cfg and "pbp" in cfg["resource"]:
        pbp_data_offset = next_res_offset
        pbp_data_length = filesizes["resource/pbp"]
        next_res_offset = pbp_data_offset + filesizes["round(resource/pbp)"]
    priv_data_offset = 0
    priv_data_length = 0
    if "resource" in cfg and "private" in cfg["resource"]:
        priv_data_offset = next_res_offset
        priv_data_length = filesizes["resource/private"]
        next_res_offset = priv_data_offset + filesizes["round(resource/private)"]
    if "signature" in cfg and cfg["signature"]["algo"] == "rsa,2048":
        sign_algo = 1
        sign_length = 256
        sign_offset = img_len - sign_length
    elif "signature" in cfg and cfg["signature"]["algo"] == "sm2":
        sign_algo = 2
        sign_length = 64
        sign_offset = img_len - sign_length
    else:
        # Append md5 result to the end
        sign_algo = 0
        sign_length = 16
        sign_offset = img_len - sign_length

    if "resource" in cfg and "pubkey" in cfg["resource"]:
        sign_key_offset = next_res_offset
        # Set the length value equal to real size
        sign_key_length = filesizes["resource/pubkey"]
        # Calculate offset use the size after alignment
        next_res_offset = sign_key_offset + filesizes["round(resource/pubkey)"]
    enc_algo = 0
    iv_data_offset = 0
    iv_data_length = 0
    if "encryption" in cfg and cfg["encryption"]["algo"] == "aes-128-cbc" and loader_length != 0:
        enc_algo = 1
        iv_data_offset = next_res_offset
        iv_data_length = 16
        next_res_offset = iv_data_offset + filesizes["round(encryption/iv)"]
    elif "encryption" in cfg and cfg["encryption"]["algo"] == "sm4-ecb":
        enc_algo = 2
    # Generate header bytes
    header_bytes = magic.encode(encoding="utf-8")
    header_bytes = aic_boot_add_header(header_bytes, checksum)
    header_bytes = aic_boot_add_header(header_bytes, header_ver)
    header_bytes = aic_boot_add_header(header_bytes, img_len)
    header_bytes = aic_boot_add_header(header_bytes, fw_ver)
    header_bytes = aic_boot_add_header(header_bytes, loader_length)
    header_bytes = aic_boot_add_header(header_bytes, load_address)
    header_bytes = aic_boot_add_header(header_bytes, entry_point)
    header_bytes = aic_boot_add_header(header_bytes, sign_algo)
    header_bytes = aic_boot_add_header(header_bytes, enc_algo)
    header_bytes = aic_boot_add_header(header_bytes, sign_offset)
    header_bytes = aic_boot_add_header(header_bytes, sign_length)
    header_bytes = aic_boot_add_header(header_bytes, sign_key_offset)
    header_bytes = aic_boot_add_header(header_bytes, sign_key_length)
    header_bytes = aic_boot_add_header(header_bytes, iv_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, iv_data_length)
    header_bytes = aic_boot_add_header(header_bytes, priv_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, priv_data_length)
    header_bytes = aic_boot_add_header(header_bytes, pbp_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, pbp_data_length)
    header_bytes = aic_boot_add_header(header_bytes, loader_ext_offset)
    header_bytes = header_bytes + bytearray(256 - len(header_bytes))
    return header_bytes


def aic_boot_gen_header_for_ext(cfg, filesizes):
    """ Generate header bytes

        Legacy code, will be removed in later's version
    """
    # Prepare header information
    magic = "AIC "
    checksum = 0
    header_ver = int("0x00010001", 16)
    if "head_ver" in cfg:
        header_ver = int(cfg["head_ver"], 16)

    img_len = aic_boot_calc_image_length_for_ext(filesizes, "signature" in cfg)
    fw_ver = 0

    loader_length = 0
    if "loader/file" in filesizes:
        loader_length = filesizes["loader/file"]

    loader_ext_offset = 0

    load_address = 0
    entry_point = 0
    if "loader" in cfg:
        if "load address ext" in cfg["loader"]:
            load_address = int(cfg["loader"]["load address ext"], 16)
        else:
            load_address = int(cfg["loader"]["load address"], 16)
        if "entry point ext" in cfg["loader"]:
            entry_point = int(cfg["loader"]["entry point ext"], 16)
        else:
            entry_point = int(cfg["loader"]["entry point"], 16)
    sign_algo = 0
    sign_offset = 0
    sign_length = 0
    sign_key_offset = 0
    sign_key_length = 0
    next_res_offset = filesizes["resource_start"]
    priv_data_offset = 0
    priv_data_length = 0
    if "resource" in cfg and "private" in cfg["resource"]:
        priv_data_offset = next_res_offset
        priv_data_length = filesizes["resource/private"]
        next_res_offset = priv_data_offset + filesizes["round(resource/private)"]
    if "signature" in cfg and cfg["signature"]["algo"] == "rsa,2048":
        sign_algo = 1
        sign_length = 256
        sign_offset = img_len - sign_length
    else:
        # Append md5 result to the end
        sign_algo = 0
        sign_length = 16
        sign_offset = img_len - sign_length

    if "resource" in cfg and "pubkey" in cfg["resource"]:
        sign_key_offset = next_res_offset
        # Set the length value equal to real size
        sign_key_length = filesizes["resource/pubkey"]
        # Calculate offset use the size after alignment
        next_res_offset = sign_key_offset + filesizes["round(resource/pubkey)"]
    enc_algo = 0
    iv_data_offset = 0
    iv_data_length = 0
    if "encryption" in cfg and cfg["encryption"]["algo"] == "aes-128-cbc":
        enc_algo = 1
        iv_data_offset = next_res_offset
        iv_data_length = 16
        next_res_offset = iv_data_offset + filesizes["round(encryption/iv)"]
    pbp_data_offset = 0
    pbp_data_length = 0
    # Generate header bytes
    header_bytes = magic.encode(encoding="utf-8")
    header_bytes = aic_boot_add_header(header_bytes, checksum)
    header_bytes = aic_boot_add_header(header_bytes, header_ver)
    header_bytes = aic_boot_add_header(header_bytes, img_len)
    header_bytes = aic_boot_add_header(header_bytes, fw_ver)
    header_bytes = aic_boot_add_header(header_bytes, loader_length)
    header_bytes = aic_boot_add_header(header_bytes, load_address)
    header_bytes = aic_boot_add_header(header_bytes, entry_point)
    header_bytes = aic_boot_add_header(header_bytes, sign_algo)
    header_bytes = aic_boot_add_header(header_bytes, enc_algo)
    header_bytes = aic_boot_add_header(header_bytes, sign_offset)
    header_bytes = aic_boot_add_header(header_bytes, sign_length)
    header_bytes = aic_boot_add_header(header_bytes, sign_key_offset)
    header_bytes = aic_boot_add_header(header_bytes, sign_key_length)
    header_bytes = aic_boot_add_header(header_bytes, iv_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, iv_data_length)
    header_bytes = aic_boot_add_header(header_bytes, priv_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, priv_data_length)
    header_bytes = aic_boot_add_header(header_bytes, pbp_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, pbp_data_length)
    header_bytes = aic_boot_add_header(header_bytes, loader_ext_offset)
    header_bytes = header_bytes + bytearray(256 - len(header_bytes))
    return header_bytes


def aic_boot_gen_header_bytes_v2(cfg, filesizes):
    """ Generate header bytes
    """
    # Prepare header information
    magic = "AIC "
    checksum = 0
    header_ver = int("0x00010001", 16)
    if "head_ver" in cfg:
        header_ver = int(cfg["head_ver"], 16)

    img_len = aic_boot_calc_image_length(filesizes, cfg)
    fw_ver = 0
    if "anti-rollback counter" in cfg:
        fw_ver = cfg["anti-rollback counter"]

    loader_length = 0
    if "loader/file" in filesizes:
        loader_length = filesizes["loader/file"]

    loader_ext_offset = 0
    if aic_boot_with_ext_loader(cfg):
        loader_length = 0
        loader_ext_offset = img_len
        # ensure ext loader start position is aligned to 512
        loader_ext_offset = round_up(img_len, META_ALIGNED_SIZE)

    load_address = 0
    entry_point = 0
    if "loader" in cfg:
        if "load address" in cfg["loader"]:
            load_address = int(cfg["loader"]["load address"], 16)
        if "entry point" in cfg["loader"]:
            entry_point = int(cfg["loader"]["entry point"], 16)
        if "load address" in cfg["loader"] and "entry point" not in cfg["loader"]:
            entry_point = load_address + 256
        if "load address" not in cfg["loader"] and "entry point" in cfg["loader"]:
            load_address = entry_point - 256
    sign_algo = 0
    sign_offset = 0
    sign_length = 0
    sign_key_offset = 0
    sign_key_length = 0
    next_res_offset = filesizes["resource_start"]
    pbp_data_offset = 0
    pbp_data_length = 0
    if "resource" in cfg and "pbp" in cfg["resource"]:
        pbp_data_offset = next_res_offset
        pbp_data_length = filesizes["resource/pbp"]
        next_res_offset = pbp_data_offset + filesizes["round(resource/pbp)"]
    priv_data_offset = 0
    priv_data_length = 0
    if "resource" in cfg and "private" in cfg["resource"]:
        priv_data_offset = next_res_offset
        priv_data_length = filesizes["resource/private"]
        next_res_offset = priv_data_offset + filesizes["round(resource/private)"]
    if "signature" in cfg and cfg["signature"]["algo"] == "rsa,2048":
        sign_algo = 1
        sign_length = 256
        sign_offset = img_len - sign_length
    elif "signature" in cfg and cfg["signature"]["algo"] == "sm2":
        sign_algo = 2
        sign_length = 64
        sign_offset = img_len - sign_length
    else:
        # Append md5 result to the end
        sign_algo = 0
        sign_length = 16
        sign_offset = img_len - sign_length

    if "resource" in cfg and "pubkey" in cfg["resource"]:
        sign_key_offset = next_res_offset
        # Set the length value equal to real size
        sign_key_length = filesizes["resource/pubkey"]
        # Calculate offset use the size after alignment
        next_res_offset = sign_key_offset + filesizes["round(resource/pubkey)"]
    enc_algo = 0
    iv_data_offset = 0
    iv_data_length = 0
    if "encryption" in cfg and cfg["encryption"]["algo"] == "aes-128-cbc" and loader_length != 0:
        enc_algo = 1
        iv_data_offset = next_res_offset
        iv_data_length = 16
        next_res_offset = iv_data_offset + filesizes["round(encryption/iv)"]
    elif "encryption" in cfg and cfg["encryption"]["algo"] == "sm4-ecb":
        enc_algo = 2
    # Generate header bytes
    header_bytes = magic.encode(encoding="utf-8")
    header_bytes = aic_boot_add_header(header_bytes, checksum)
    header_bytes = aic_boot_add_header(header_bytes, header_ver)
    header_bytes = aic_boot_add_header(header_bytes, img_len)
    header_bytes = aic_boot_add_header(header_bytes, fw_ver)
    header_bytes = aic_boot_add_header(header_bytes, loader_length)
    header_bytes = aic_boot_add_header(header_bytes, load_address)
    header_bytes = aic_boot_add_header(header_bytes, entry_point)
    header_bytes = aic_boot_add_header(header_bytes, sign_algo)
    header_bytes = aic_boot_add_header(header_bytes, enc_algo)
    header_bytes = aic_boot_add_header(header_bytes, sign_offset)
    header_bytes = aic_boot_add_header(header_bytes, sign_length)
    header_bytes = aic_boot_add_header(header_bytes, sign_key_offset)
    header_bytes = aic_boot_add_header(header_bytes, sign_key_length)
    header_bytes = aic_boot_add_header(header_bytes, iv_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, iv_data_length)
    header_bytes = aic_boot_add_header(header_bytes, priv_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, priv_data_length)
    header_bytes = aic_boot_add_header(header_bytes, pbp_data_offset)
    header_bytes = aic_boot_add_header(header_bytes, pbp_data_length)
    header_bytes = aic_boot_add_header(header_bytes, loader_ext_offset)
    header_bytes = header_bytes + bytearray(256 - len(header_bytes))
    return header_bytes


def aic_boot_gen_signature_bytes(cfg, bootimg):
    """ Generate RSASSA-PKCS1-v1.5 Signature with SHA-256
    """
    if "privkey" not in cfg["signature"]:
        print("RSA Private key is not exist.")
        sys.exit(1)
    try:
        fpath = get_file_path(cfg["signature"]["privkey"], cfg["keydir"])
        if fpath is None:
            fpath = get_file_path(cfg["signature"]["privkey"], cfg["datadir"])
        with open(fpath, 'rb') as frsa:
            rsakey = RSA.importKey(frsa.read())
    except IOError:
        print("Failed to open file: " + cfg["signature"]["privkey"])
        sys.exit(1)
    # Check if it is private key
    if rsakey.has_private() is False:
        print("Should to use RSA private key to sign")
        sys.exit(1)
    keysize = max(1, math.ceil(rsakey.n.bit_length() / 8))
    if keysize != 256:
        print("Only RSA 2048 is supported, please input RSA 2048 Private Key.")
        sys.exit(1)
    # Calculate SHA-256 hash
    sha256 = SHA256.new()
    sha256.update(bootimg)
    # Encrypt the hash, and using RSASSA-PKCS1-V1.5 Padding
    signer = PKCS1_v1_5.new(rsakey)
    sign_bytes = signer.sign(sha256)
    return sign_bytes


def aic_boot_gen_img_md5_bytes(cfg, bootimg):
    """ Calculate MD5 of image to make brom verify image faster
    """
    # Calculate MD5 hash
    md5 = MD5.new()
    md5.update(bootimg)
    md5_bytes = md5.digest()
    return md5_bytes


def aic_boot_check_params(cfg):
    if "encryption" in cfg and (cfg["encryption"]["algo"] != "aes-128-cbc" and
            cfg["encryption"]["algo"] != "sm4-ecb"):
        print("Only support aes-128-cbc or sm4-ecb encryption")
        return False
    if "signature" in cfg and (cfg["signature"]["algo"] != "rsa,2048" and
            cfg["signature"]["algo"] != "sm2"):
        print("Only support rsa,2048 or sm2 signature")
        return False
    # if "loader" not in cfg or "load address" not in cfg["loader"]:
    #     print("load address is not set")
    #     return False
    # if "loader" not in cfg or "entry point" not in cfg["loader"]:
    #     print("entry point is not set")
    #     return False
    return True

def get_sm2_key_pair(derfile):
    pk = None
    pr = None
    try:
        with open(derfile, 'rb') as fsm2:
            asn1 = asn1crypto.core.load(fsm2.read())
            # asn1.debug()
            asn1._parse_children()
            pr = asn1.children[1][4]
            value = asn1.children[3][4][2:]
            if value[0]:
                pk = value
            else:
                pk = value[1:]
    except IOError:
        print('Failed to open file: ' + derfile)
        sys.exit(1)
    priv_key_hex = binascii.hexlify(pr).decode('utf-8')
    pub_key_hex = binascii.hexlify(pk).decode('utf-8')
    return (priv_key_hex, pub_key_hex)

def aic_boot_gen_sm2_signature_bytes(cfg, bootimg):
    """ Generate SM2 Signature with SM3
    """
    if "privkey" not in cfg["signature"]:
        print("SM2 Private key is not exist.")
        sys.exit(1)
    if os.path.exists(cfg["keydir"] + cfg["signature"]["privkey"]):
        fpath = cfg["keydir"] + cfg["signature"]["privkey"]
    else:
        fpath = cfg["datadir"] + cfg["signature"]["privkey"]
    (pr, pk) = get_sm2_key_pair(fpath)
    sm2_crypt = SM2.CryptSM2(public_key=pk, private_key=pr)
    sm3_str = SM3.sm3_hash(bytearray(bootimg))
    sm3_bin = binascii.unhexlify(sm3_str)
    random_str = func.random_hex(sm2_crypt.para_len)
    sign_str = sm2_crypt.sign(sm3_bin, random_str)
    sign_bytes = binascii.unhexlify(sign_str)
    return sign_bytes

def aic_boot_create_image(cfg, keydir, datadir):
    """ Create AIC format Boot Image for Boot ROM

        Legacy code, will be removed in later's version
    """
    if aic_boot_check_params(cfg) is False:
        sys.exit(1)
    filesizes = aic_boot_get_resource_file_size(cfg, keydir, datadir)

    loader_bytes = aic_boot_get_loader_bytes(cfg, filesizes)
    resource_bytes = bytearray(0)
    if "resource" in cfg or "encryption" in cfg:
        resource_bytes = aic_boot_get_resource_bytes(cfg, filesizes)
    header_bytes = aic_boot_gen_header_bytes(cfg, filesizes)
    bootimg = header_bytes + loader_bytes + resource_bytes

    head_ver = int("0x00010001", 16)
    if "head_ver" in cfg:
        head_ver = int(cfg["head_ver"], 16)
    if "signature" in cfg:
        if "signature" in cfg and cfg["signature"]["algo"] == "sm2":
            signature_bytes = aic_boot_gen_sm2_signature_bytes(cfg, bootimg)
        else:
            signature_bytes = aic_boot_gen_signature_bytes(cfg, bootimg)
        bootimg = bootimg + signature_bytes
        return bootimg

    # Secure boot is not enabled, always add md5 result to the end
    md5_bytes = aic_boot_gen_img_md5_bytes(cfg, bootimg[8:])
    bootimg = bootimg + md5_bytes
    # Calculate checksum.
    # When MD5 is disabled, checksum will be checked by BROM.
    cs = aic_boot_checksum(bootimg)
    cs_bytes = cs.to_bytes(4, byteorder='little', signed=False)
    bootimg = bootimg[0:4] + cs_bytes + bootimg[8:]
    # Verify the checksum value
    cs = aic_boot_checksum(bootimg)
    if cs != 0:
        print("Checksum is error: {}".format(cs))
        sys.exit(1)
    return bootimg


def aic_boot_create_ext_image(cfg, keydir, datadir):
    """ Create AIC format Boot Image for Boot ROM

        Legacy code, will be removed in later's version
    """

    filesizes = aic_boot_get_resource_file_size(cfg, keydir, datadir)
    loader_bytes = aic_boot_get_loader_for_ext(cfg, filesizes)
    resource_bytes = bytearray(0)
    if "resource" in cfg:
        resource_bytes = aic_boot_get_resource_for_ext(cfg, filesizes)
    header_bytes = aic_boot_gen_header_for_ext(cfg, filesizes)
    bootimg = header_bytes + loader_bytes + resource_bytes

    head_ver = int("0x00010001", 16)
    if "head_ver" in cfg:
        head_ver = int(cfg["head_ver"], 16)
    if "signature" in cfg:
        signature_bytes = aic_boot_gen_signature_bytes(cfg, bootimg)
        bootimg = bootimg + signature_bytes
        return bootimg

    # Secure boot is not enabled, always add md5 result to the end
    md5_bytes = aic_boot_gen_img_md5_bytes(cfg, bootimg[8:])
    bootimg = bootimg + md5_bytes
    # Calculate checksum.
    # When MD5 is disabled, checksum will be checked by BROM.
    cs = aic_boot_checksum(bootimg)
    cs_bytes = cs.to_bytes(4, byteorder='little', signed=False)
    bootimg = bootimg[0:4] + cs_bytes + bootimg[8:]
    # Verify the checksum value
    cs = aic_boot_checksum(bootimg)
    if cs != 0:
        print("Checksum is error: {}".format(cs))
        sys.exit(1)
    return bootimg


def aic_boot_create_image_v2(cfg, keydir, datadir):
    """ Create AIC format Boot Image for Boot ROM
    """
    if aic_boot_check_params(cfg) is False:
        sys.exit(1)
    filesizes = aic_boot_get_resource_file_size(cfg, keydir, datadir)

    loader_bytes = aic_boot_get_loader_bytes_v2(cfg, filesizes)
    resource_bytes = bytearray(0)
    if "resource" in cfg or "encryption" in cfg:
        resource_bytes = aic_boot_get_resource_bytes(cfg, filesizes)
    header_bytes = aic_boot_gen_header_bytes_v2(cfg, filesizes)
    bootimg = header_bytes + loader_bytes + resource_bytes

    head_ver = int("0x00010001", 16)
    if "head_ver" in cfg:
        head_ver = int(cfg["head_ver"], 16)
    if "signature" in cfg:
        if "signature" in cfg and cfg["signature"]["algo"] == "sm2":
            signature_bytes = aic_boot_gen_sm2_signature_bytes(cfg, bootimg)
        else:
            signature_bytes = aic_boot_gen_signature_bytes(cfg, bootimg)
        bootimg = bootimg + signature_bytes
        return bootimg

    # Secure boot is not enabled, always add md5 result to the end
    md5_bytes = aic_boot_gen_img_md5_bytes(cfg, bootimg[8:])
    bootimg = bootimg + md5_bytes
    if aic_boot_with_ext_loader(cfg):
        padlen = round_up(len(bootimg), META_ALIGNED_SIZE) - len(bootimg)
        if padlen > 0:
            bootimg += bytearray(padlen)
    # Calculate checksum.
    # When MD5 is disabled, checksum will be checked by BROM.
    cs = aic_boot_checksum(bootimg)
    cs_bytes = cs.to_bytes(4, byteorder='little', signed=False)
    bootimg = bootimg[0:4] + cs_bytes + bootimg[8:]
    # Verify the checksum value
    cs = aic_boot_checksum(bootimg)
    if cs != 0:
        print("Checksum is error: {}".format(cs))
        sys.exit(1)
    return bootimg


def itb_create_image(itsname, itbname, keydir, dtbname, script_dir):
    # If the key exists, generate image signature information and write it to the itb file.
    # If the key exists, write the public key to the dtb file.
    if keydir is not None and dtbname is not None and SIGN:
        cmd = ["mkimage", "-f", itsname, "-k", keydir, "-K", dtbname, "-r", itbname]
    else:
        cmd = ["mkimage", "-f", itsname, itbname]

    ret = subprocess.run(cmd, stdout=subprocess.PIPE)
    if ret.returncode != 0:
        sys.exit(1)


def spienc_create_image(imgcfg, script_dir):

    keypath = get_file_path(imgcfg["key"], imgcfg["keydir"])
    if keypath is None:
        keypath = get_file_path(imgcfg["key"], imgcfg["datadir"])

    mkcmd = os.path.join(script_dir, "spienc")
    if os.path.exists(mkcmd) is False:
        mkcmd = "spienc"
    if sys.platform == "win32":
        mkcmd += ".exe"
    cmd = [mkcmd]
    cmd.append("--key")
    cmd.append("{}".format(keypath))
    if "nonce" in imgcfg:
        noncepath = get_file_path(imgcfg["nonce"], imgcfg["keydir"])
        if noncepath is None:
            noncepath = get_file_path(imgcfg["nonce"], imgcfg["datadir"])
        cmd.append("--nonce")
        cmd.append("{}".format(noncepath))
    if "tweak" in imgcfg:
        cmd.append("--tweak")
        cmd.append("{}".format(imgcfg["tweak"]))
    cmd.append("--addr")
    cmd.append("{}".format(imgcfg["address"]))
    cmd.append("--input")
    cmd.append("{}".format(imgcfg["input"]))
    cmd.append("--output")
    cmd.append("{}".format(imgcfg["output"]))
    ret = subprocess.run(cmd, stdout=subprocess.PIPE)
    if ret.returncode != 0:
        print(ret.stdout.decode("utf-8"))
        sys.exit(1)


def concatenate_create_image(outname, flist, datadir):
    with open(outname, "wb") as fout:
        for fn in flist:
            fpath = get_file_path(fn, datadir)
            if fpath is None:
                print("Error, {} is not found.".format(fn))
                sys.exit(1)
            fin = open(fpath, "rb")
            data = fin.read()
            fout.write(data)
            fin.close()


def img_gen_fw_file_name(cfg):
    # Image file name format:
    # <platform>_<product>_v<version>_c<anti-rollback counter>.img
    img_file_name = cfg["image"]["info"]["platform"]
    img_file_name += "_"
    img_file_name += cfg["image"]["info"]["product"]
    img_file_name += "_v"
    img_file_name += cfg["image"]["info"]["version"]
    if "anti-rollback" in cfg["image"]["info"]:
        img_file_name += "_c"
        img_file_name += cfg["image"]["info"]["anti-rollback"]
    img_file_name += ".img"
    return img_file_name.replace(" ", "_")


def calc_crc32(fname, size):
    """Calculate crc32 for a file
    Args:
        fname: file path
    """
    hash = 0
    step = 16 * 1024
    if size > 0:
        step = size

    if os.path.exists(fname) is False:
        return 0

    with open(fname, 'rb') as fp:
        while True:
            s = fp.read(step)
            if not s:
                break
            hash = zlib.crc32(s, hash)
            if size > 0:
                # only need to calc first 'size' byte
                break
    return hash & 0xffffffff


def size_str_to_int(size_str):
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


def str_to_nbytes(s, n):
    """ String to n bytes
    """
    ba = bytearray(s, encoding="utf-8")
    nzero = n - len(ba)
    if nzero > 0:
        ba.extend([0] * nzero)
    return bytes(ba)


def str_from_nbytes(s):
    """ String from n bytes
    """
    return str(s, encoding='utf-8')


def int_to_uint32_bytes(n):
    """ Int value to uint32 bytes
    """
    return n.to_bytes(4, byteorder='little', signed=False)


def int_to_uint8_bytes(n):
    """ Int value to uint8 bytes
    """
    return n.to_bytes(1, byteorder='little', signed=False)


def int_to_uint16_bytes(n):
    """ Int value to uint8 bytes
    """
    return n.to_bytes(2, byteorder='little', signed=False)


def int_from_uint32_bytes(s):
    """ Int value from uint32 bytes
    """
    return int.from_bytes(s, byteorder='little', signed=False)


def gen_bytes(n, length):
    """ gen len uint8 bytes
    """
    return bytearray([n] * length)


"""
struct artinchip_fw_hdr{
    char magic[8];
    char platform[64];
    char product[64];
    char version[64];
    char media_type[64];
    u32  media_dev_id;
    u8   nand_array_org[64];/* NAND Array Organization */
    u32  meta_offset; /* Meta Area start offset */
    u32  meta_size;   /* Meta Area size */
    u32  file_offset; /* File data Area start offset */
    u32  file_size;   /* File data Area size */
    u32  ex_flag;     /* Extension flag */
    u32  ex_offset;   /* Extension File data Area start offset */
    u32  ex_size;     /* Extension File data Area size */
};
"""


def img_write_fw_header(imgfile, cfg, meta_area_size, file_area_size, ex_area_size):
    """ Generate Firmware image's header data
    Args:
        cfg: Dict from JSON
        meta_area_size: size of meta data area
        file_area_size: size of file data area
    """
    array_org_len = 64
    nand_array_org = ""
    if "array_organization" in cfg["image"]["info"]["media"]:
        array_orgval = cfg["image"]["info"]["media"]["array_organization"]
        if not isinstance(array_orgval, list):
            print("Error, nand array organization should be a list.")
            return -1
        param_str = ""
        for item in array_orgval:
            param_str += "P={},B={};".format(item["page"].upper(), item["block"].upper())
        param_str = param_str[0:-1]
        nand_array_org = param_str
    dev_id = 0
    if "device_id" in cfg["image"]["info"]["media"]:
        val = cfg["image"]["info"]["media"]["device_id"]
        if isinstance(val, str):
            dev_id = int(val)
        else:
            dev_id = val

    magic = "AIC.FW"
    platform = str(cfg["image"]["info"]["platform"])
    product = str(cfg["image"]["info"]["product"])
    version = str(cfg["image"]["info"]["version"])
    media_type = str(cfg["image"]["info"]["media"]["type"])
    media_dev_id = dev_id
    meta_offset = DATA_ALIGNED_SIZE
    meta_size = meta_area_size
    file_offset = DATA_ALIGNED_SIZE + meta_area_size
    file_size = file_area_size
    ex_flag = 0
    ex_offset = DATA_ALIGNED_SIZE + meta_area_size + file_area_size - ex_area_size
    ex_size = ex_area_size

    buff = str_to_nbytes("AIC.FW", 8)
    buff = buff + str_to_nbytes(platform, 64)
    buff = buff + str_to_nbytes(product, 64)
    buff = buff + str_to_nbytes(version, 64)
    buff = buff + str_to_nbytes(media_type, 64)
    buff = buff + int_to_uint32_bytes(media_dev_id)
    buff = buff + str_to_nbytes(nand_array_org, 64)
    buff = buff + int_to_uint32_bytes(meta_offset)
    buff = buff + int_to_uint32_bytes(meta_size)
    buff = buff + int_to_uint32_bytes(file_offset)
    buff = buff + int_to_uint32_bytes(file_size)
    buff = buff + int_to_uint32_bytes(ex_flag)
    buff = buff + int_to_uint32_bytes(ex_offset)
    buff = buff + int_to_uint32_bytes(ex_size)
    imgfile.seek(0, 0)
    imgfile.write(buff)
    imgfile.flush()

    if VERBOSE:
        print("\tImage header is generated.")
    return 0


"""
struct artinchip_fwc_meta {
    char magic[8];
    char name[64];
    char partition[64];
    u32  offset;
    u32  size;
    u32  crc;
    u32  ram;
    char attr[64]
    char filename[64]
};
"""


def img_gen_fwc_meta(name, part, offset, size, crc, ram, attr, filename):
    """ Generate Firmware component's meta data
    Args:
        cfg: Dict from JSON
        datadir: working directory for image data
    """
    buff = str_to_nbytes("META", 8)
    buff = buff + str_to_nbytes(name, 64)
    buff = buff + str_to_nbytes(part, 64)
    buff = buff + int_to_uint32_bytes(offset)
    buff = buff + int_to_uint32_bytes(size)
    buff = buff + int_to_uint32_bytes(crc)
    buff = buff + int_to_uint32_bytes(ram)
    buff = buff + str_to_nbytes(attr, 64)
    buff = buff + str_to_nbytes(filename, 64)

    if VERBOSE:
        print("\t\tMeta for {:<25} offset {:<10} size {} ({})".format(
              name, hex(offset), hex(size), size))
    return buff


PAGE_TABLE_MAX_ENTRY = 101

"""
struct nand_page_table_head {
    char magic[4]; /* AICP: AIC Page table */
    u32 entry_cnt;
    u16 page_size;
    u8 pad[10];   /* Padding it to fit size 20 bytes */
};

struct nand_page_table_entry {
    u32 pageaddr1;
    u32 pageaddr2;
    u32 checksum2;
    u32 reserved;
    u32 checksum1;
};

struct nand_page_table {
    struct nand_page_table_head head;
    struct nand_page_table_entry entry[PAGE_TABLE_MAX_ENTRY];
};
"""


def img_gen_page_table(binfile, cfg, datadir):
    """ Generate page table data
    Args:
        cfg: Dict from JSON
        datadir: working directory for image data
    """
    page_size = 0
    page_cnt = 64

    if "array_organization" in cfg["image"]["info"]["media"]:
        orglist = cfg["image"]["info"]["media"]["array_organization"]
        for item in orglist:
            page_size = int(re.sub(r"[^0-9]", "", item["page"]))
            block_size = int(re.sub(r"[^0-9]", "", item["block"]))

    spl_file = cfg["image"]["target"]["spl"]["file"]
    filesize = round_up(cfg["image"]["target"]["spl"]["filesize"], DATA_ALIGNED_SIZE)
    page_per_blk = block_size // page_size
    page_cnt = filesize // (page_size * 1024)
    if (page_cnt + 1 > (2 * PAGE_TABLE_MAX_ENTRY)):
        print("SPL too large, more than 400K.")
        sys.exit(1)

    path = get_file_path(spl_file, datadir)
    if path is None:
        sys.exit(1)

    step = page_size * 1024

    entry_page = page_cnt + 1
    buff = str_to_nbytes("AICP", 4)
    buff = buff + int_to_uint32_bytes(entry_page)
    buff = buff + int_to_uint16_bytes(page_size * 1024)
    buff = buff + gen_bytes(0xFF, 10)

    with open(path, "rb") as fwcfile:
        pageaddr1 = 0
        pageaddr2 = PAGE_TABLE_MAX_ENTRY

        if (pageaddr1 < PAGE_TABLE_MAX_ENTRY):
            buff = buff + int_to_uint32_bytes(pageaddr1)
        else:
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

        if (pageaddr2 < (2 * PAGE_TABLE_MAX_ENTRY) and pageaddr2 <= (page_cnt + 1)):
            offset2 = (pageaddr2 - 1) * (page_size * 1024)
            fwcfile.seek(offset2, 0)
            bindata = fwcfile.read(step)
            checksum2 = aic_calc_checksum(bindata, page_size * 1024)

            buff = buff + int_to_uint32_bytes(pageaddr2)
            buff = buff + int_to_uint32_bytes(checksum2)
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)
        else:
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

        if (pageaddr1 < PAGE_TABLE_MAX_ENTRY):
            buff = buff + int_to_uint32_bytes(0)
        else:
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

        for i in range(1, PAGE_TABLE_MAX_ENTRY):
            pageaddr1 = i
            pageaddr2 = PAGE_TABLE_MAX_ENTRY + i

            if (pageaddr1 < PAGE_TABLE_MAX_ENTRY and pageaddr1 <= (page_cnt + 1)):
                buff = buff + int_to_uint32_bytes(pageaddr1)
            else:
                buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

            if (pageaddr2 < (2 * PAGE_TABLE_MAX_ENTRY) and pageaddr2 <= (page_cnt + 1)):
                offset2 = (pageaddr2 - 1) * (page_size * 1024)
                fwcfile.seek(offset2, 0)
                bindata = fwcfile.read(step)
                checksum2 = aic_calc_checksum(bindata, page_size * 1024)

                buff = buff + int_to_uint32_bytes(pageaddr2)
                buff = buff + int_to_uint32_bytes(checksum2)
                buff = buff + int_to_uint32_bytes(0xFFFFFFFF)
            else:
                buff = buff + int_to_uint32_bytes(0xFFFFFFFF)
                buff = buff + int_to_uint32_bytes(0xFFFFFFFF)
                buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

            if (pageaddr1 < PAGE_TABLE_MAX_ENTRY):
                offset1 = (pageaddr1 - 1) * (page_size * 1024)
                fwcfile.seek(offset1, 0)
                bindata = fwcfile.read(step)
                checksum1 = aic_calc_checksum(bindata, page_size * 1024)

                buff = buff + int_to_uint32_bytes(checksum1)
            else:
                buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

    buff = buff + gen_bytes(0xFF, page_size * 1024 - len(buff))
    checksum = aic_calc_checksum(buff, page_size * 1024)
    buff = buff[0:36] + int_to_uint32_bytes(checksum) + buff[40:]

    binfile.seek(0, 0)
    binfile.write(buff)
    binfile.flush()

    if VERBOSE:
        print("\tPage table is generated.")

    return 0


def check_partition_exist(table, partval):
    if isinstance(partval, list):
        for item in partval:
            if item.find(":") > 0: # UBI Volume
                parts = item.split(":")
                part = parts[0]
                vol = parts[1]
                if part not in table:
                    print("{} not in table {}".format(part, table))
                    return False

                if "ubi" in table[part]:
                    if vol not in table[part]["ubi"]:
                        print("{} not in ubi {}".format(vol, table[part]["ubi"]))
                        return False
                elif "nftl" in table[part]:
                    if vol not in table[part]["nftl"]:
                        print("{} not in nftl {}".format(vol, table[part]["nftl"]))
                        return False
                else:
                    print("{} not in {}".format(vol, table[part]))
                    return False
            else:
                if item not in table:
                    print("{} not in table {}".format(partval, table))
                    return False
    else:
        if partval not in table:
            print("{} not in table {}".format(partval, table))
            return False
    return True


def img_write_fwc_meta_section(imgfile, cfg, sect, meta_off, file_off, datadir):
    if sect == "extension" and SIGN:
        return (meta_off, file_off)
    if sect in cfg["image"].keys():
        fwcset = cfg["image"][sect]
    else:
        return (meta_off, file_off)

    media_type = cfg["image"]["info"]["media"]["type"]

    if media_type not in cfg:
        print("Cannot find partitions for {}".format(media_type))
        return (-1, -1)
    partitions = cfg[media_type]["partitions"]
    for fwc in fwcset:
        file_size = fwcset[fwc]["filesize"]
        if sect == "target":
            if "part_size" not in fwcset[fwc]:
                print("There is no partition for component '{}', please remove it.".format(fwc))
                continue
            part_size = fwcset[fwc]["part_size"]
            if file_size > part_size:
                print("{} file_size: {} is over much than part_size: {}".format(fwcset[fwc]["file"],
                                                                                hex(file_size),
                                                                                hex(part_size)))
                return (-1, -1)
        if file_size <= 0:
            continue
        imgfile.seek(meta_off, 0)
        path = str(datadir + fwcset[fwc]["file"])
        crc = calc_crc32(path, 0)
        if "ram" in fwcset[fwc]:
            ram = int(fwcset[fwc]["ram"], 16)
        else:
            ram = 0xFFFFFFFF
        attrval = fwcset[fwc]["attr"]
        if isinstance(attrval, list):
            attr = str(";".join(attrval))
        else:
            attr = str(attrval)
        attr = attr.replace(' ', '')
        name = str("image." + sect + "." + fwc)

        if "part" in fwcset[fwc]:
            partval = fwcset[fwc]["part"]
            if check_partition_exist(partitions, partval) is False:
                print("Partition {} not exist".format(partval))
                return (-1, -1)
            if isinstance(partval, list):
                part = str(";".join(partval))
            else:
                part = str(partval)
        else:
            part = ""
        file_name = fwcset[fwc]["file"]
        meta = img_gen_fwc_meta(name, part, file_off, file_size, crc, ram, attr, file_name)
        imgfile.write(meta)
        fwcset[fwc]["meta_off"] = meta_off
        fwcset[fwc]["file_off"] = file_off
        # Update for next item
        meta_off += META_ALIGNED_SIZE
        file_size = round_up(file_size, DATA_ALIGNED_SIZE)
        file_off += file_size
    return (meta_off, file_off)


def img_write_fwc_meta_to_imgfile(imgfile, cfg, meta_start, file_start, datadir):
    """ Generate and write FW component's meta data
    Args:
        imgfile: Image file handle
        cfg: Dict from JSON
        meta_start: meta data area start offset
        file_start: file data area start offset
        datadir: working directory
    """
    meta_offset = meta_start
    file_offset = file_start
    if VERBOSE:
        print("\tMeta data for image components:")
    # 1, FWC of updater
    meta_offset, file_offset = img_write_fwc_meta_section(imgfile, cfg, "updater",
                                                          meta_offset, file_offset,
                                                          datadir)
    if meta_offset < 0:
        return -1
    # 2, Image Info(The same with image header)
    imgfile.seek(meta_offset, 0)
    img_fn = datadir + img_gen_fw_file_name(cfg);
    crc = calc_crc32(img_fn, DATA_ALIGNED_SIZE)
    info_offset = 0 # Image info is the image header, start from 0
    info_size = DATA_ALIGNED_SIZE
    part = ""
    name = "image.info"
    ram = 0xFFFFFFFF
    attr = "required"
    file_name = "info.bin"
    meta = img_gen_fwc_meta(name, part, info_offset, info_size, crc, ram, attr, file_name)
    imgfile.write(meta)
    # Only meta offset increase
    meta_offset += META_ALIGNED_SIZE
    # 3, FWC of target
    meta_offset, file_offset = img_write_fwc_meta_section(imgfile, cfg, "target",
                                                          meta_offset, file_offset,
                                                          datadir)
    if meta_offset < 0:
        return -1
    # 4, FWC of extension
    meta_offset, file_offset = img_write_fwc_meta_section(imgfile, cfg, "extension",
                                                          meta_offset, file_offset,
                                                          datadir)
    if meta_offset < 0:
        return -1
    imgfile.flush()
    return 0


def img_write_fwc_file_to_imgfile(imgfile, cfg, file_start, datadir):
    """ Write FW component's file data
    Args:
        imgfile: Image file handle
        cfg: Dict from JSON
        file_start: file data area start offset
        datadir: working directory
    """
    file_offset = file_start
    if VERBOSE:
        print("\tPacking file data:")
    for section in ["updater", "target", "extension"]:
        if section == "extension" and SIGN:
            continue
        if section in cfg["image"].keys():
            fwcset = cfg["image"][section]
        else:
            continue
        for fwc in fwcset:
            path = get_file_path(fwcset[fwc]["file"], datadir)
            if path is None:
                continue
            if VERBOSE:
                print("\t\t" + os.path.split(path)[1])
            # Read fwc file content, and write to image file
            imgfile.seek(file_offset, 0)
            step = 16 * 1024
            with open(path, "rb") as fwcfile:
                while True:
                    bindata = fwcfile.read(step)
                    if not bindata:
                        break
                    imgfile.write(bindata)
            # Update for next file
            filesize = fwcset[fwc]["filesize"]
            filesize = round_up(filesize, DATA_ALIGNED_SIZE)
            file_offset += filesize
    imgfile.flush()
    return 0


BIN_FILE_MAX_SIZE = 300 * 1024 * 1024


def img_write_fwc_file_to_binfile(binfile, cfg, datadir):
    """ Write FW component's file data
    Args:
        imgfile: Image bin file handle
        cfg: Dict from JSON
        file_start: file data area start offset
        datadir: working directory
    """
    page_size = 0
    block_size = 0
    media_size = 0

    if "array_organization" in cfg["image"]["info"]["media"]:
        orglist = cfg["image"]["info"]["media"]["array_organization"]
        for item in orglist:
            page_size = int(re.sub(r"[^0-9]", "", item["page"]))
            block_size = int(re.sub(r"[^0-9]", "", item["block"]))

    media_type = str(cfg["image"]["info"]["media"]["type"])
    media_size = size_str_to_int(cfg[media_type]["size"])

    if (media_size > BIN_FILE_MAX_SIZE):
        media_size = BIN_FILE_MAX_SIZE

    step = 1024 * 1024
    buff = gen_bytes(0xFF, step)
    while True:
        binfile.write(buff)
        if int(binfile.tell()) >= int(media_size):
            break

    page_table_size = page_size * 1024

    buff = bytes()
    start_block = 0
    last_block = 0
    used_block = 0

    if VERBOSE:
        print("\tPacking file data:")
    for section in ["target"]:
        fwcset = cfg["image"][section]
        for fwc in fwcset:
            path = get_file_path(fwcset[fwc]["file"], datadir)
            if path is None:
                continue
            if path.find(".ubifs") != -1:
                path = path.replace(".ubifs", ".ubi")
                if os.path.exists(path) is False:
                    print("File {} is not exist".format(path))
                    continue
            if VERBOSE:
                print("\t\t" + os.path.split(path)[1])
            # Read fwc file content, and write to image file
            part_offset = fwcset[fwc]["part_offset"]
            part_size = fwcset[fwc]["part_size"]
            part_name = fwcset[fwc]["part"][0]
            filesize = round_up(os.stat(path).st_size, DATA_ALIGNED_SIZE)

            # gen part table
            if fwc == "spl" and cfg["image"]["info"]["media"]["type"] == "spi-nand":
                filesize += page_table_size

            if cfg["image"]["info"]["media"]["type"] == "spi-nand":
                start_block = part_offset // block_size // 1024
                if filesize % (block_size * 1024) != 0:
                    used_block = filesize // block_size // 1024 + 1
                else:
                    used_block = filesize // block_size // 1024
                last_block = start_block + used_block - 1
            elif cfg["image"]["info"]["media"]["type"] == "spi-nor":
                block_size = 64
                start_block = part_offset // block_size // 1024
                if filesize % (block_size * 1024) != 0:
                    used_block = filesize // block_size // 1024 + 1
                else:
                    used_block = filesize // block_size // 1024
                last_block = start_block + used_block - 1
            elif cfg["image"]["info"]["media"]["type"] == "mmc":
                block_size = 512
                start_block = part_offset // block_size
                if filesize % (block_size * 1024) != 0:
                    used_block = filesize // block_size + 1
                else:
                    used_block = filesize // block_size
                last_block = start_block + used_block - 1

            buff = buff + int_to_uint32_bytes(start_block)
            buff = buff + int_to_uint32_bytes(last_block)
            buff = buff + int_to_uint32_bytes(used_block)
            buff = buff + int_to_uint32_bytes(0xFFFFFFFF)

            if fwc == "spl" and cfg["image"]["info"]["media"]["type"] == "spi-nand":
                part_offset += page_table_size
                filesize -= page_table_size
            binfile.seek(part_offset, 0)
            step = 1024 * 1024
            with open(path, "rb") as fwcfile:
                while True:
                    bindata = fwcfile.read(step)
                    if not bindata:
                        break
                    binfile.write(bindata)
            binfile.seek(part_offset + filesize, 0)
            if (part_size - filesize < 0):
                print("file {} size({}) exceeds {} partition size({})".format(fwcset[fwc]["file"],
                                                                              filesize,
                                                                              part_name,
                                                                              part_size))
                sys.exit(1)

            remain = part_size - filesize
            while remain > 0:
                if (remain < step):
                    binfile.write(gen_bytes(0xFF, remain))
                    remain -= remain
                else:
                    binfile.write(gen_bytes(0xFF, step))
                    remain -= step
    binfile.flush()

    buff = buff + gen_bytes(0xFF, 16)
    part_table_file = datadir + "burner/" + "{}".format(cfg["image"]["part_table"])
    with open(part_table_file, "wb") as partfile:
        partfile.write(buff)
        partfile.flush()

    return 0


def img_get_fwc_file_size(cfg, datadir):
    """ Scan directory and get Firmware component's file size, update to cfg
    Args:
        cfg: Dict from JSON
        datadir: working directory for image data
    """
    for section in ["extension", "updater", "target"]:
        if section == "extension" and SIGN:
            continue
        if section in cfg["image"].keys():
            fwcset = cfg["image"][section]
        else:
            continue
        for fwc in fwcset:
            path = get_file_path(fwcset[fwc]["file"], datadir)
            if path is None:
                attr = fwcset[fwc]["attr"]
                if "required" in attr:
                    print("Error, file {} is not exist".format(fwcset[fwc]["file"]))
                    return -1
                else:
                    # FWC file is not exist, but it is not necessary
                    fwcset[fwc]["filesize"] = 0
                    continue
            statinfo = os.stat(path)
            fwcset[fwc]["filesize"] = statinfo.st_size
    return 0


def img_get_part_size(cfg, datadir):
    part_name = ""
    part_size = 0
    part_offs = 0
    total_siz = 0

    fwcset = cfg["image"]["target"]
    media_type = cfg["image"]["info"]["media"]["type"]
    if media_type == "spi-nand" or media_type == "spi-nor":
        total_siz = size_str_to_int(cfg[media_type]["size"])
        partitions = cfg[media_type]["partitions"]
        if len(partitions) == 0:
            print("Partition table is empty")
            sys.exit(1)

        for part in partitions:
            if "size" not in partitions[part]:
                print("No size value for partition: {}".format(part))
            # get part size
            part_size = size_str_to_int(partitions[part]["size"])
            if partitions[part]["size"] == "-":
                part_size = total_siz - part_offs
            if "offset" in partitions[part]:
                part_offs = size_str_to_int(partitions[part]["offset"])
            if "ubi" in partitions[part]:
                volumes = partitions[part]["ubi"]
                if len(volumes) == 0:
                    print("Volume of {} is empty".format(part))
                    sys.exit(1)
                for vol in volumes:
                    if "size" not in volumes[vol]:
                        print("No size value for ubi volume: {}".format(vol))
                    vol_size = size_str_to_int(volumes[vol]["size"])
                    if volumes[vol]["size"] == "-":
                        vol_size = part_size
                    if "offset" in volumes[vol]:
                        part_offs = size_str_to_int(volumes[vol]["offset"])
                    part_name = part + ":" + vol
                    for fwc in fwcset:
                        if fwcset[fwc]["part"][0] == part_name:
                            fwcset[fwc]["part_size"] = vol_size
                            fwcset[fwc]["part_offset"] = part_offs
                    part_offs += vol_size
            else:
                part_name = part
                for fwc in fwcset:
                    if fwcset[fwc]["part"][0] == part_name:
                        fwcset[fwc]["part_size"] = part_size
                        fwcset[fwc]["part_offset"] = part_offs
                part_offs += part_size
    elif media_type == "mmc":
        total_siz = size_str_to_int(cfg[media_type]["size"])
        partitions = cfg[media_type]["partitions"]
        if len(partitions) == 0:
            print("Partition table is empty")
            sys.exit(1)
        for part in partitions:
            if "size" not in partitions[part]:
                print("No size value for partition: {}".format(part))
            # get part size
            part_size = size_str_to_int(partitions[part]["size"])
            if partitions[part]["size"] == "-":
                part_size = total_siz - part_offs
            if "offset" in partitions[part]:
                part_offs = size_str_to_int(partitions[part]["offset"])
            part_name = part
            for fwc in fwcset:
                if fwcset[fwc]["part"][0] == part_name:
                    fwcset[fwc]["part_size"] = part_size
                    fwcset[fwc]["part_offset"] = part_offs
            part_offs += part_size
    else:
        print("Not supported media type: {}".format(media_type))
        sys.exit(1)

    return 0


def round_up(x, y):
    return int((x + y - 1) / y) * y


def aic_create_parts_for_env(cfg):
    mtd = ""
    ubi = ""
    gpt = ""

    part_str = ""
    media_type = cfg["image"]["info"]["media"]["type"]
    if media_type == "spi-nand" or media_type == "spi-nor":
        partitions = cfg[media_type]["partitions"]
        mtd = "spi{}.0:".format(cfg["image"]["info"]["media"]["device_id"])
        if len(partitions) == 0:
            print("Partition table is empty")
            sys.exit(1)
        for part in partitions:
            itemstr = ""
            if "size" not in partitions[part]:
                print("No size value for partition: {}".format(part))
            itemstr += partitions[part]["size"]
            if "offset" in partitions[part]:
                itemstr += "@{}".format(partitions[part]["offset"])
            itemstr += "({})".format(part)
            mtd += itemstr + ","
            if "ubi" in partitions[part]:
                volumes = partitions[part]["ubi"]
                if len(volumes) == 0:
                    print("Volume of {} is empty".format(part))
                    sys.exit(1)
                ubi += "{}:".format(part)
                for vol in volumes:
                    itemstr = ""
                    if "size" not in volumes[vol]:
                        print("No size value for ubi volume: {}".format(vol))
                    itemstr += volumes[vol]["size"]
                    if "offset" in volumes[vol]:
                        itemstr += "@{}".format(volumes[vol]["offset"])
                    itemstr += "({})".format(vol)
                    ubi += itemstr + ","
                ubi = ubi[0:-1] + ";"
        mtd = mtd[0:-1]
        part_str = "MTD={}\n".format(mtd)
        if len(ubi) > 0:
            ubi = ubi[0:-1]
            part_str += "UBI={}\n".format(ubi)
    elif media_type == "mmc":
        partitions = cfg[media_type]["partitions"]
        if len(partitions) == 0:
            print("Partition table is empty")
            sys.exit(1)
        for part in partitions:
            itemstr = ""
            if "size" not in partitions[part]:
                print("No size value for partition: {}".format(part))
            itemstr += partitions[part]["size"]
            if "offset" in partitions[part]:
                itemstr += "@{}".format(partitions[part]["offset"])
            itemstr += "({})".format(part)
            gpt += itemstr + ","
        gpt = gpt[0:-1]
        part_str = "GPT={}\nparts_mmc={}\n".format(gpt, gpt)
        # parts_mmc will be deleted later, keep it just for old version AiBurn tool
    else:
        print("Not supported media type: {}".format(media_type))
        sys.exit(1)

    return part_str


def uboot_env_create_image(srcfile, outfile, size, part_str, redund, script_dir):
    tmpfile = srcfile + ".part.tmp"
    fs = open(srcfile, "r+")
    envstr = fs.readlines()
    fs.close()
    fp = open(tmpfile, "w+")
    fp.write(part_str)
    fp.writelines(envstr)
    fp.close()

    if "enable" in redund:
        cmd = ["mkenvimage", "-r","-s", str(size), "-o", outfile, tmpfile]
    else:
        cmd = ["mkenvimage", "-s", str(size), "-o", outfile, tmpfile]
    ret = subprocess.run(cmd, subprocess.PIPE)
    if ret.returncode != 0:
        sys.exit(1)


def get_pre_process_cfg(cfg):
    if "temporary" in cfg:
        return cfg["temporary"]
    elif "pre-process" in cfg:
        return cfg["pre-process"]
    return None


def make_tar(datadir, exfiles, exdirs, out_file):
    with tarfile.open(out_file, "w") as t:
        for filename in exfiles:
            pathfile = os.path.join(datadir, filename)
            t.add(pathfile, filename)
        for dirname in exdirs:
            pathdir = os.path.join(datadir, dirname)
            t.add(pathdir, dirname)


def run_shell(cmd):
    # Only array format commands are supported
    ret = subprocess.run(cmd, text=True, stdout=subprocess.PIPE)
    if ret.returncode != 0:
        sys.exit(1)
    return ret.stdout


def run_shell_str(cmd):
    # Support for string format commands
    ret = subprocess.run(cmd, shell=True, text=True, stdout=subprocess.PIPE)
    if ret.returncode != 0:
        sys.exit(1)
    return ret.stdout


def rootfs_signature_deal(cfg, datadir, dtbfile, data):
    # Root hash signature and set parameters to dtb file
    # Deal hash table header
    data = data.split("\n")[1:-1]
    data = dict(re.split(': +\t', x) for x in data)
    data_sectors = int(int(data['Data blocks']) * int(data['Data block size']) / 512)

    if "priv" in cfg and "cert" in cfg:
        privname = cfg["priv"]
        certname = cfg["cert"]
        hashtxt = datadir + 'roothash.txt'
        hashsigned = datadir + "roothash.txt.signed"

        privfile = get_file_path(privname, datadir)
        if privfile is None:
            print("File {} is not exist".format(privname))
            sys.exit(1)
        certfile = get_file_path(certname, datadir)
        if certfile is None:
            print("File {} is not exist".format(certname))
            sys.exit(1)

        # Save Root hash value
        run_shell_str(["echo {} | tr -d '\n' > {}".format(data['Root hash'], hashtxt)])

        # Signature
        run_shell(["openssl", "smime", "-sign", "-nocerts", "-noattr", "-binary", "-in", hashtxt, "-inkey", privfile, "-signer", certfile, "-outform", "der", "-out", hashsigned])

        # Set dm-mod.create parameters
        str_list = []
        hexhashsigned = run_shell_str(["xxd -p {} | tr -d '\n'".format(hashsigned)])
        str_list.append("dm-mod.create=\"dm-0,,,ro,0")
        str_list.append(data_sectors)
        str_list.append("verity 1")
        str_list.append("data_dev")
        str_list.append("hash_dev")
        str_list.append(data['Data block size'])
        str_list.append(data['Hash block size'])
        str_list.append(data['Data blocks'])
        str_list.append(data['Hash type'])
        str_list.append(data['Hash algorithm'])
        str_list.append(data['Root hash'])
        str_list.append(data['Salt'])
        str_list.append("3 ignore_zero_blocks root_hash_sig_hex")
        str_list.append(hexhashsigned + "\"")
        create = " ".join(str(i) for i in str_list)
        run_shell(["fdtput", dtbfile, "/chosen", "dm-mod.create", create, "-ts"])


def firmware_component_preproc_hash_table(cfg, datadir, keydir):
    preproc_cfg = get_pre_process_cfg(cfg)
    # Need to generate hash table
    imgnames = preproc_cfg["hash_table"].keys()
    for name in imgnames:
        rootfsname = preproc_cfg["hash_table"][name]["file"]
        dtbname = preproc_cfg["hash_table"][name]["dtb"]
        outfile = datadir + name
        datasize = 1024
        hashsize = 4096

        if VERBOSE:
            print("\tCreating {} ...".format(outfile))
        rootfsfile = get_file_path(rootfsname, datadir)
        if rootfsfile is None:
            print("File {} is not exist".format(rootfsname))
            sys.exit(1)
        dtbfile = get_file_path(dtbname, datadir)
        if dtbfile is None:
            print("File {} is not exist".format(dtbname))
            sys.exit(1)

        # Build hash table
        imgcfg = preproc_cfg["hash_table"][name]
        if "datasize" in imgcfg.keys():
            datasize = int(imgcfg["datasize"])
        if "hashsize" in imgcfg.keys():
            hashsize = int(imgcfg["hashsize"])

        data = run_shell(["veritysetup",
                          "format",
                          "--data-block-size={}".format(datasize),
                          "--hash-block-size={}".format(hashsize),
                          rootfsfile, outfile])

        rootfs_signature_deal(imgcfg, datadir, dtbfile, data)

        # Set dm-mod.waitfor parameters
        root_part = rootfsname.split('.')[0]
        hash_part = name.split('.')[1]
        waitfor = "dm-mod.waitfor=PARTLABEL=" + root_part + ",PARTLABEL=" + hash_part
        run_shell(["fdtput", dtbfile, "/chosen", "dm-mod.waitfor", waitfor, "-ts"])

        # Set other bootargs parameters
        str_list = []
        rootfstype = "rootfstype=" + rootfsname.split('.')[1]
        root = "root=/dev/dm-0"

        if rootfsname.split('.')[1] == "ext4" or rootfsname.split('.')[1] == "ext2":
            str_list.append("rootwait")
        str_list.append(rootfstype)
        str_list.append(root)
        str_list.append("dm_verity.require_signature=1")
        args = " ".join(str(i) for i in str_list)
        run_shell(["fdtput", dtbfile, "/chosen", "args", args, "-ts"])

        # Set other parameters
        run_shell(["fdtput", dtbfile, "/chosen", "root_part", root_part, "-ts"])
        run_shell(["fdtput", dtbfile, "/chosen", "hash_part", hash_part, "-ts"])


def firmware_component_preproc_itb(cfg, datadir, keydir, bindir):
    # Need to generate FIT image
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["itb"].keys()
    for itbname in imgnames:
        itsname = preproc_cfg["itb"][itbname]["its"]
        outfile = datadir + itbname
        dtbfile = None
        keypath = None

        if VERBOSE:
            print("\tCreating {} ...".format(outfile))
        srcfile = get_file_path(itsname, datadir)
        if srcfile is None:
            print("File {} is not exist".format(itsname))
            sys.exit(1)
        if "dtb" in preproc_cfg["itb"][itbname].keys():
            dtbname = preproc_cfg["itb"][itbname]["dtb"]
            dtbfile = get_file_path(dtbname, datadir)
            if dtbfile is None:
                print("File {} is not exist".format(dtbname))
                sys.exit(1)
        if "keydir" in preproc_cfg["itb"][itbname].keys():
            keydir = preproc_cfg["itb"][itbname]["keydir"]
            keypath = get_file_path(keydir, datadir)
            if keypath is None:
                print("File {} is not exist".format(keydir))

        itb_create_image(srcfile, outfile, keypath, dtbfile, bindir)

        # Generate a spl image with spl dtb file
        if "bin" in preproc_cfg["itb"][itbname].keys():
            srcbin = preproc_cfg["itb"][itbname]["bin"]["src"]
            dstbin = preproc_cfg["itb"][itbname]["bin"]["dst"]
            srcfile = get_file_path(srcbin, datadir)
            if srcfile is None:
                print("File {} is not exist".format(srcbin))
                sys.exit(1)
            dstfile = get_file_path(dstbin, datadir)
            if dstfile is None:
                print("File {} is not exist".format(dstbin))
                sys.exit(1)
            cmd = ["cat {} {} > {}".format(srcfile, dtbfile, dstfile)]
            ret = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE)
            if ret.returncode != 0:
                sys.exit(1)


def firmware_component_preproc_uboot_env(cfg, datadir, keydir, bindir):
    # Need to generate uboot env bin
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["uboot_env"].keys()
    part_str = aic_create_parts_for_env(cfg)
    envredund = "disable"
    for binname in imgnames:
        envfile = preproc_cfg["uboot_env"][binname]["file"]
        envsize = preproc_cfg["uboot_env"][binname]["size"]
        if "redundant" in preproc_cfg["uboot_env"][binname]:
            envredund = preproc_cfg["uboot_env"][binname]["redundant"]
        outfile = datadir + binname
        if VERBOSE:
            print("\tCreating {} ...".format(outfile))
        srcfile = get_file_path(envfile, datadir)
        if srcfile is None:
            print("File {} is not exist".format(envfile))
            sys.exit(1)
        uboot_env_create_image(srcfile, outfile, envsize, part_str,
                               envredund, bindir)


def firmware_component_preproc_aicboot(cfg, datadir, keydir, bindir):
    # Legacy code, should not use after Luban-Lite 1.0.6 and Luban SDK 1.2.5
    # Need to generate aicboot image
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["aicboot"].keys()
    for name in imgnames:
        imgcfg = preproc_cfg["aicboot"][name]
        imgcfg["keydir"] = keydir
        imgcfg["datadir"] = datadir
        outname = datadir + name
        if VERBOSE:
            print("\tCreating {} ...".format(outname))
        imgbytes = aic_boot_create_image(imgcfg, keydir, datadir)

        if check_loader_run_in_dram(imgcfg):
            extimgbytes = aic_boot_create_ext_image(imgcfg, keydir, datadir)
            padlen = round_up(len(imgbytes), META_ALIGNED_SIZE) - len(imgbytes)
            if padlen > 0:
                imgbytes += bytearray(padlen)
            imgbytes += extimgbytes
            # For Debug
            # with open(outname + ".ext", "wb") as f:
            #     f.write(extimgbytes)

        with open(outname, "wb") as f:
            f.write(imgbytes)


def firmware_component_preproc_aicimage(cfg, datadir, keydir, bindir):
    # Need to generate aicboot image
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["aicimage"].keys()
    for name in imgnames:
        imgcfg = preproc_cfg["aicimage"][name]
        imgcfg["keydir"] = keydir
        imgcfg["datadir"] = datadir
        outname = datadir + name
        if VERBOSE:
            print("\tCreating {} ...".format(outname))
        imgbytes = aic_boot_create_image_v2(imgcfg, keydir, datadir)
        with open(outname, "wb") as f:
            f.write(imgbytes)


def firmware_component_preproc_spienc(cfg, datadir, keydir, bindir):
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["spienc"].keys()
    for name in imgnames:
        imgcfg = preproc_cfg["spienc"][name]
        imgcfg["keydir"] = keydir
        imgcfg["datadir"] = datadir
        outname = datadir + name
        imgcfg["input"] = datadir + imgcfg["file"]
        imgcfg["output"] = outname
        if VERBOSE:
            print("\tCreating {} ...".format(outname))
        spienc_create_image(imgcfg, bindir)


def firmware_component_preproc_concatenate(cfg, datadir, keydir, bindir):
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["concatenate"].keys()
    for name in imgnames:
        outname = datadir + name
        if VERBOSE:
            print("\tCreating {} ...".format(outname))
        flist = preproc_cfg["concatenate"][name]
        if isinstance(flist, list):
            concatenate_create_image(outname, flist, datadir)
        else:
            print("\tWarning: {} in \'concatenate' is not list".format(name))
            continue


def firmware_component_preproc_extension(cfg, datadir, keydir):
    # Need to compress source file
    preproc_cfg = get_pre_process_cfg(cfg)
    imgnames = preproc_cfg["extension"].keys()
    for name in imgnames:
        outname = datadir + name
        exfiles = preproc_cfg["extension"][name]["exfiles"]
        exdirs = preproc_cfg["extension"][name]["exdirs"]
        make_tar(datadir, exfiles, exdirs, outname)


def firmware_component_preproc(cfg, datadir, keydir, bindir):
    """ Perform firmware component pre-process
    Args:
        cfg: Dict from JSON
        datadir: working directory for image data
        keydir: key material directory for image data
    """
    preproc_cfg = get_pre_process_cfg(cfg)
    if preproc_cfg is None:
        return None
    if "hash_table" in preproc_cfg and SIGN:
        firmware_component_preproc_hash_table(cfg, datadir, keydir)
    if "itb" in preproc_cfg:
        firmware_component_preproc_itb(cfg, datadir, keydir, bindir)
    if "uboot_env" in preproc_cfg:
        firmware_component_preproc_uboot_env(cfg, datadir, keydir, bindir)
    if "aicboot" in preproc_cfg:
        # Legacy code
        firmware_component_preproc_aicboot(cfg, datadir, keydir, bindir)
    if "aicimage" in preproc_cfg:
        firmware_component_preproc_aicimage(cfg, datadir, keydir, bindir)
    if "spienc" in preproc_cfg:
        firmware_component_preproc_spienc(cfg, datadir, keydir, bindir)
    if "concatenate" in preproc_cfg:
        firmware_component_preproc_concatenate(cfg, datadir, keydir, bindir)


def generate_bootcfg(bcfgfile, cfg):
    comments = ["# Boot configuration file\n",
                "# Used in SD Card FAT32 boot and USB Disk upgrade.\n",
                "# Format:\n",
                "# protection=part1 name,part2 name,part3 name\n",
                "#   Protects partitions from being overwritten when they are upgraded.\n"
                "# boot0=size@offset\n",
                "#   boot0 size and location offset in 'image' file, boot rom read it.\n"
                "# boot0=example.bin\n",
                "#   boot0 image is file example.bin, boot rom read it.\n"
                "# boot1=size@offset\n",
                "#   boot1 size and location offset in 'image' file, boot0 read it.\n"
                "# boot1=example.bin\n",
                "#   boot1 image is file example.bin, boot0 read it.\n"
                "# image=example.img\n",
                "#   Packed image file is example.img, boot1 use it.\n",
                "\n\n",
                ]
    bytes_comments = [comment.encode() for comment in comments]
    bcfgfile.writelines(bytes_comments)

    fwcset = cfg["image"]["updater"]
    fwckeys = cfg["image"]["updater"].keys()
    if "spl" in fwckeys:
        fwcname = "spl"
        linestr = "# {}\n".format(fwcset[fwcname]["file"])
        bcfgfile.write(linestr.encode())
        linestr = "boot0={}@{}\n".format(hex(fwcset[fwcname]["filesize"]),
                                         hex(fwcset[fwcname]["file_off"]))
        bcfgfile.write(linestr.encode())

    if "uboot" in fwckeys:
        fwcname = "uboot"
        linestr = "# {}\n".format(fwcset[fwcname]["file"])
        bcfgfile.write(linestr.encode())
        linestr = "boot1={}@{}\n".format(hex(fwcset[fwcname]["filesize"]),
                                         hex(fwcset[fwcname]["file_off"]))
        bcfgfile.write(linestr.encode())

    if "env" in fwckeys:
        fwcname = "env"
        linestr = "# {}\n".format(fwcset[fwcname]["file"])
        bcfgfile.write(linestr.encode())
        linestr = "env={}@{}\n".format(hex(fwcset[fwcname]["filesize"]),
                                       hex(fwcset[fwcname]["file_off"]))
        bcfgfile.write(linestr.encode())

    if "logo" in fwckeys:
        fwcname = "logo"
        linestr = "# {}\n".format(fwcset[fwcname]["file"])
        bcfgfile.write(linestr.encode())
        linestr = "logo={}@{}\n".format(hex(fwcset[fwcname]["filesize"]),
                                        hex(fwcset[fwcname]["file_off"]))
        bcfgfile.write(linestr.encode())

    imgfn = img_gen_fw_file_name(cfg)
    linestr = "image={}\n".format(imgfn)
    bcfgfile.write(linestr.encode())
    bcfgfile.flush()


def get_spinand_image_list(cfg, datadir):
    imglist = []
    orglist = cfg["image"]["info"]["media"]["array_organization"]
    for item in orglist:
        paramstr = "_page_{}_block_{}".format(item["page"], item["block"])
        paramstr = paramstr.lower()
        status_ok = True
        for fwcname in cfg["image"]["target"]:
            if "ubi" not in cfg["image"]["target"][fwcname]["attr"]:
                # Not UBI partition
                continue
            # UBI component
            filepath = cfg["image"]["target"][fwcname]["file"]
            if filepath.find("*") <= 0:
                # No need to check
                continue
            filepath = filepath.replace("*", paramstr)
            filepath = get_file_path(filepath, datadir)
            if filepath is None and "optional" not in cfg["image"]["target"][fwcname]["attr"]:
                # FWC file not exist
                status_ok = False
                print("{} is not found".format(cfg["image"]["target"][fwcname]["file"]))
                break
            backup = cfg["image"]["target"][fwcname]["file"]
            # Backup the original file path string, because it will be modified
            # when generating image
            cfg["image"]["target"][fwcname]["file.backup"] = backup
        if status_ok:
            imglist.append(paramstr)
    backup = cfg["image"]["info"]["product"]
    cfg["image"]["info"]["product.backup"] = backup
    backup = cfg["image"]["bootcfg"]
    cfg["image"]["bootcfg.backup"] = backup
    backup = cfg["image"]["part_table"]
    cfg["image"]["part_table.backup"] = backup
    return imglist, orglist


def fixup_spinand_ubi_fwc_name(cfg, paramstr, orgitem):
    for fwcname in cfg["image"]["target"]:
        if "ubi" not in cfg["image"]["target"][fwcname]["attr"]:
            # Not UBI partition
            continue
        # UBI component
        filepath = cfg["image"]["target"][fwcname]["file.backup"]
        if filepath.find("*") <= 0:
            # No need to fixup
            continue
        cfg["image"]["target"][fwcname]["file"] = filepath.replace("*", paramstr)
    # fixup others
    backup = cfg["image"]["info"]["product.backup"]
    cfg["image"]["info"]["product"] = backup + paramstr
    backup = cfg["image"]["bootcfg.backup"]
    cfg["image"]["bootcfg"] = "{}({})".format(backup, paramstr[1:])
    backup = cfg["image"]["part_table.backup"]
    cfg["image"]["part_table"] = "{}({})".format(backup, paramstr[1:])
    cfg["image"]["info"]["media"]["array_organization"] = [orgitem]


def build_firmware_image(cfg, datadir, outdir):
    """ Build firmware image
    Args:
        cfg: Dict from JSON
        datadir: working directory for image data
    """
    # Step0: Get all part size
    ret = img_get_part_size(cfg, datadir)
    if ret != 0:
        return ret

    # Step1: Get all FWC file's size
    ret = img_get_fwc_file_size(cfg, datadir)
    if ret != 0:
        return ret

    # Step2: Calculate Meta Area's size, one FWC use DATA_ALIGNED_SIZE bytes
    meta_area_size = 0
    for s in ["updater", "target", "extension"]:
        if s == "extension" and SIGN:
            continue
        if s in cfg["image"].keys():
            fwcset = cfg["image"][s]
        else:
            continue
        for fwc in fwcset:
            if fwcset[fwc]["filesize"] > 0:
                meta_area_size += META_ALIGNED_SIZE
    # Image header is also one FWC, it need one FWC Meta
    meta_area_size += META_ALIGNED_SIZE

    # Step3: Calculate the size of FWC File Data Area
    file_area_size = 0
    for s in ["updater", "target", "extension"]:
        if s == "extension" and SIGN:
            continue
        if s in cfg["image"].keys():
            for fwc in cfg["image"][s]:
                if "filesize" in cfg["image"][s][fwc] is False:
                    return -1
                filesize = cfg["image"][s][fwc]["filesize"]
                if filesize > 0:
                    filesize = round_up(filesize, DATA_ALIGNED_SIZE)
                    file_area_size += filesize

    # Step5: Calculate the size of Extension File Data Area
    ex_area_size = 0
    ret = img_get_fwc_file_size(cfg, datadir)
    if ret != 0:
        return ret
    for s in ["extension"]:
        if s == "extension" and SIGN:
            continue
        if s in cfg["image"].keys():
            for fwc in cfg["image"][s]:
                if "filesize" in cfg["image"][s][fwc] is False:
                    return -1
                filesize = cfg["image"][s][fwc]["filesize"]
                if filesize > 0:
                    filesize = round_up(filesize, DATA_ALIGNED_SIZE)
                    ex_area_size += filesize

    # Step6: Create empty image file
    img_fn = datadir + img_gen_fw_file_name(cfg)
    img_total_size = DATA_ALIGNED_SIZE # Header
    img_total_size += meta_area_size
    img_total_size += file_area_size
    with open(img_fn, 'wb') as imgfile:
        imgfile.truncate(img_total_size)
        # Step7: Write header
        ret = img_write_fw_header(imgfile, cfg, meta_area_size, file_area_size, ex_area_size)
        if ret != 0:
            return ret
        # Step8: Write FW Component meta to image
        meta_start = DATA_ALIGNED_SIZE
        fwc_file_start = meta_start + meta_area_size
        ret = img_write_fwc_meta_to_imgfile(imgfile, cfg, meta_start,
                                            fwc_file_start, datadir)
        if ret != 0:
            return ret
        # Step9: Write FW Component file data to image
        ret = img_write_fwc_file_to_imgfile(imgfile, cfg, fwc_file_start, datadir)
        if ret != 0:
            return ret

        imgfile.flush()

    abspath = "{}".format(img_fn)
    (img_path, img_name) = os.path.split(abspath)
    if VERBOSE:
        print("\tImage file is generated: {}/{}\n\n".format(img_path, img_name))

    if BURNER:
        os.makedirs(outdir + "burner/", exist_ok=True)
        img_bin_fn = outdir + "burner/" + img_gen_fw_file_name(cfg).replace(".img", ".bin")
        with open(img_bin_fn, 'wb') as binfile:
            # Only spi-nand need gen page table
            if cfg["image"]["info"]["media"]["type"] == "spi-nand":
                ret = img_gen_page_table(binfile, cfg, datadir)
                if ret != 0:
                    return ret

            ret = img_write_fwc_file_to_binfile(binfile, cfg, datadir)
            if ret != 0:
                return ret

        abspath = "{}".format(img_bin_fn)
        (img_path, img_name) = os.path.split(abspath)
        if VERBOSE:
            print("\tImage bin file is generated: {}/{}\n\n".format(img_path, img_name))

    bootcfg_fn = datadir + cfg["image"]["bootcfg"]
    with open(bootcfg_fn, 'wb') as bcfgfile:
        generate_bootcfg(bcfgfile, cfg)
        bcfgfile.flush()
    # Always set page_2k_block_128k nand image as default
    if "page_2k_block_128k" in bootcfg_fn:
        default_bootcfg_fn = bootcfg_fn.replace('(page_2k_block_128k)', '')
        with open(default_bootcfg_fn, 'wb') as bcfgfile:
            generate_bootcfg(bcfgfile, cfg)
            bcfgfile.flush()

    return 0


def make_untar(src_file, out_dir):
    with tarfile.open(src_file) as t:
        t.extractall(out_dir)
        print("Extract {} file data to {}".format(src_file, out_dir))


def extract_img_meta_data(imgfile, datadir, meta_off):
    magic = str_from_nbytes(imgfile.read(8))
    name = str_from_nbytes(imgfile.read(64))
    partition = str_from_nbytes(imgfile.read(64))
    offset = int_from_uint32_bytes(imgfile.read(4))
    size = int_from_uint32_bytes(imgfile.read(4))
    crc = int_from_uint32_bytes(imgfile.read(4))
    ram = int_from_uint32_bytes(imgfile.read(4))
    attr = str_from_nbytes(imgfile.read(64))
    filename = str_from_nbytes(imgfile.read(64)).strip(b'\x00'.decode())

    imgfile.seek(offset)
    pathfile = datadir + '/' + filename
    with open(pathfile, 'wb') as f:
        f.write(imgfile.read(size))

    if Path(pathfile).suffix == ".tar":
        make_untar(pathfile, datadir)


def extract_img_data(img):
    datadir = os.path.join(os.path.dirname(img), Path(img).stem)
    os.makedirs(datadir, exist_ok=True)
    with open(img, 'rb+') as imgfile:
        magic = str_from_nbytes(imgfile.read(8))
        platform = str_from_nbytes(imgfile.read(64))
        product = str_from_nbytes(imgfile.read(64))
        version = str_from_nbytes(imgfile.read(64))
        media_type = str_from_nbytes(imgfile.read(64))
        media_dev_id = int_from_uint32_bytes(imgfile.read(4))
        nand_array_org = str_from_nbytes(imgfile.read(64))
        meta_offset = int_from_uint32_bytes(imgfile.read(4))
        meta_size = int_from_uint32_bytes(imgfile.read(4))
        file_offset = int_from_uint32_bytes(imgfile.read(4))
        file_size = int_from_uint32_bytes(imgfile.read(4))
        ex_flag = int_from_uint32_bytes(imgfile.read(4))
        ex_offset = int_from_uint32_bytes(imgfile.read(4))
        ex_size = int_from_uint32_bytes(imgfile.read(4))

        count = (int)(meta_size / META_ALIGNED_SIZE)
        for i in range(0, count):
            imgfile.seek(meta_offset)
            extract_img_meta_data(imgfile, datadir, meta_offset)
            meta_offset += META_ALIGNED_SIZE

    return datadir


if __name__ == "__main__":
    default_bin_root = os.path.dirname(sys.argv[0])
    if sys.platform.startswith("win"):
        default_bin_root = os.path.dirname(sys.argv[0]) + "/"
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-d", "--datadir", type=str,
                       help="input image data directory")
    group.add_argument("-i", "--imgfile", type=str,
                       help="input unsigned image file")
    parser.add_argument("-o", "--outdir", type=str,
                        help="output image file dir")
    parser.add_argument("-c", "--config", type=str,
                        help="image configuration file name")
    parser.add_argument("-k", "--keydir", type=str,
                        help="key material directory")
    parser.add_argument("-e", "--extract", action='store_true',
                        help="extract extension file")
    parser.add_argument("-s", "--sign", action='store_true',
                        help="sign image file")
    parser.add_argument("-b", "--burner", action='store_true',
                        help="generate burner format image")
    parser.add_argument("-p", "--preprocess", action='store_true',
                        help="run preprocess only")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="show detail information")
    args = parser.parse_args()
    # If user not specified data directory, use current directory as default
    if args.datadir is None:
        args.datadir = './'
    if args.outdir is None:
        args.outdir = args.datadir
    if args.datadir.endswith('/') is False and args.datadir.endswith('\\') is False:
        args.datadir = args.datadir + '/'
    if args.outdir.endswith('/') is False and args.outdir.endswith('\\') is False:
        args.outdir = args.outdir + '/'
    if args.imgfile:
        args.datadir = extract_img_data(args.imgfile) + '/'
    if args.config is None:
        args.config = args.datadir + "image_cfg.json"
    if args.keydir is None:
        args.keydir = args.datadir
    if args.keydir.endswith('/') is False and args.keydir.endswith('\\') is False:
        args.keydir = args.keydir + '/'
    if args.sign:
        SIGN = True
    if args.burner:
        BURNER = True
    if args.verbose:
        VERBOSE = True
    if args.extract:
        sys.exit(1)
    if args.config is None:
        print('Error, option --config is required.')
        sys.exit(1)
    # pinmux check
    pinmux_check.check_pinmux(args.datadir + "u-boot.dtb")

    cfg = parse_image_cfg(args.config)
    # Pre-process here, e.g: signature, encryption, ...
    if get_pre_process_cfg(cfg) is not None:
        firmware_component_preproc(cfg, args.datadir, args.keydir, default_bin_root)
    if args.preprocess:
        sys.exit(0)

    cfg["image"]["bootcfg"] = "bootcfg.txt"
    cfg["image"]["part_table"] = "image_part_table.bin"
    # Finally build the firmware image
    imglist = []
    if cfg["image"]["info"]["media"]["type"] == "spi-nand":
        imglist, orglist = get_spinand_image_list(cfg, args.datadir)
    if len(imglist) > 0:
        # SPI-NAND UBI case
        for imgitem, orgitem in zip(imglist, orglist):
            # fixup file path
            fixup_spinand_ubi_fwc_name(cfg, imgitem, orgitem)
            ret = build_firmware_image(cfg, args.datadir, args.outdir)
            if ret != 0:
                sys.exit(1)
    else:
        # Just create image, no need to fixup anything
        ret = build_firmware_image(cfg, args.datadir, args.outdir)
        if ret != 0:
            sys.exit(1)
