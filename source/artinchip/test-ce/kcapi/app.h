/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#ifndef _APP_CFG_H_
#define _APP_CFG_H_

#define NO_ALIGNMENT          0
#define INPUT_ALIGNMENT       1
#define OUTPUT_ALIGNMENT      2
#define BIDIRECTION_ALIGNMENT 3

#define TYPE_SK           0
#define TYPE_MD           1
#define TYPE_AK           2
#define TYPE_UTIL         3

#define KEY_64            8
#define KEY_128           16
#define KEY_192           24
#define KEY_256           32
#define KEY_512           64

#define BLK_64            8
#define BLK_128           16


struct subcmd_cfg {
	char *cmd_name;
	char *cipher_name;
	int type;
	int keylen;
	int ivlen;
	char *app_name;
};

int skcipher_command(struct subcmd_cfg *cmd, int argc, char *argv[]);
int akcipher_command(struct subcmd_cfg *cmd, int argc, char *argv[]);
int digest_command(struct subcmd_cfg *cmd, int argc, char *argv[]);
int util_command(struct subcmd_cfg *cmd, int argc, char *argv[]);

#endif
