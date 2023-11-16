#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0+

# Copyright (C) 2021 ArtInChip
# Wu Dehuang
#
# Tool to encrypt RSA private key or public key
#

import sys, os
import binascii
import argparse
from Crypto.Cipher import DES
from Crypto.Cipher import AES
from Crypto.PublicKey import RSA
from Crypto.Util import asn1

def gen_encrypt_rsa_key(args):
    aes_flag = False
    symm_key = args.deskey
    if args.aeskey:
        aes_flag = True
        symm_key = args.aeskey

    try:
        with open(symm_key, 'rb') as fkey:
            keydata = fkey.read()
    except IOError:
        print('Failed to open file: ' + symm_key)
        sys.exit(1)

    if aes_flag:
        if len(keydata) != 16:
            print('AES key is not 128bit.')
            sys.exit(1)
        cipher = AES.new(keydata, AES.MODE_ECB)
    else:
        if len(keydata) != 8:
            print('DES key is not 64bit.')
            sys.exit(1)
        cipher = DES.new(keydata, DES.MODE_ECB)

    try:
        with open(args.rsakey, 'rb') as frsa:
            rsakey = RSA.importKey(frsa.read())
    except IOError:
        print('Failed to open file: ' + args.rsakey)
        sys.exit(1)

    fname, ext = os.path.splitext(args.rsakey)
    if rsakey.size() > 3072:
        keysize = int(4096 / 8)
    elif rsakey.size() > 2048:
        keysize = int(3072 / 8)
    elif rsakey.size() > 1024:
        keysize = int(2048 / 8)
    elif rsakey.size() > 512:
        keysize = int(1024 / 8)
    elif rsakey.size() > 500:
        keysize = int(512 / 8)
    else:
        print('Not supported key size' + str(rsakey.size()))
        sys.exit(1)

    # Encrypt RSA key file
    # ArtInChip's hardware Crypto Engine read little-endian data
    # here translate big-number to little-endian byte stream and encrypt it

    data = rsakey.n.to_bytes(keysize, byteorder='little', signed=False)
    enc_data = cipher.encrypt(data)
    #zbyte = bytes(1) # Make the length of n always greater than other numbers
    aa = 1
    zbyte = aa.to_bytes(1, byteorder='little', signed=False)
    enc_n = enc_data + zbyte
    n2 = int.from_bytes(enc_n, byteorder='little', signed=False)

    data = rsakey.e.to_bytes(keysize, byteorder='little', signed=False)
    enc_data = cipher.encrypt(data)
    e2 = int.from_bytes(enc_data, byteorder='little', signed=False)

    data = rsakey.p.to_bytes(keysize, byteorder='little', signed=False)
    enc_data = cipher.encrypt(data)
    p2 = int.from_bytes(enc_data, byteorder='little', signed=False)

    data = rsakey.q.to_bytes(keysize, byteorder='little', signed=False)
    enc_data = cipher.encrypt(data)
    q2 = int.from_bytes(enc_data, byteorder='little', signed=False)

    if rsakey.has_private():
        data = rsakey.d.to_bytes(keysize, byteorder='little', signed=False)
        enc_data = cipher.encrypt(data)
        d2 = int.from_bytes(enc_data, byteorder='little', signed=False)
        newkey = RSA.construct((n2, e2, d2, p2, q2, rsakey.u))
    else:
        newkey = RSA.construct((n2, e2))
        # PKCS#1 asn.1 format public key
        seq_der = asn1.DerSequence()
        seq_der.append(n2)
        seq_der.append(e2)
        fpkcs1 = fname + '_encrypted_pkcs1.der'
        with open(fpkcs1, 'wb') as fpk:
            fpk.write(seq_der.encode())

    newkeydata = newkey.exportKey('DER')
    fname = fname + '_encrypted.der'
    with open(fname, 'wb') as fder:
        fder.write(newkeydata)

def gen_pkcs1_pub_key(args):
    fname, ext = os.path.splitext(args.rsakey)
    try:
        with open(args.rsakey, 'rb') as frsa:
            rsakey = RSA.importKey(frsa.read())
        # PKCS#1 asn.1 format public key
        seq_der = asn1.DerSequence()
        seq_der.append(rsakey.n)
        seq_der.append(rsakey.e)
    except IOError:
        print('Failed to open file: ' + args.rsakey)
        sys.exit(1)
    try:
        fname = fname + '_pubkey_pkcs1.der'
        with open(fname, 'wb') as fder:
            fder.write(seq_der.encode())
    except IOError:
        print('Failed to open file: ' + fname)
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Tool to encrypt RSA key file by using AES/DES ECB')
    parser.add_argument("-d", "--deskey", type=str, help="DES 64bit key binary file")
    parser.add_argument("-a", "--aeskey", type=str, help="AES 128bit key binary file")
    parser.add_argument("-r", "--rsakey", type=str, required=True, help="DER/PEM key file")
    args = parser.parse_args()
    # parser.print_help()
    if args.deskey and args.aeskey:
        print('Only can use one of AES/DES key')
        sys.exit(1)
    if args.deskey == None and args.aeskey == None:
        gen_pkcs1_pub_key(args)
    else:
        gen_encrypt_rsa_key(args)

