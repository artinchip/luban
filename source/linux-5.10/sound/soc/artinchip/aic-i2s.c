// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 Artinchip Inc.
 */
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/dmaengine.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/pcm_params.h>

#define I2S_CTL_REG			0x00
#define I2S_CTL_GEN			BIT(0)
#define I2S_CTL_RXEN			BIT(1)
#define I2S_CTL_TXEN			BIT(2)
#define I2S_CTL_LOOP			BIT(3)
#define I2S_CTL_MODE_MASK		GENMASK(5, 4)
#define I2S_CTL_PCM_MODE		(0 << 4)
#define I2S_CTL_LEFT_MODE		(1 << 4)
#define I2S_CTL_RIGHT_J_MODE		(2 << 4)
#define I2S_CTL_OUTMUTE_MASK		BIT(6)
#define I2S_CTL_DOUT_EN			BIT(8)
#define I2S_CTL_LRCK_MASK		BIT(17)
#define I2S_CTL_LRCK_SLAVE		(0 << 17)
#define I2S_CTL_LRCK_MASTER		(1 << 17)
#define I2S_CTL_BCLK_MASK		BIT(18)
#define I2S_CTL_BCLK_SLAVE		(0 << 18)
#define I2S_CTL_BCLK_MASTER		(1 << 18)

#define I2S_FMT0_REG			(0x04)
#define I2S_FMT0_SW_MASK		GENMASK(2, 0)
#define I2S_FMT0_SW(sw)			((sw) << 0)
#define I2S_FMT0_EDGE_TRANS		BIT(3)
#define I2S_FMT0_SR_MASK		GENMASK(6, 4)
#define I2S_FMT0_SR(sr)			((sr) << 4)
#define I2S_FMT0_BCLK_POL_MASK		BIT(7)
#define I2S_FMT0_BCLK_POL_NORMAL	(0 << 7)
#define I2S_FMT0_BCLK_POL_INVERTED	(1 << 7)
#define I2S_FMT0_LRCK_PERIOD_MASK	GENMASK(17, 8)
#define I2S_FMT0_LRCK_PERIOD(wid)	((wid) << 8)
#define I2S_FMT0_LRCK_PERIOD_MAX	(1024)
#define I2S_FMT0_LRCK_POL_MASK		BIT(19)
#define I2S_FMT0_LRCK_POL_NORMAL	(0 << 19)
#define I2S_FMT0_LRCK_POL_INVERTED	(1 << 19)
#define I2S_FMT0_LRCK_WIDTH		BIT(30)

#define I2S_FMT1_REG			(0x08)
#define I2S_FMT1_TX_PDM_MASK		GENMASK(1, 0)
#define I2S_FMT1_TX_PDM_LINEAR		(0 << 0)
#define I2S_FMT1_TX_PDM_ULAW		(2 << 0)
#define I2S_FMT1_TX_PDM_ALAW		(3 << 0)
#define I2S_FMT1_RX_PDM_MASK		GENMASK(3, 2)
#define I2S_FMT1_RX_PDM_LINEAR		(0 << 2)
#define I2S_FMT1_RX_PDM_ULAW		(2 << 2)
#define I2S_FMT1_RX_PDM_ALAW		(3 << 2)
#define I2S_FMT1_SEXT_MASK		GENMASK(5, 4)
#define I2S_FMT1_SEXT_ZERO_LSB		(0 << 4)
#define I2S_FMT1_SEXT_SE_MSB		(1 << 4)
#define I2S_FMT1_SEXT_TRANS0		(3 << 4)
#define I2S_FMT1_TXMLS			BIT(6)
#define I2S_FMT1_RXMLS			BIT(7)

#define I2S_ISTA_REG			(0x0c)
#define I2S_RXFIFO_REG			(0x10)

#define I2S_FCTL_REG			(0x14)
#define I2S_FCTL_RXOM_MASK		GENMASK(1, 0)
#define I2S_FCTL_RXOM(mode)		((mode) << 0)
#define I2S_FCTL_TXIM			BIT(2)
#define I2S_FCTL_FRX			BIT(24)
#define I2S_FCTL_FTX			BIT(25)

#define I2S_FSTA_REG			(0x18)
#define I2S_INT_REG			(0x1c)
#define I2S_INT_RXDRQ_EN		BIT(3)
#define I2S_INT_TXDRQ_EN		BIT(7)

#define I2S_TXFIFO_REG			(0x20)
#define I2S_CLKD_REG			(0X24)
#define I2S_CLKD_MCLKDIV_MASK		GENMASK(3, 0)
#define I2S_CLKD_MCLKDIV(mdiv)  	((mdiv) << 0)
#define I2S_CLKD_BCLKDIV_MASK		GENMASK(7, 4)
#define I2S_CLKD_BCLKDIV(bdiv)		((bdiv) << 4)
#define I2S_CLKD_MCLKO_EN		BIT(8)

#define I2S_TXCNT_REG			(0x28)
#define I2S_RXCNT_REG			(0x2c)

#define I2S_CHCFG_REG			(0x30)
#define I2S_CHCFG_TXSLOTNUM_MASK	GENMASK(3, 0)
#define I2S_CHCFG_TXSLOTNUM(num)	(((num) - 1) << 0)
#define I2S_CHCFG_RXSLOTNUM_MASK	GENMASK(7, 4)
#define I2S_CHCFG_RXSLOTNUM(num)	(((num) - 1) << 4)

#define I2S_TXCHSEL_REG			(0x34)
#define I2S_TXCHSEL_TXCHEN_MASK		GENMASK(15, 0)
#define I2S_TXCHSEL_TXCHEN(ch)		((1 << (ch)) - 1)
#define I2S_TXCHSEL_TXCHSEL_MASK	GENMASK(19, 16)
#define I2S_TXCHSEL_TXCHSEL(num)	(((num) - 1) << 16)
#define I2S_TXCHSEL_TXOFFSET_MASK	GENMASK(21, 20)
#define I2S_TXCHSEL_OFFSET_0		(0 << 20)
#define I2S_TXCHSEL_OFFSET_1		(1 << 20)

#define I2S_TXCHMAP0_REG		(0x44)
#define I2S_TXCHMAP0_CHMAP_MASK(ch)	GENMASK(((ch) - 8) * 4 + 3, (ch) - 8)
#define I2S_TXCHMAP0_CHMAP(ch, chmap)	((chmap) << ((ch) - 8) * 4)

#define I2S_TXCHMAP1_REG		(0x48)
#define I2S_TXCHMAP1_CHMAP_MASK(ch)	GENMASK((ch) * 4 + 3, (ch) * 4)
#define I2S_TXCHMAP1_CHMAP(ch, chmap)	((chmap) << ((ch) * 4))

#define I2S_RXCHSEL_REG			(0x64)
#define I2S_RXCHSEL_RXCHSEL_MASK	GENMASK(19, 16)
#define I2S_RXCHSEL_RXCHSEL(num)	((num - 1) << 16)
#define I2S_RXCHSEL_RXOFFSET_MASK	GENMASK(21, 20)
#define I2S_RXCHSEL_RXOFFSET_0		(0 << 20)
#define I2S_RXCHSEL_RXOFFSET_1		(1 << 20)

#define I2S_RXCHMAP0_REG		(0x68)
#define I2S_RXCHMAP0_CHMAP_MASK(ch)	GENMASK(((ch) - 8) * 4 + 3, (ch) - 8)
#define I2S_RXCHMAP0_CHMAP(ch, chmap)	((chmap) << ((ch) - 8) * 4)

#define I2S_RXCHMAP1_REG		(0x6C)
#define I2S_RXCHMAP1_CHMAP_MASK(ch)	GENMASK((ch) * 4 + 3, (ch) * 4)
#define I2S_RXCHMAP1_CHMAP(ch, chmap)	((chmap) << ((ch) * 4))

struct aic_i2s {
	struct clk *clk;
	struct reset_control *rst;
	struct regmap *regmap;

	struct snd_dmaengine_dai_dma_data playback_dma_data;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	unsigned int mclk_freq;
	unsigned int bclk_ratio;
	unsigned int format;
	unsigned int slots;
	unsigned int slot_width;
};

struct aic_i2s_clk_div {
	u8 div;
	u8 val;
};

static const struct aic_i2s_clk_div i2s_bmclk_div[] = {
		{ .div = 1,   .val = 1 },
		{ .div = 2,   .val = 2 },
		{ .div = 4,   .val = 3 },
		{ .div = 6,   .val = 4 },
		{ .div = 8,   .val = 5 },
		{ .div = 12,  .val = 6 },
		{ .div = 16,  .val = 7 },
		{ .div = 24,  .val = 8 },
		{ .div = 32,  .val = 9 },
		{ .div = 48,  .val = 10 },
		{ .div = 64,  .val = 11 },
		{ .div = 96,  .val = 12 },
		{ .div = 128, .val = 13 },
		{ .div = 176, .val = 14 },
		{ .div = 192, .val = 15 },
};

static void aic_set_lrck_period(struct aic_i2s *i2s,
					unsigned int ratio)
{
	switch (i2s->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAI_FORMAT_I2S:
	case SND_SOC_DAI_FORMAT_LEFT_J:
	case SND_SOC_DAI_FORMAT_RIGHT_J:
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_PERIOD_MASK,
					I2S_FMT0_LRCK_PERIOD(ratio / 2 -1));
		break;
	case SND_SOC_DAI_FORMAT_DSP_A:
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_PERIOD_MASK,
					I2S_FMT0_LRCK_PERIOD(ratio - 1));
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_POL_MASK,
					I2S_FMT0_LRCK_POL_MASK);
		break;
	case SND_SOC_DAI_FORMAT_DSP_B:
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_WIDTH,
					I2S_FMT0_LRCK_WIDTH);
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_POL_MASK,
					I2S_FMT0_LRCK_POL_MASK);
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_PERIOD_MASK,
					I2S_FMT0_LRCK_PERIOD(ratio - 1));
		break;
	default:	/* I2S default Mode is PCM mode */
		regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_LRCK_PERIOD_MASK,
					I2S_FMT0_LRCK_PERIOD(ratio - 1));
		break;
	}
}

static int aic_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
					unsigned int freq, int dir)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	if (clk_id != 0)
		return -EINVAL;

	i2s->mclk_freq = freq;
	return 0;
}

static int aic_i2s_set_bclk_ratio(struct snd_soc_dai *dai,
					unsigned int ratio)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	if (ratio > I2S_FMT0_LRCK_PERIOD_MAX) {
		dev_err(dai->dev, "Unsupported bclk ratio\n");
		return -EINVAL;
	}

	i2s->bclk_ratio = ratio;
	return 0;
}

static int aic_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	u32 val = 0;

	/* Set I2S master or slave mode */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		val = I2S_CTL_LRCK_SLAVE | I2S_CTL_BCLK_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		val = I2S_CTL_LRCK_SLAVE | I2S_CTL_BCLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		val = I2S_CTL_LRCK_MASTER | I2S_CTL_BCLK_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		val = I2S_CTL_LRCK_MASTER | I2S_CTL_BCLK_MASTER;
		break;
	default:
		dev_err(dai->dev, "Unsupported master or slave mode of I2S\n");
		return -EINVAL;
	}

	regmap_update_bits(i2s->regmap, I2S_CTL_REG,
				I2S_CTL_LRCK_MASK | I2S_CTL_BCLK_MASK, val);

	/* Set I2S BCLK and LRCK polarity */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		val = I2S_FMT0_BCLK_POL_INVERTED | I2S_FMT0_LRCK_POL_INVERTED;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val = I2S_FMT0_BCLK_POL_INVERTED;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		val = I2S_FMT0_LRCK_POL_INVERTED;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		val = 0;
		break;
	default:
		dev_err(dai->dev, "Unsupported bclk/lrck polarity of I2S\n");
		return -EINVAL;
	}

	regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
			I2S_FMT0_BCLK_POL_MASK | I2S_FMT0_LRCK_POL_MASK,
			val);

	/* Set Transfer Mode selection */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAI_FORMAT_DSP_A:
	case SND_SOC_DAI_FORMAT_DSP_B:
		val = I2S_CTL_PCM_MODE;
		break;
	case SND_SOC_DAI_FORMAT_I2S:
	case SND_SOC_DAI_FORMAT_LEFT_J:
		val = I2S_CTL_LEFT_MODE;
		break;
	case SND_SOC_DAI_FORMAT_RIGHT_J:
		val = I2S_CTL_RIGHT_J_MODE;
		break;
	default:
		dev_err(dai->dev, "Unsupported transfer mode of I2S\n");
		return -EINVAL;
	}

	regmap_update_bits(i2s->regmap, I2S_CTL_REG,
					   I2S_CTL_MODE_MASK, val);
	i2s->format = fmt;
	return 0;
}

static int aic_i2s_set_tdm_slot(struct snd_soc_dai *dai,
				unsigned int tx_mask, unsigned int rx_mask,
				int slots, int slot_width)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	if (!slots)
		return 0;

	if (tx_mask >= (1 << slots) || rx_mask >= (1 << slots)) {
		dev_err(dai->dev, "Invalid TDM slot mask\n");
		return -EINVAL;
	}

	if (slots > 16) {
		dev_err(dai->dev, "Invalid slots number: Max value is 16\n");
		return -EINVAL;
	}

	i2s->slots = slots;
	i2s->slot_width = slot_width;
	return 0;
}

static int aic_i2s_set_chan_cfg(const struct aic_i2s *i2s,
					const struct snd_pcm_hw_params *params)
{
	unsigned int channels = params_channels(params);
	u32 val = 0;
	/* Map the channels for playback and capture */
	regmap_write(i2s->regmap, I2S_TXCHMAP0_REG, 0xFEDCBA98);
	regmap_write(i2s->regmap, I2S_TXCHMAP1_REG, 0x76543210);
	regmap_write(i2s->regmap, I2S_RXCHMAP0_REG, 0xFEDCBA98);
	regmap_write(i2s->regmap, I2S_RXCHMAP1_REG, 0x76543210);

	/* Select channels number */
	regmap_update_bits(i2s->regmap, I2S_CHCFG_REG,
					   I2S_CHCFG_TXSLOTNUM_MASK,
					   I2S_CHCFG_TXSLOTNUM(channels));
	regmap_update_bits(i2s->regmap, I2S_CHCFG_REG,
					   I2S_CHCFG_RXSLOTNUM_MASK,
					   I2S_CHCFG_RXSLOTNUM(channels));
	regmap_update_bits(i2s->regmap, I2S_TXCHSEL_REG,
					   I2S_TXCHSEL_TXCHSEL_MASK,
					   I2S_TXCHSEL_TXCHSEL(channels));
	regmap_update_bits(i2s->regmap, I2S_RXCHSEL_REG,
					   I2S_RXCHSEL_RXCHSEL_MASK,
					   I2S_RXCHSEL_RXCHSEL(channels));

	/* Set channel offset */
	switch (i2s->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAI_FORMAT_DSP_B:
	case SND_SOC_DAI_FORMAT_LEFT_J:
		val = 0;
		break;
	case SND_SOC_DAI_FORMAT_DSP_A:
	case SND_SOC_DAI_FORMAT_I2S:
		val = (1 << 20);
		break;
	default:
		val = 0;
		break;
	}

	/* Setting offset to select transfer mode */
	regmap_update_bits(i2s->regmap, I2S_TXCHSEL_REG,
					I2S_TXCHSEL_TXOFFSET_MASK, val);
	regmap_update_bits(i2s->regmap, I2S_RXCHSEL_REG,
					I2S_RXCHSEL_RXOFFSET_MASK, val);

	/* Enable TX channel */
	regmap_update_bits(i2s->regmap, I2S_TXCHSEL_REG,
						I2S_TXCHSEL_TXCHEN_MASK,
						I2S_TXCHSEL_TXCHEN(channels));

	return 0;
}

static int aic_i2s_get_sr(struct aic_i2s *i2s, unsigned int width)
{
	if (width < 8 || width > 32)
		return -EINVAL;

	if (width % 4)
		return -EINVAL;

	return (width - 8) / 4 + 1;
}

static int aic_i2s_get_sw(struct aic_i2s *i2s, unsigned int width)
{
	if (width < 8 || width > 32)
		return -EINVAL;

	if (width % 4)
		return -EINVAL;

	return (width - 8) / 4 + 1;
}

static int aic_i2s_get_bclk_div(struct aic_i2s *i2s,
				unsigned int module_clk_rate,
				unsigned int sample_rate,
				unsigned int bclk_ratio)
{
	int i;
	const struct aic_i2s_clk_div *dividers = &i2s_bmclk_div[0];
	int div = module_clk_rate / sample_rate / bclk_ratio;

	for (i = 0; i < ARRAY_SIZE(i2s_bmclk_div); i++) {
		const struct aic_i2s_clk_div *bmdiv = &dividers[i];

		if (bmdiv->div == div)
			return bmdiv->val;
	}
	return -EINVAL;
}

static int aic_i2s_get_mclk_div(struct aic_i2s *i2s,
				unsigned int module_clk_rate,
				unsigned int mclk_rate)
{
	int i;
	const struct aic_i2s_clk_div *dividers = &i2s_bmclk_div[0];
	int div = module_clk_rate / mclk_rate;

	for (i = 0; i < ARRAY_SIZE(i2s_bmclk_div); i++) {
		const struct aic_i2s_clk_div *bmdiv = &dividers[i];

		if (bmdiv->div == div)
			return bmdiv->val;
	}
	return -EINVAL;
}

static int aic_i2s_set_clk(struct snd_soc_dai *dai,
				unsigned int sample_rate,
				unsigned int slots,
				unsigned int slot_width)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int clk_rate, bclk_div, mclk_div;
	struct clk *parent_clk;
	unsigned long freq = 0;
	int ret = 0;

	switch (sample_rate) {
	case 176400:
	case 88200:
	case 44100:
	case 22050:
	case 11025:
		clk_rate = 22579200;
		break;
	case 384000:
	case 192000:
	case 128000:
	case 96000:
	case 64000:
	case 48000:
	case 32000:
	case 24000:
	case 16000:
	case 12000:
	case 8000:
		clk_rate = 24576000;
		break;
	default:
		dev_err(dai->dev, "Unsupport sample rate: %u\n", sample_rate);
		return -EINVAL;
	}

	parent_clk = clk_get_parent(i2s->clk);
	if (!parent_clk) {
		dev_err(dai->dev, "Cannot get i2s parent clock\n");
		return -EINVAL;
	}

	freq = clk_get_rate(parent_clk);
	if (abs(clk_rate * 20 - freq) > 2000) {
		/* Set i2s parent clock rate */
		ret = clk_set_rate(parent_clk, clk_rate * 20);
		if (ret)
			return ret;
	}

	/* Set module clock rate */
	ret = clk_set_rate(i2s->clk, clk_rate);
	if (ret)
		return ret;

	/* Set bclk div */
	if (i2s->bclk_ratio)
		bclk_div = aic_i2s_get_bclk_div(i2s, clk_rate,
					sample_rate, i2s->bclk_ratio);
	else
		bclk_div = aic_i2s_get_bclk_div(i2s, clk_rate,
					sample_rate, slots * slot_width);

	if (bclk_div < 0) {
		dev_err(dai->dev, "Unsupported BCLK divider\n");
		return -EINVAL;
	}

	/* Set mclk rate */
	mclk_div = aic_i2s_get_mclk_div(i2s, clk_rate, i2s->mclk_freq);
	if (mclk_div < 0) {
		dev_err(dai->dev, "Unsupported MCLK divider\n");
		return -EINVAL;
	}

	regmap_update_bits(i2s->regmap, I2S_CLKD_REG,
			I2S_CLKD_BCLKDIV_MASK, I2S_CLKD_BCLKDIV(bclk_div));
	regmap_update_bits(i2s->regmap, I2S_CLKD_REG,
			I2S_CLKD_MCLKDIV_MASK, I2S_CLKD_MCLKDIV(mclk_div));

	return 0;
}

static int aic_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int word_size = params_width(params);
	unsigned int slot_width = params_physical_width(params);
	unsigned int slots = params_channels(params);
	int ret, sr, sw;
	u32 width;

	if (i2s->slots)
		slots = i2s->slots;

	if (i2s->slot_width)
		slot_width = i2s->slot_width;

	ret = aic_i2s_set_chan_cfg(i2s, params);
	if (ret < 0) {
		dev_err(dai->dev, "Invalid channel configuration\n");
		return ret;
	}

	switch (params_physical_width(params)) {
	case 8:
		width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case 16:
		width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case 32:
		width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		dev_err(dai->dev, "Unsupported physical sample width: %d\n",
				params_physical_width(params));
		return -EINVAL;
	}
	i2s->playback_dma_data.addr_width = width;
	i2s->capture_dma_data.addr_width = width;

	/* Set LRCK_PERIOD */
	if (i2s->bclk_ratio)
		aic_set_lrck_period(i2s, i2s->bclk_ratio);
	else
		aic_set_lrck_period(i2s, slots * slot_width);

	/* Set sample resolution and slot width */
	sr = aic_i2s_get_sr(i2s, word_size);
	if (sr < 0)
		return -EINVAL;

	sw = aic_i2s_get_sw(i2s, slot_width);
	if (sw < 0)
		return -EINVAL;

	regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_SR_MASK, I2S_FMT0_SR(sr));
	regmap_update_bits(i2s->regmap, I2S_FMT0_REG,
					I2S_FMT0_SW_MASK, I2S_FMT0_SW(sw));

	/* Set bclk, mclk and PLL_Audio clock */
	ret = aic_i2s_set_clk(dai, params_rate(params), slots, slot_width);
	if (ret < 0) {
		dev_err(dai->dev, "aic set I2S clock error\n");
		return ret;
	}

	return 0;
}

static void aic_i2s_start_capture(struct aic_i2s *i2s)
{
	/* Flush RX FIFO */
	regmap_update_bits(i2s->regmap, I2S_FCTL_REG,
					I2S_FCTL_FRX, I2S_FCTL_FRX);
	/* Clear RX counter */
	regmap_write(i2s->regmap, I2S_RXCNT_REG, 0);
	/* Enable RX block */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG,
					I2S_CTL_RXEN, I2S_CTL_RXEN);
	/* Enable RX DRQ */
	regmap_update_bits(i2s->regmap, I2S_INT_REG,
					I2S_INT_RXDRQ_EN, I2S_INT_RXDRQ_EN);
}

static void aic_i2s_start_playback(struct aic_i2s *i2s)
{
	/* Flush TX FIFO */
	regmap_update_bits(i2s->regmap, I2S_FCTL_REG,
					I2S_FCTL_FTX, I2S_FCTL_FTX);
	/* Clear TX counter */
	regmap_write(i2s->regmap, I2S_TXCNT_REG, 0);
	/* Enable TX block */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG,
					I2S_CTL_TXEN, I2S_CTL_TXEN);
	/* Enable TX DRQ */
	regmap_update_bits(i2s->regmap, I2S_INT_REG,
					I2S_INT_TXDRQ_EN, I2S_INT_TXDRQ_EN);
	/* Enable DOUT */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG,
					I2S_CTL_DOUT_EN, I2S_CTL_DOUT_EN);
}

static void aic_i2s_stop_capture(struct aic_i2s *i2s)
{
	/* Disable RX block */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG, I2S_CTL_RXEN, 0);
	/* Disable RX DRQ */
	regmap_update_bits(i2s->regmap, I2S_INT_REG, I2S_INT_RXDRQ_EN, 0);
}

static void aic_i2s_stop_playback(struct aic_i2s *i2s)
{
	/* Disable TX block */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG, I2S_CTL_TXEN, 0);
	/* Disable TX DRQ */
	regmap_update_bits(i2s->regmap, I2S_INT_REG, I2S_INT_TXDRQ_EN, 0);
	/* Disable DOUT */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG, I2S_CTL_DOUT_EN, 0);
}

static int aic_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
				    struct snd_soc_dai *dai)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			aic_i2s_start_playback(i2s);
		else
			aic_i2s_start_capture(i2s);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			aic_i2s_stop_playback(i2s);
		else
			aic_i2s_stop_capture(i2s);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aic_i2s_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	int ret;

	/* Make sure that the period bytes are 8/128 bytes aligned according to
	 * the DMA transfer requested.
	 */
	if (of_device_is_compatible(dai->dev->of_node,
		"artinchip,aic-i2s-v1.0")) {
		ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 8);
		if (ret < 0) {
			dev_err(dai->dev,
				"Could not apply period step: %d\n", ret);
			return ret;
		}

		ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 8);
		if (ret < 0) {
			dev_err(dai->dev,
				"Could not apply buffer step: %d\n", ret);
			return ret;
		}
	} else {
		ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 128);
		if (ret < 0) {
			dev_err(dai->dev,
				"Could not apply period step: %d\n", ret);
			return ret;
		}

		ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 128);
		if (ret < 0) {
			dev_err(dai->dev,
				"Could not apply buffer step: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static const struct snd_soc_dai_ops aic_i2s_dai_ops = {
	.startup = aic_i2s_startup,
	.set_sysclk = aic_i2s_set_sysclk,
	.set_bclk_ratio = aic_i2s_set_bclk_ratio,
	.set_fmt = aic_i2s_set_fmt,
	.set_tdm_slot = aic_i2s_set_tdm_slot,
	.hw_params = aic_i2s_hw_params,
	.trigger = aic_i2s_trigger,
};

static int aic_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct aic_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai, &i2s->playback_dma_data,
					&i2s->capture_dma_data);

	snd_soc_dai_set_drvdata(dai, i2s);
	return 0;
}

static struct snd_soc_dai_driver aic_i2s_dai = {
	.probe = aic_i2s_dai_probe,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 16,
		.rates = SNDRV_PCM_RATE_8000_384000,
		.formats = SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_LE |
			   SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 16,
		.rates = SNDRV_PCM_RATE_8000_384000,
		.formats = SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_LE |
			   SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &aic_i2s_dai_ops,
};

static const struct snd_soc_component_driver aic_i2s_component = {
	.name = "aic_i2s_comp",
};

static const struct snd_pcm_hardware aic_i2s_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_INTERLEAVED,
	.buffer_bytes_max	= 128 * 1024,
	.period_bytes_max	= 64 * 1024,
	.period_bytes_min	= 256,
	.periods_max		= 255,
	.periods_min		= 2,
	.fifo_size		= 0,
};

static const struct snd_dmaengine_pcm_config aic_i2s_dmaengine_pcm_config = {
	.pcm_hardware = &aic_i2s_pcm_hardware,
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
	.prealloc_buffer_size = 128 * 1024,
};

static bool aic_i2s_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXFIFO_REG:
		return false;
	default:
		return true;
	}
}

static bool aic_i2s_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_RXFIFO_REG:
	case I2S_FSTA_REG:
		return false;
	default:
		return true;
	}
}

static bool aic_i2s_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_RXFIFO_REG:
	case I2S_ISTA_REG:
	case I2S_TXCNT_REG:
	case I2S_RXCNT_REG:
		return true;
	default:
		return false;
	}
}

static const struct reg_default aic_i2s_reg_default[] = {
	{ I2S_CTL_REG, 0x00060000 },
	{ I2S_FMT0_REG, 0x00000033 },
	{ I2S_FMT1_REG, 0x00000030 },
	{ I2S_ISTA_REG, 0x00000010 },
	{ I2S_FCTL_REG, 0x000400f0 },
	{ I2S_INT_REG, 0x00000000 },
	{ I2S_CLKD_REG, 0x00000000 },
	{ I2S_CHCFG_REG, 0x00000000 },
	{ I2S_TXCHSEL_REG, 0x00000000 },
	{ I2S_TXCHMAP0_REG, 0x00000000 },
	{ I2S_TXCHMAP1_REG, 0x00000000 },
	{ I2S_RXCHSEL_REG, 0x00000000 },
	{ I2S_RXCHMAP0_REG, 0x00000000 },
	{ I2S_RXCHMAP1_REG, 0x00000000 },
};

static const struct regmap_config aic_i2s_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = I2S_RXCHMAP1_REG,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = aic_i2s_reg_default,
	.num_reg_defaults = ARRAY_SIZE(aic_i2s_reg_default),
	.writeable_reg = aic_i2s_wr_reg,
	.readable_reg = aic_i2s_rd_reg,
	.volatile_reg = aic_i2s_volatile_reg,
};

static int aic_i2s_runtime_resume(struct device *dev)
{
	struct aic_i2s *i2s = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(i2s->clk);
	if (ret) {
		dev_err(dev, "Failed to enable I2S clock");
		goto err_disable_clk;
	}

	/* Enable the whole hardware block */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG, I2S_CTL_GEN, I2S_CTL_GEN);
	return 0;

err_disable_clk:
	clk_disable_unprepare(i2s->clk);
	return ret;
}

static int aic_i2s_runtime_suspend(struct device *dev)
{
	struct aic_i2s *i2s = dev_get_drvdata(dev);

	/* Disable the whole hardware block */
	regmap_update_bits(i2s->regmap, I2S_CTL_REG, I2S_CTL_GEN, 0);

	regcache_cache_only(i2s->regmap, true);
	clk_disable_unprepare(i2s->clk);
	return 0;
}

static int aic_i2s_probe(struct platform_device *pdev)
{
	struct aic_i2s *i2s;
	struct resource *res;
	void __iomem *regs;
	int ret;

	i2s = devm_kzalloc(&pdev->dev, sizeof(*i2s), GFP_KERNEL);
	if (!i2s)
		return -ENOMEM;
	platform_set_drvdata(pdev, i2s);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	i2s->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
						&aic_i2s_regmap_config);
	if (IS_ERR(i2s->regmap)) {
		dev_err(&pdev->dev, "Regmap initialization failed\n");
		return PTR_ERR(i2s->regmap);
	}

	/* Get I2S clock structure */
	i2s->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2s->clk)) {
		dev_err(&pdev->dev, "Cann't get I2S module clock\n");
		return PTR_ERR(i2s->clk);
	}

	ret = clk_prepare_enable(i2s->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable I2S clock");
		return ret;
	}

	/* Get I2S reset control */
	i2s->rst = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(i2s->rst)) {
		dev_err(&pdev->dev, "Failed to get I2S reset control\n");
		//return PTR_ERR(i2s->rst);
		goto err_suspend;
	}

	ret = reset_control_deassert(i2s->rst);
	if (ret) {
		dev_err(&pdev->dev, "Failed to deassert I2S reset control\n");
		goto err_suspend;
	}

	/* Enable MCLK OUT enable */
	regmap_update_bits(i2s->regmap, I2S_CLKD_REG,
					I2S_CLKD_MCLKO_EN, I2S_CLKD_MCLKO_EN);
	/* Configure valid data to occupy the low bytes of TX FIFO */
	regmap_update_bits(i2s->regmap, I2S_FCTL_REG,
					I2S_FCTL_TXIM, I2S_FCTL_TXIM);
	/* Configure valid data to occupy the low bytes of RX FIFO */
	regmap_update_bits(i2s->regmap, I2S_FCTL_REG,
					I2S_FCTL_RXOM_MASK, I2S_FCTL_RXOM(1));

	i2s->playback_dma_data.addr = res->start + I2S_TXFIFO_REG;
	i2s->playback_dma_data.maxburst = 1;
	i2s->capture_dma_data.addr = res->start + I2S_RXFIFO_REG;
	i2s->capture_dma_data.maxburst = 1;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = aic_i2s_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev,
						&aic_i2s_dmaengine_pcm_config, 0);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM DMA\n");
		goto err_pm_disable;
	}

	ret = devm_snd_soc_register_component(&pdev->dev,
						&aic_i2s_component,
						&aic_i2s_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Could not register CPU DAI\n");
		goto err_pm_disable;
	}
	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		aic_i2s_runtime_suspend(&pdev->dev);

	if (!IS_ERR(i2s->rst))
		reset_control_assert(i2s->rst);

	return ret;
}

static int aic_i2s_remove(struct platform_device *pdev)
{
	struct aic_i2s *i2s = dev_get_drvdata(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		aic_i2s_runtime_suspend(&pdev->dev);

	if (!IS_ERR(i2s->rst))
		reset_control_assert(i2s->rst);

	return 0;
}

static const struct of_device_id aic_i2s_match[] = {
	{
		.compatible = "artinchip,aic-i2s-v0.1",
	},
	{
		.compatible = "artinchip,aic-i2s-v1.0",
	},
	{}
};

MODULE_DEVICE_TABLE(of, aic_i2s_match);

static const struct dev_pm_ops aic_i2s_pm_ops = {
	.runtime_resume = aic_i2s_runtime_resume,
	.runtime_suspend = aic_i2s_runtime_suspend,
};

static struct platform_driver aic_i2s_driver = {
	.probe = aic_i2s_probe,
	.remove = aic_i2s_remove,
	.driver = {
		.name = "aic-i2s",
		.of_match_table = aic_i2s_match,
		.pm = &aic_i2s_pm_ops,
	}
};

module_platform_driver(aic_i2s_driver);
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_DESCRIPTION("ArtInChip aic I2S driver");
MODULE_LICENSE("GPL");

