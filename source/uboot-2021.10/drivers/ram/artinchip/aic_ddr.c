// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */
#include <clk.h>
#include <common.h>
#include <cpu_func.h>
#include <mapmem.h>
#include <dm.h>
#include <dt-structs.h>
#include <ram.h>
#include <asm/io.h>
#include <asm/arch/boot_param.h>

#ifndef CONFIG_ARCH_RISCV_ARTINCHIP

#define DRAM_PARA_CNT   (28)
struct aic_dram_para {
	unsigned int dram_clk;
	unsigned int dram_type;
	unsigned int dram_zq;
	unsigned int dram_odt_en;
	unsigned int dram_para1;
	unsigned int dram_para2;
	unsigned int dram_mr0;
	unsigned int dram_mr1;
	unsigned int dram_mr2;
	unsigned int dram_mr3;
	unsigned int dram_mr4;
	unsigned int dram_mr5;
	unsigned int dram_mr6;
	unsigned int dram_tpr0;
	unsigned int dram_tpr1;
	unsigned int dram_tpr2;
	unsigned int dram_tpr3;
	unsigned int dram_tpr4;
	unsigned int dram_tpr5;
	unsigned int dram_tpr6;
	unsigned int dram_tpr7;
	unsigned int dram_tpr8;
	unsigned int dram_tpr9;
	unsigned int dram_tpr10;
	unsigned int dram_tpr11;
	unsigned int dram_tpr12;
	unsigned int dram_tpr13;
	unsigned int dram_tpr14;
	unsigned int dram_tpr15;
	unsigned int dram_tpr16;
	unsigned int dram_tpr17;
	unsigned int dram_tpr18;
};

extern void aic_dram_init(struct aic_dram_para *para, int runtime);

struct aic_ddr_priv {
	fdt_addr_t base;
	u32 mem_size;
	struct aic_dram_para para;
	int runtime_training;
};

struct aic_ddr_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_dramc dtplat;
#endif
};

#if CONFIG_IS_ENABLED(OF_PLATDATA)
static void aic_ddr_init_platdata(struct udevice *dev)
{
	struct aic_ddr_plat *plat = dev_get_plat(dev);
	struct dtd_artinchip_aic_dramc *dtplat = &plat->dtplat;
	struct aic_ddr_priv *priv = dev_get_priv(dev);

	/* get dram controller register address */
	priv->base = (fdt_addr_t)map_sysmem(dtplat->reg[0], dtplat->reg[1]);

	/* get dram parameters */
	priv->para.dram_clk = dtplat->artinchip_freq;
	priv->para.dram_type = dtplat->artinchip_type;
	priv->para.dram_zq = dtplat->artinchip_zq;
	priv->para.dram_odt_en = dtplat->artinchip_odt;
	memcpy(&priv->para.dram_para1, dtplat->artinchip_para,
	       DRAM_PARA_CNT * sizeof(u32));
	priv->mem_size = dtplat->artinchip_memsize;
	priv->runtime_training = dtplat->artinchip_runtime_training;

	pr_info("%s: dram parameter:freq(%u), type(%u), zq(0x%x), odt(%u)\n",
	       __func__, priv->para.dram_clk, priv->para.dram_type,
	       priv->para.dram_zq, priv->para.dram_odt_en);
}

#else
static void aic_ddr_init_ofdata(struct udevice *dev)
{
	struct aic_ddr_priv *priv = dev_get_priv(dev);

	/* get dram controller register address */
	priv->base = dev_read_addr_index(dev, 0);

	/* get dram parameters */
	dev_read_u32(dev, "artinchip,freq", &priv->para.dram_clk);
	dev_read_u32(dev, "artinchip,type", &priv->para.dram_type);
	dev_read_u32(dev, "artinchip,zq", &priv->para.dram_zq);
	dev_read_u32(dev, "artinchip,odt", &priv->para.dram_odt_en);
	dev_read_u32_array(dev, "artinchip,para",
			   (u32 *)&priv->para.dram_para1, DRAM_PARA_CNT);
	dev_read_u32(dev, "artinchip,memsize", &priv->mem_size);
	priv->runtime_training =
		dev_read_bool(dev, "artinchip,runtime-training");

	pr_info("%s: dram parameter:freq(%u), type(%u), zq(0x%x), odt(%u)\n",
	       __func__, priv->para.dram_clk, priv->para.dram_type,
	       priv->para.dram_zq, priv->para.dram_odt_en);
}
#endif

static int aic_ddr_probe(struct udevice *dev)
{
	struct aic_ddr_priv *priv = dev_get_priv(dev);
	(void)priv;
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	aic_ddr_init_platdata(dev);
#else
	aic_ddr_init_ofdata(dev);
#endif

#ifndef CONFIG_ARCH_RISCV_ARTINCHIP
	pr_notice("DRAM size %dMB, I-Cache is %s, D-Cache is %s\n",
		  priv->mem_size / (1024 * 1024),
		  icache_status() ? "Enabled" : "Disabled",
		  dcache_status() ? "Enabled" : "Disabled");

#if !defined(CONFIG_SEMIHOSTING) && \
	(!defined(CONFIG_SPL) || defined(CONFIG_SPL_BUILD))
	aic_dram_init(&priv->para, priv->runtime_training);
#endif
#endif
	return 0;
}

static int aic_ddr_get_info(struct udevice *dev, struct ram_info *info)
{
	struct aic_ddr_priv *priv = dev_get_priv(dev);

	info->base = CONFIG_SYS_SDRAM_BASE;
	info->size = priv->mem_size;

	return 0;
}

static struct ram_ops aic_ddr_ops = {
	.get_info = aic_ddr_get_info,
};

static const struct udevice_id aic_ddr_ids[] = {
	{ .compatible = "artinchip,aic-dramc" },
	{ }
};

U_BOOT_DRIVER(artinchip_ddr) = {
	.name                     = "artinchip_aic_dramc",
	.id                       = UCLASS_RAM,
	.of_match                 = aic_ddr_ids,
	.ops                      = &aic_ddr_ops,
	.probe                    = aic_ddr_probe,
	.priv_auto                = sizeof(struct aic_ddr_priv),
	.plat_auto                = sizeof(struct aic_ddr_plat),
};
#endif
