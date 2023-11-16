/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _MPP_FUNC_H_
#define _MPP_FUNC_H_

#include <video/artinchip_ge.h>
#include <linux/errno.h>

/*
 * Graphics Engine funcs
 */
#if defined(CONFIG_ARTINCHIP_GE) && defined(CONFIG_ARTINCHIP_GE_NORMAL)

int aic_ge_bitblt(struct ge_bitblt *blt);

int aic_ge_bitblt_with_hsbc(struct ge_bitblt *blt, u32 *csc_coef);

#else /* CONFIG_ARTINCHIP_GE */

static inline int aic_ge_bitblt(struct ge_bitblt *blt)
{
	return -EINVAL;
}

static inline int aic_ge_bitblt_with_hsbc(struct ge_bitblt *blt,
					  u32 *csc_coef)
{
	return -EINVAL;
}

#endif /* CONFIG_ARTINCHIP_GE */

#endif /* _MPP_FUNC_H_ */
