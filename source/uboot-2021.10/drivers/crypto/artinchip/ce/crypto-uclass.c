// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#define LOG_CATEGORY UCLASS_CRYPTO

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <dm/device_compat.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/lists.h>
#include <dm/util.h>
#include <cpu_func.h>
#include <cache.h>
#include <artinchip_crypto.h>

DECLARE_GLOBAL_DATA_PTR;

int crypto_init_device(struct udevice **dev)
{
	int ret;

	ret = uclass_first_device(UCLASS_CRYPTO, dev);
	if (ret)
		return ret;

	return ret;
}

static int crypto_init(struct udevice *dev)
{
	const struct dm_crypto_ops *ops;
	int ret = 0;

	ops = device_get_ops(dev);
	if (ops->init)
		ret = ops->init(dev);
	else
		return -EINVAL;

	return ret;
}

static int crypto_exit(struct udevice *dev)
{
	const struct dm_crypto_ops *ops;
	int ret = 0;

	ops = device_get_ops(dev);
	if (ops->release)
		ops->release(dev);
	else
		return -EINVAL;

	return ret;
}

static int crypto_start(struct udevice *dev, struct task_desc *task)
{
	const struct dm_crypto_ops *ops;
	int ret;

	ops = device_get_ops(dev);
	if (ops->start)
		ret = ops->start(dev, task);
	else
		ret = -EINVAL;
	if (ret) {
		dev_err(dev, "Cannot start ce (err=%d)\n", ret);
		return ret;
	}

	return 0;
}

static int crypto_poll_finish(struct udevice *dev, u32 alg_unit)
{
	const struct dm_crypto_ops *ops;
	int ret;

	ops = device_get_ops(dev);
	if (ops->poll_finish)
		ret = ops->poll_finish(dev, alg_unit);
	else
		ret = -EINVAL;

	return ret;
}

static int crypto_pending_clear(struct udevice *dev, u32 alg_unit)
{
	const struct dm_crypto_ops *ops;
	int ret = 0;

	ops = device_get_ops(dev);
	if (ops->pending_clear)
		ops->pending_clear(dev, alg_unit);
	else
		ret = -EINVAL;

	return ret;
}

static int crypto_get_err(struct udevice *dev, u32 alg_unit)
{
	const struct dm_crypto_ops *ops;
	int ret;

	ops = device_get_ops(dev);
	if (ops->get_err)
		ret = ops->get_err(dev, alg_unit);
	else
		ret = -EINVAL;

	return ret;
}

/* Big Number from little-endian to big-endian */
static int crypto_bignum_le2be(u8 *src, u32 slen, u8 *dst, u32 dlen)
{
	int i;

	if (dlen < slen)
		return(-1);

	for (i = 0; i < slen; i++)
		dst[dlen - 1 - i] = src[i];

	for (; i < dlen; i++)
		dst[dlen - 1 - i] = 0;

	return 0;
}

/* Big Number from big-endian to litte-endian */
static int crypto_bignum_be2le(u8 *src, u32 slen, u8 *dst, u32 dlen)
{
	int i;

	if (dlen < slen)
		return(-1);

	for (i = 0; i < slen; i++)
		dst[i] = src[slen - 1 - i];

	for (; i < dlen; i++)
		dst[i] = 0;

	return 0;
}

int aes_init(struct udevice *dev)
{
	return crypto_init(dev);
}

void aes_exit(struct udevice *dev)
{
	crypto_exit(dev);
}

int aes_set_encrypt_key(struct udevice *dev, void *key, u32 key_len)
{
	struct aes_priv *aes;
	struct crypto_priv *priv;

	if ((key_len > AES_MAX_KEY_LEN) || (key_len < AES_MIN_KEY_LEN))
		return -1;

	priv = dev_get_uclass_priv(dev);
	aes = (struct aes_priv *)priv->aes;
	memcpy(aes->key, key, key_len);
	aes->key_len = key_len;

	return 0;
}

int aes_set_decrypt_key(struct udevice *dev, void *key, u32 key_len)
{
	struct aes_priv *aes;
	struct crypto_priv *priv;

	if ((key_len > AES_MAX_KEY_LEN) || (key_len < AES_MIN_KEY_LEN))
		return -1;

	priv = dev_get_uclass_priv(dev);
	aes = (struct aes_priv *)priv->aes;
	memcpy(aes->key, key, key_len);
	aes->key_len = key_len;

	return 0;
}

#define INPUT_ALIGN_MSK (0x1 << 0)
#define OUTPUT_ALIGN_MSK (0x1 << 1)

#define INPUT_NOT_ALIGN(flag) ((flag) & INPUT_ALIGN_MSK)
#define OUTPUT_NOT_ALIGN(flag) ((flag) & OUTPUT_ALIGN_MSK)
#define CALC_ONCE_INPUT_MAX_LEN 0x3fff0

static int aes_ecb_calc(struct udevice *dev, struct task_desc *task,
				char *in, char *out, u32 len)
{
	struct crypto_priv *priv;
	u32 align_flag, dolen;
	u8 *pin, *pout;
	int ret;

	priv = dev_get_uclass_priv(dev);
	align_flag = 0;
	pin = in;
	if (((ulong)in) % 8) {
		/* input is not alignment */
		pin = priv->aes->align_buf;
		align_flag |= INPUT_ALIGN_MSK;
	}
	pout = out;
	if (((ulong)out) % 8) {
		/* output is not alignment */
		pout = priv->aes->align_buf;
		align_flag |= OUTPUT_ALIGN_MSK;
	}

	dolen = len;
	do {
		if (align_flag) {
			if (len > CE_WORK_BUF_LEN)
				dolen = CE_WORK_BUF_LEN;
			else
				dolen = len;
		}
		if (INPUT_NOT_ALIGN(align_flag)) {
			memcpy(pin, in, dolen);
			in += dolen;
		}

		flush_dcache_range((ulong)pin, (ulong)pin + dolen);

		task->data.in_addr = (u32)(uintptr_t)pin;
		task->data.in_len = dolen;
		task->data.out_addr = (u32)(uintptr_t)pout;
		task->data.out_len = dolen;

		flush_dcache_range((ulong)task, (ulong)task + sizeof(task));

		crypto_start(dev, task);

		ret = crypto_poll_finish(dev, ALG_UNIT_SYMM);
		if (ret) {
			dev_err(dev, "AES poll finish error.\n");
			return ret;
		}
		ret = crypto_pending_clear(dev, ALG_UNIT_SYMM);
		if (ret) {
			dev_err(dev, "AES pending clear error.\n");
			return ret;
		}

		ret = crypto_get_err(dev, ALG_UNIT_SYMM);
		if (ret) {
			dev_err(dev, "AES run error.\n");
			return ret;
		}
		invalidate_dcache_range((ulong)pout,
					(ulong)pout + dolen);

		if (OUTPUT_NOT_ALIGN(align_flag)) {
			memcpy(out, pout, dolen);
			out += dolen;
		}
		if ((!INPUT_NOT_ALIGN(align_flag)))
			pin += dolen;
		len -= dolen;
	} while (len);

	return 0;
}

static int crypto_aes_ecb_crypto(struct udevice *dev, u8 dir, u8 *in, u8 *out,
				 u32 len)
{
	struct task_desc task __attribute__ ((aligned(CONFIG_SYS_CACHELINE_SIZE)));
	struct crypto_priv *priv;
	u8 *pin, *pout, *key, kl;
	u32 dolen, input_once_size;
	int ret;

	priv = dev_get_uclass_priv(dev);
	key = priv->aes->key;
	kl = priv->aes->key_len;
	memset(&task, 0, sizeof(task));

	task.alg.aes_ecb.alg_tag = ALG_AES_ECB;
	task.alg.aes_ecb.direction = dir;
	task.alg.aes_ecb.key_src = CE_KEY_SRC_USER;
	if (kl == AES128_KEY_LEN)
		task.alg.aes_ecb.key_siz = KEY_SIZE_128;
	else if (kl == AES192_KEY_LEN)
		task.alg.aes_ecb.key_siz = KEY_SIZE_192;
	else if (kl == AES256_KEY_LEN)
		task.alg.aes_ecb.key_siz = KEY_SIZE_256;
	else
		return -1;

	flush_dcache_range((ulong)key, (ulong)key + kl);
	task.alg.aes_ecb.key_addr = (u32)(uintptr_t)key;

	pin = in;
	pout = out;
	do {
		dolen = len;
		input_once_size = CALC_ONCE_INPUT_MAX_LEN;
		if (dolen > input_once_size)
			dolen = input_once_size;
		else
			dolen = len;

		ret = aes_ecb_calc(dev, &task, pin, pout, dolen);
		if (ret) {
			printf("aes ebc calc error\n");
			return ret;
		}

		pout += dolen;
		pin += dolen;
		len -= dolen;
	} while (len);

	return 0;
}

int aes_ecb_encrypt(struct udevice *dev, void *in, void *out, u32 size)
{
	struct aes_priv *aes;
	struct crypto_priv *priv;

	priv = dev_get_uclass_priv(dev);
	aes = (struct aes_priv *)priv->aes;
	if (!aes) {
		printf("aes not exist\n");
		return -1;
	}

	if (size % 16) {
		printf("size not aligned\n");
		return -1;
	}

	return crypto_aes_ecb_crypto(dev, ALG_DIR_ENCRYPT, in, out, size);
}

int aes_ecb_decrypt(struct udevice *dev, void *in, void *out, u32 size)
{
	struct aes_priv *aes;
	struct crypto_priv *priv;

	priv = dev_get_uclass_priv(dev);
	aes = (struct aes_priv *)priv->aes;
	if (!aes) {
		printf("aes not exist\n");
		return -1;
	}

	if (size % 16) {
		printf("size not aligned\n");
		return -1;
	}

	return crypto_aes_ecb_crypto(dev, ALG_DIR_DECRYPT, in, out, size);
}

static int aes_cbc_calc(struct udevice *dev, struct task_desc *task,
		char *in, char *iv_in, char *out, u8 dir, u32 len)
{
	struct crypto_priv *priv;
	u32 align_flag, dolen = len;
	u8 *pin, *pout;
	int ret;

	priv = dev_get_uclass_priv(dev);
	align_flag = 0;
	pin = in;
	if (((ulong)in) % 8) {
		/* input is not alignment */
		pin = priv->aes->align_buf;
		align_flag |= INPUT_ALIGN_MSK;
	}
	pout = out;
	if (((ulong)out) % 8) {
		/* output is not alignment */
		pout = priv->aes->align_buf;
		align_flag |= OUTPUT_ALIGN_MSK;
	}

	do {
		if (align_flag) {
			if (len > CE_WORK_BUF_LEN)
				dolen = CE_WORK_BUF_LEN;
			else
				dolen = len;
		}
		if (INPUT_NOT_ALIGN(align_flag)) {
			memcpy(pin, in, dolen);
			in += dolen;
		}

		flush_dcache_range((ulong)iv_in,
				   (ulong)iv_in + AES_BLOCK_SIZE);
		flush_dcache_range((ulong)pin,
				   (ulong)pin + dolen);
		flush_dcache_range((ulong)pout,
				   (ulong)pout + dolen);

		task->data.in_addr = (u32)(uintptr_t)pin;
		task->data.in_len = dolen;
		task->data.out_addr = (u32)(uintptr_t)pout;
		task->data.out_len = dolen;

		flush_dcache_range((ulong)task,
				   (ulong)task + sizeof(task));

		crypto_start(dev, task);

		ret = crypto_poll_finish(dev, ALG_UNIT_SYMM);
		if (ret)
			return ret;

		ret = crypto_pending_clear(dev, ALG_UNIT_SYMM);
		if (ret)
			return ret;

		ret = crypto_get_err(dev, ALG_UNIT_SYMM);
		if (ret) {
			pr_err("AES run error.\n");
			return ret;
		}
		invalidate_dcache_range((ulong)pout,
					(ulong)pout + dolen);

		/* prepare iv for next */
		if (dir == ALG_DIR_ENCRYPT)
			memcpy(iv_in, pout + dolen - AES_BLOCK_SIZE, AES_BLOCK_SIZE);
		else
			memcpy(iv_in, pin + dolen - AES_BLOCK_SIZE, AES_BLOCK_SIZE);

		if (OUTPUT_NOT_ALIGN(align_flag)) {
			memcpy(out, pout, dolen);
			out += dolen;
		}

		if (!(INPUT_NOT_ALIGN(align_flag)))
			pin += dolen;
		len -= dolen;
	} while (len);
	return 0;
}

static int aes_cbc_crypto(struct udevice *dev, u8 dir, u8 *iv, u8 *in,
			  u8 *out, u32 len)
{
	struct task_desc task __attribute__ ((aligned(CONFIG_SYS_CACHELINE_SIZE)));
	struct crypto_priv *priv;
	u8 *iv_in, *key, kl;
	u32 dolen, input_once_size;
	int ret;

	priv = dev_get_uclass_priv(dev);
	key = priv->aes->key;
	kl = priv->aes->key_len;
	memset(&task, 0, sizeof(task));

	task.alg.aes_cbc.alg_tag = ALG_AES_CBC;
	task.alg.aes_cbc.direction = dir;
	task.alg.aes_cbc.key_src = CE_KEY_SRC_USER;
	if (kl == AES128_KEY_LEN)
		task.alg.aes_cbc.key_siz = KEY_SIZE_128;
	else if (kl == AES192_KEY_LEN)
		task.alg.aes_cbc.key_siz = KEY_SIZE_192;
	else if (kl == AES256_KEY_LEN)
		task.alg.aes_cbc.key_siz = KEY_SIZE_256;
	else
		return -1;

	flush_dcache_range((unsigned long)key, (unsigned long)key + kl);
	task.alg.aes_cbc.key_addr = (u32)(uintptr_t)key;
	iv_in = priv->aes->iv_in;
	memcpy(iv_in, iv, AES_BLOCK_SIZE);
	task.alg.aes_cbc.iv_addr = (u32)(uintptr_t)iv_in;

	do {
		dolen = len;
		input_once_size = CALC_ONCE_INPUT_MAX_LEN;
		if (dolen > input_once_size)
			dolen = input_once_size;
		else
			dolen = len;

		ret = aes_cbc_calc(dev, &task, in, iv_in, out, dir, dolen);
		if (ret) {
			printf("aes ebc calc error\n");
			return ret;
		}

		out += dolen;
		in += dolen;
		len -= dolen;
	} while (len);

	return 0;
}

int aes_cbc_encrypt(struct udevice *dev, void *in, void *out,
		    u32 size, void *iv)
{
	if (size % 16) {
		printf("size not aligned\n");
		return -1;
	}
	return aes_cbc_crypto(dev, ALG_DIR_ENCRYPT, iv, in, out, size);
}

int aes_cbc_decrypt(struct udevice *dev, void *in, void *out,
		    u32 size, void *iv)
{
	if (size % 16) {
		printf("size not aligned\n");
		return -1;
	}
	return aes_cbc_crypto(dev, ALG_DIR_DECRYPT, iv, in, out, size);
}

int rsa_init(struct udevice *dev)
{
	return crypto_init(dev);
}

void rsa_exit(struct udevice *dev)
{
	crypto_exit(dev);
}

static int rsa_calc(struct udevice *dev, u32 keybits, void *mod,
	void *prime, void *src, u32 src_size, void *out, int is_pubkey)
{
	struct task_desc task __attribute__ ((aligned(CONFIG_SYS_CACHELINE_SIZE)));
	struct crypto_priv *priv;
	u8 *pn, *pp, *data, *pout;
	u32 opsize;
	int ret;

	if (keybits == RSA_KEY_BITS_512 && src_size != 64)
		return -1;
	else if (keybits == RSA_KEY_BITS_1024 && src_size != 128)
		return -1;
	else if (keybits == RSA_KEY_BITS_2048 && src_size != 256)
		return -1;

	if ((keybits < RSA_KEY_BITS_512) || (keybits > RSA_KEY_BITS_2048))
		return -1;

	/* Use aligned buffer to CE */
	opsize = src_size;
	priv = dev_get_uclass_priv(dev);
	pp = priv->rsa->align_buf;
	memset(pp, 0, CE_WORK_BUF_LEN);
	pn = pp + opsize;
	data = pn + opsize;
	pout = data + opsize;

	crypto_bignum_be2le(src, opsize, data, opsize);
	crypto_bignum_be2le(mod, opsize, pn, opsize);
	if (is_pubkey)
		crypto_bignum_be2le(prime, sizeof(uint64_t), pp, opsize);
	else
		crypto_bignum_be2le(prime, opsize, pp, opsize);

	memset(&task, 0, sizeof(task));
	task.alg.rsa.alg_tag = ALG_RSA;

	if (opsize == 256)
		task.alg.rsa.op_siz = KEY_SIZE_2048;
	else if (opsize == 128)
		task.alg.rsa.op_siz = KEY_SIZE_1024;
	else if (opsize == 64)
		task.alg.rsa.op_siz = KEY_SIZE_512;
	else
		return -1;

	task.alg.rsa.m_addr = (u32)(uintptr_t)pn;
	task.alg.rsa.d_e_addr = (u32)(uintptr_t)pp;
	task.data.in_addr = (u32)(uintptr_t)data;
	task.data.in_len = opsize;
	task.data.out_addr = (u32)(uintptr_t)pout;
	task.data.out_len = opsize;

	flush_dcache_range((unsigned long)priv->rsa->align_buf,
			   (unsigned long)priv->rsa->align_buf +
			   CE_WORK_BUF_LEN);
	flush_dcache_range((unsigned long)&task,
			   (unsigned long)&task + sizeof(task));

	crypto_start(dev, &task);

	ret = crypto_poll_finish(dev, ALG_UNIT_ASYM);
	if (ret)
		return ret;

	ret = crypto_pending_clear(dev, ALG_UNIT_ASYM);
	if (ret)
		return ret;

	ret = crypto_get_err(dev, ALG_UNIT_ASYM);
	if (ret) {
		pr_err("RSA run error.\n");
		return ret;
	}

	invalidate_dcache_range((unsigned long)pout,
				(unsigned long)pout + opsize);
	crypto_bignum_le2be(pout, opsize, out, opsize);

	return 0;
}

/*
 * Use public key to encrypt
 */
int rsa_public_encrypt(struct udevice *dev, rsa_context_t *context,
		       void *src, u32 src_size, void *out)
{
	s32 ret;

	ret = rsa_calc(dev, context->key_bits, context->n, context->e,
		       src, src_size, out, 1);
	if (ret) {
		printf("Rsa calc failed\n");
		return ret;
	}

	return 0;
}

/*
 * Use private key to decrypt
 */
int rsa_private_decrypt(struct udevice *dev, rsa_context_t *context, void *src,
			u32 src_size, void *out, u32 *out_size)
{
	s32 ret;

	ret = rsa_calc(dev, context->key_bits, context->n, context->d,
		       src, src_size, out, 0);
	if (ret) {
		printf("Rsa calc failed\n");
		return ret;
	}

	*out_size = src_size;
	return 0;
}

/*
 * Use private key to encrypt
 */
int rsa_private_encrypt(struct udevice *dev, rsa_context_t *context, void *src,
			u32 src_size, void *out)
{
	s32 ret;

	ret = rsa_calc(dev, context->key_bits, context->n, context->d,
		       src, src_size, out, 0);
	if (ret) {
		printf("Rsa calc failed\n");
		return ret;
	}

	return 0;
}

/*
 * Use public key to decrypt
 */
int rsa_public_decrypt(struct udevice *dev, rsa_context_t *context, void *src,
		       u32 src_size, void *out, u32 *out_size)
{
	s32 ret;

	ret = rsa_calc(dev, context->key_bits, context->n, context->e,
		       src, src_size, out, 1);
	if (ret) {
		printf("Rsa calc failed\n");
		return ret;
	}

	*out_size = src_size;
	return 0;
}

int sha_init(struct udevice *dev)
{
	return crypto_init(dev);
}

void sha_exit(struct udevice *dev)
{
	crypto_exit(dev);
}

int sha_start(struct udevice *dev, sha_context_t *context, sha_mode_t mode)
{
	struct crypto_priv *priv;

	priv = dev_get_uclass_priv(dev);

	memset(context, 0, sizeof(*context));
	context->mode = mode;

	switch (mode) {
	case SHA_MODE_1:
		priv->sha->alg_tag = ALG_SHA1;
		priv->sha->out_len = SHA1_CE_OUT_LEN;
		priv->sha->digest_len = SHA1_DIGEST_SIZE;
		u32 sha1_iv[] = { BE_SHA1_H0, BE_SHA1_H1, BE_SHA1_H2,
				  BE_SHA1_H3, BE_SHA1_H4 };
		memcpy(priv->sha->digest, sha1_iv, SHA1_DIGEST_SIZE);
		break;
	case SHA_MODE_256:
		priv->sha->alg_tag = ALG_SHA256;
		priv->sha->out_len = SHA256_CE_OUT_LEN;
		priv->sha->digest_len = SHA256_DIGEST_SIZE;
		u32 sha256_iv[] = { BE_SHA256_H0, BE_SHA256_H1,
				    BE_SHA256_H2, BE_SHA256_H3,
				    BE_SHA256_H4, BE_SHA256_H5,
				    BE_SHA256_H6, BE_SHA256_H7 };
		memcpy(priv->sha->digest, sha256_iv, SHA256_DIGEST_SIZE);
		break;
	case SHA_MODE_224:
		priv->sha->alg_tag = ALG_SHA224;
		priv->sha->out_len = SHA224_CE_OUT_LEN;
		priv->sha->digest_len = SHA224_DIGEST_SIZE;
		u32 sha224_iv[] = { BE_SHA224_H0, BE_SHA224_H1,
				    BE_SHA224_H2, BE_SHA224_H3,
				    BE_SHA224_H4, BE_SHA224_H5,
				    BE_SHA224_H6, BE_SHA224_H7 };
		memcpy(priv->sha->digest, sha224_iv, SHA224_DIGEST_SIZE);
		break;
	case SHA_MODE_512:
		priv->sha->alg_tag = ALG_SHA512;
		priv->sha->out_len = SHA512_CE_OUT_LEN;
		priv->sha->digest_len = SHA512_DIGEST_SIZE;
		u64 sha512_iv[] = { BE_SHA512_H0, BE_SHA512_H1,
				    BE_SHA512_H2, BE_SHA512_H3,
				    BE_SHA512_H4, BE_SHA512_H5,
				    BE_SHA512_H6, BE_SHA512_H7 };
		memcpy(priv->sha->digest, sha512_iv, SHA512_DIGEST_SIZE);
		break;
	case SHA_MODE_384:
		priv->sha->alg_tag = ALG_SHA384;
		priv->sha->out_len = SHA384_CE_OUT_LEN;
		priv->sha->digest_len = SHA384_DIGEST_SIZE;
		u64 sha384_iv[] = { BE_SHA384_H0, BE_SHA384_H1,
				    BE_SHA384_H2, BE_SHA384_H3,
				    BE_SHA384_H4, BE_SHA384_H5,
				    BE_SHA384_H6, BE_SHA384_H7 };
		memcpy(priv->sha->digest, sha384_iv, SHA384_DIGEST_SIZE);
		break;
	default:
		return -1;
	}

	return 0;
}

int sha_update(struct udevice *dev, sha_context_t *context, const void *input,
	       u32 size, int is_last)
{
	struct task_desc task __attribute__ ((aligned(CONFIG_SYS_CACHELINE_SIZE)));
	struct crypto_priv *priv;
	u8 *in, *out, *iv;
	u32 dolen;
	u64 total_len;
	int ret;

	memcpy(&total_len, context->total, 8);
	total_len += size;

	priv = dev_get_uclass_priv(dev);
	memset(&task, 0, sizeof(struct task_desc));

	iv = priv->sha->digest;
	out = priv->sha->digest;
	task.alg.hash.alg_tag = priv->sha->alg_tag;
	task.alg.hash.iv_mode = 1;

	flush_dcache_range((unsigned long)iv,
			   (unsigned long)iv + SHA_MAX_OUTPUT_LEN);
	task.alg.hash.iv_addr = (u32)(uintptr_t)iv;

	do {
		dolen = size;
		in = (u8 *)input;
		if (((unsigned long)input) % 8) {
			/* input is not alignment */
			in = priv->sha->align_buf;
			if (dolen > CE_WORK_BUF_LEN)
				dolen = CE_WORK_BUF_LEN;
			memcpy(in, input, dolen);
		}
		flush_dcache_range((unsigned long)in,
				   (unsigned long)in + dolen);
		flush_dcache_range((unsigned long)out,
				   (unsigned long)out +
				   priv->sha->out_len);

		task.data.in_addr = (u32)(uintptr_t)in;
		task.data.in_len = dolen;
		task.data.out_addr = (u32)(uintptr_t)out;
		task.data.out_len = priv->sha->out_len;
		if ((size - dolen) <= 0) {
			task.data.last_flag = is_last;
			task.data.total_bytelen = (u32)(uintptr_t)total_len;
		}

		flush_dcache_range((unsigned long)&task,
				   (unsigned long)&task + sizeof(task));

		flush_dcache_all();
		crypto_start(dev, &task);

		ret = crypto_poll_finish(dev, ALG_UNIT_HASH);
		if (ret)
			return ret;

		ret = crypto_pending_clear(dev, ALG_UNIT_HASH);
		if (ret)
			return ret;

		ret = crypto_get_err(dev, ALG_UNIT_HASH);
		if (ret) {
			pr_err("SHA run error.\n");
			return ret;
		}
		size -= dolen;
		input += dolen;
	} while (size > 0);

	invalidate_dcache_range((unsigned long)out,
				(unsigned long)out + SHA_MAX_OUTPUT_LEN);

	return 0;
}

int sha_finish(struct udevice *dev, sha_context_t *context, void *output,
	       u32 *out_size)
{
	struct crypto_priv *priv;
	u8 *out;

	priv = dev_get_uclass_priv(dev);
	out = priv->sha->digest;
	memcpy(output, out, priv->sha->digest_len);
	*out_size = priv->sha->digest_len;

	return 0;
}

static int crypto_post_probe(struct udevice *dev)
{
	struct crypto_priv *priv;

	priv = dev_get_uclass_priv(dev);
	priv->aes = memalign(CONFIG_SYS_CACHELINE_SIZE, sizeof(struct aes_priv));
	if (!priv->aes) {
		pr_err("malloc  buf failed.\n");
		return -1;
	}
	memset(priv->aes, 0, sizeof(struct aes_priv));

	priv->sha = memalign(CONFIG_SYS_CACHELINE_SIZE, sizeof(struct sha_priv));
	if (!priv->sha) {
		pr_err("malloc  buf failed.\n");
		return -1;
	}
	memset(priv->sha, 0, sizeof(struct sha_priv));

	priv->rsa = memalign(CONFIG_SYS_CACHELINE_SIZE, sizeof(struct rsa_priv));
	if (!priv->rsa) {
		pr_err("malloc  buf failed.\n");
		return -1;
	}
	memset(priv->rsa, 0, sizeof(struct rsa_priv));

	return 0;
}

UCLASS_DRIVER(crypto) = {
	.id		= UCLASS_CRYPTO,
	.name		= "ce",
	.post_probe	= crypto_post_probe,
	.per_device_auto = sizeof(struct crypto_priv),
};
