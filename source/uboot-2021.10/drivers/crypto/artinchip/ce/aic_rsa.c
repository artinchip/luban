// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#include <config.h>
#include <common.h>
#include <dm.h>
#include <log.h>
#include <asm/types.h>
#include <malloc.h>
#include <u-boot/rsa-mod-exp.h>
#include <artinchip_crypto.h>

int aic_mod_exp(struct udevice *dev, const uint8_t *sig, uint32_t sig_len,
		struct key_prop *prop, uint8_t *out)
{
	struct udevice *bus;
	rsa_context_t ctx;
	int ret, out_size;

	ctx.n = (void *)prop->modulus;
	ctx.e = (void *)prop->public_exponent;
	ctx.key_bits = RSA_KEY_BITS_2048;

	ret = crypto_init_device(&bus);
	if (ret)
		return ret;
	ret = rsa_init(bus);
	if (ret)
		return ret;

	ret = rsa_public_decrypt(bus, &ctx, (void *)sig, sig_len, out, &out_size);
	if (ret) {
		pr_err("%s: RSA failed to verify: %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}

static const struct mod_exp_ops aic_mod_exp_ops = {
	.mod_exp	= aic_mod_exp,
};

U_BOOT_DRIVER(aic_rsa_mod_exp) = {
	.name	= "aic_rsa_mod_exp",
	.id	= UCLASS_MOD_EXP,
	.ops	= &aic_mod_exp_ops,
	.flags	= DM_FLAG_PRE_RELOC,
};

U_BOOT_DRVINFO(aic_rsa) = {
	.name = "aic_rsa_mod_exp",
};
