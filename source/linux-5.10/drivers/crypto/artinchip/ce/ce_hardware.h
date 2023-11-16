/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 *
 * Wu Dehuang <dehuang.wu@artinchip.com>
 */

#ifndef _AIC_CE_DEFINE_H_
#define _AIC_CE_DEFINE_H_
#include <linux/io.h>

#define CE_REG_ICR              (0x00)
#define CE_REG_ISR              (0x04)
#define CE_REG_TAR              (0x08)
#define CE_REG_TCR              (0x0C)
#define CE_REG_TSR              (0x10)
#define CE_REG_TER              (0x14)
#define CE_REG_VER              (0xFFC)

#define ALG_TAG_AES_ECB         (0x00)
#define ALG_TAG_AES_CBC         (0x01)
#define ALG_TAG_AES_CTR         (0x02)
#define ALG_TAG_AES_XTS         (0x03)
#define ALG_TAG_AES_CTS         (0x04)
#define ALG_TAG_DES_ECB         (0x10)
#define ALG_TAG_DES_CBC         (0x11)
#define ALG_TAG_TDES_ECB        (0x20)
#define ALG_TAG_TDES_CBC        (0x21)
#define ALG_TAG_RSA             (0x30)
#define ALG_TAG_SHA1            (0x40)
#define ALG_TAG_SHA224          (0x41)
#define ALG_TAG_SHA256          (0x42)
#define ALG_TAG_SHA384          (0x43)
#define ALG_TAG_SHA512          (0x44)
#define ALG_TAG_MD5             (0x45)
#define ALG_TAG_HMAC_SHA1       (0x46)
#define ALG_TAG_HMAC_SHA256     (0x47)
#define ALG_TAG_TRNG            (0x50)

#define KEY_SIZE_64             (0x00)
#define KEY_SIZE_128            (0x01)
#define KEY_SIZE_192            (0x02)
#define KEY_SIZE_256            (0x03)
#define KEY_SIZE_512            (0x04)
#define KEY_SIZE_1024           (0x05)
#define KEY_SIZE_2048           (0x06)

#define TASK_LOAD_BIT_OFFSET    (31)
#define TASK_ALG_BIT_OFFSET     (0)

/*
 * Key source
 *   - User input key(SRAM/DRAM/SecureSRAM)
 *   - eFuse key
 */
#define CE_KEY_SRC_USER        (0)
#define CE_KEY_SRC_SSK         (1)
#define CE_KEY_SRC_HUK         (2)
#define CE_KEY_SRC_PNK         (3)
#define CE_KEY_SRC_PSK0        (4)
#define CE_KEY_SRC_PSK1        (5)
#define CE_KEY_SRC_PSK2        (6)
#define CE_KEY_SRC_PSK3        (7)

/* Operation direction */
#define OP_DIR_ENCRYPT         (0)
#define OP_DIR_DECRYPT         (1)

#define SECURE_SRAM_SIZE       (1024)
#define SECURE_SRAM_BASE       (0x10021000)

/* Accelerator Queue size */
#define ACCEL_QUEUE_MAX_SIZE   (8)

#define SK_ALG_ACCELERATOR      0
#define HASH_ALG_ACCELERATOR    1
#define AK_ALG_ACCELERATOR      2

struct aes_ecb_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u8  r2[28];        /* Pad to 36 bytes */
};

struct aes_cbc_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u32 iv_addr;
	u8  r2[24];        /* Pad to 36 bytes */
};

/*
 * CTS-CBC-CS3(Kerberos)
 */
struct aes_cts_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u32 iv_addr;
	u8  r2[24];        /* Pad to 36 bytes */
};

struct aes_ctr_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u32 ctr_in_addr;
	u32 ctr_out_addr;
	u8  r2[20];        /* Pad to 36 bytes */
};

struct aes_xts_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u32 tweak_addr;
	u8  r2[24];        /* Pad to 36 bytes */
};

struct des_ecb_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u8  r2[28];        /* Pad to 36 bytes */
};

struct des_cbc_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 direction : 1; /* bit[8] */
	u32 r0        : 7; /* bit[15:9] */
	u32 key_src   : 4; /* bit[19:16] */
	u32 key_siz   : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 key_addr;
	u32 iv_addr;
	u8  r2[24];        /* Pad to 36 bytes */
};

struct rsa_alg_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 r0        : 12;/* bit[19:8] */
	u32 op_siz    : 4; /* bit[23:20] */
	u32 r1        : 8; /* bit[31:24] */
	u32 m_addr;
	u32 d_e_addr;
	u8  r2[24];        /* Pad to 36 bytes */
};

struct hash_alg_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 r0        : 1; /* bit[8] */
	u32 iv_mode   : 1; /* bit[9] */
	u32 r1        : 22;/* bit[31:10] */
	u32 r2;
	u32 iv_addr;
	u8  r3[24];        /* Pad to 36 bytes */
};

struct hmac_alg_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 r0        : 1; /* bit[8] */
	u32 iv_mode   : 1; /* bit[9] */
	u32 r1        : 22;/* bit[31:10] */
	u32 hmac_key_addr;
	u32 iv_addr;
	u8  r2[24];        /* Pad to 36 bytes */
};

struct trng_alg_desc {
	u32 alg_tag   : 8; /* bit[7:0] */
	u32 r1        : 24;/* bit[31:8] */
	u8  r2[32];        /* Pad to 36 bytes */
};

union alg_desc {
	u8 alg_tag;
	struct aes_ecb_desc aes_ecb;
	struct aes_cbc_desc aes_cbc;
	struct aes_ctr_desc aes_ctr;
	struct aes_cts_desc aes_cts;
	struct aes_xts_desc aes_xts;
	struct des_ecb_desc des_ecb;
	struct des_cbc_desc des_cbc;
	struct rsa_alg_desc rsa;
	struct hash_alg_desc hash;
	struct hmac_alg_desc hmac;
	struct trng_alg_desc trng;
};

struct data_desc {
	u32 first_flag  : 1; /* bit[0] */
	u32 last_flag   : 1; /* bit[1] */
	u32 r1          : 30;/* bit[31:2] */
	u32 total_bytelen;
	u32 in_addr;
	u32 in_len;
	u32 out_addr;
	u32 out_len;
};

struct task_desc {
	union alg_desc alg;
	struct data_desc data;
	u32 next;
};

#endif
