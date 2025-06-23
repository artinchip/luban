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
#include <linux/init.h>
#include <linux/kernel.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/gpio/consumer.h>

/* codec register definition */
#define RX_DMIC_IF_CTRL_REG					(0x00)
#define RX_DMIC_IF_ADOUT_SHIFT_EN				BIT(15)
#define RX_DMIC_IF_ADOUT_SHIFT_MASK				GENMASK(14, 12)
#define RX_DMIC_IF_DLT_LENGTH					(10)
#define RX_DMIC_IF_DMIC_RX_DLT_EN				(9)
#define RX_DMIC_IF_DEC2_FLT					(7)
#define RX_DMIC_IF_DEC1_FLT					(6)
#define RX_DMIC_IF_DEC_EN_MASK					GENMASK(7, 6)
#define RX_DMIC_IF_DEC_EN_ALL					(3 << 6)
#define RX_DMIC_IF_DATA_SWAP					(5)
#define RX_DMIC_IF_EN						(4)
#define RX_DMIC_IF_FS_IN_MASK					GENMASK(3, 1)
#define RX_DMIC_IF_FS_IN(fs)					((fs) << 1)
#define RX_DMIC_IF_RX_CLK_MASK					BIT(0)
#define RX_DMIC_IF_RX_CLK_24576KHZ				(0)
#define RX_DMIC_IF_RX_CLK_22579KHZ				(1)

#define RX_HPF_1_2_CTRL_REG					(0x04)
#define RX_HPF2_EN						(1)
#define RX_HPF1_EN						(0)

#define RX_HPF1_COEFF_REG					(0x08)
#define RX_HPF2_COEFF_REG					(0x0C)
#define RX_HPF1_GAIN_REG					(0x10)
#define RX_HPF2_GAIN_REG					(0x14)

#define RX_DVC_1_2_CTRL_REG					(0x18)
#define RX_DVC2_GAIN						(24)
#define RX_DVC1_GAIN						(16)
#define RX_DVC2_EN						(1)
#define RX_DVC1_EN						(0)

#define TX_MIXER_CTRL_REG					(0x1C)
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
#define TX_MIXER0_EN						(31)
#define TX_MIXER1_EN						(30)
#define TX_MIXER1_ADCOUT_GAIN					(28)
#define TX_MIXER1_DMICOUTR_GAIN					(27)
#define TX_MIXER1_DMICOUTL_GAIN					(26)
#define TX_MIXER1_AUDOUTR_GAIN					(25)
#define TX_MIXER1_AUDOUTL_GAIN					(24)
#define TX_MIXER0_ADCOUT_GAIN					(20)
#define TX_MIXER1_ADCOUT_SEL					(12)
#define TX_MIXER1_DMICOUTR_SEL					(11)
#define TX_MIXER1_DMICOUTL_SEL					(10)
#define TX_MIXER1_AUDOUTR_SEL					(9)
#define TX_MIXER1_AUDOUTL_SEL					(8)
#define TX_MIXER0_ADCOUT_SEL					(4)
#define TX_MIXER0_DMICOUTR_SEL					(3)
#define TX_MIXER0_DMICOUTL_SEL					(2)
#define TX_MIXER0_AUDOUTR_SEL					(1)
#define TX_MIXER0_AUDOUTL_SEL					(0)
#else
#define TX_MIXER1_DMICOUTR_GAIN					(23)
#define TX_MIXER1_DMICOUTL_GAIN					(22)
#define TX_MIXER1_AUDOUTR_GAIN					(21)
#define TX_MIXER1_AUDOUTL_GAIN					(20)
#define TX_MIXER1_DMICOUTR_SEL					(15)
#define TX_MIXER1_DMICOUTL_SEL					(14)
#define TX_MIXER1_AUDOUTR_SEL					(13)
#define TX_MIXER1_AUDOUTL_SEL					(12)
#define TX_MIXER0_DMICOUTR_SEL					(11)
#define TX_MIXER0_DMICOUTL_SEL					(10)
#define TX_MIXER0_AUDOUTR_SEL					(9)
#define TX_MIXER0_AUDOUTL_SEL					(8)
#define TX_MIXER1_EN						(1)
#define TX_MIXER0_EN						(0)
#endif
#define TX_MIXER0_DMICOUTR_GAIN					(19)
#define TX_MIXER0_DMICOUTL_GAIN					(18)
#define TX_MIXER0_AUDOUTR_GAIN					(17)
#define TX_MIXER0_AUDOUTL_GAIN					(16)

#define TX_DVC_3_4_CTRL_REG					(0x20)
#define TX_DVC4_GAIN						(24)
#define TX_DVC3_GAIN						(16)
#define TX_DVC4_EN						(1)
#define TX_DVC3_EN						(0)

#define TX_PLAYBACK_CTRL_REG					(0x24)
#define TX_DLT							(12)
#define TX_IF_CH1_EN						(6)
#define TX_IF_CH0_EN						(5)
#define TX_PLAYBACK_IF_EN					(4)
#define TX_IF_EN_MASK						GENMASK(6, 4)
#define TX_IF_EN_ALL						(7 << 4)
#define TX_IF_EN_CH0						(3 << 4)
#define TX_FS_OUT_MASK						GENMASK(3, 1)
#define TX_FS_OUT(fs)						((fs) << 1)
#define TX_CLK_MASK						BIT(0)
#define TX_CLK_24576KHZ						(0)
#define TX_CLK_22579KHZ						(1)

#define TX_SDM_CTRL_REG						(0x28)
#define TX_SDM_CH1_EN						(1)
#define TX_SDM_CH0_EN						(0)

#define TX_PWM_CTRL_REG						(0x2C)
#define TX_PWM1_MODE						(6)
#define TX_PWM1_DIFEN						(5)
#define TX_PWM1_EN						(4)
#define TX_PWM0_MODE						(2)
#define TX_PWM0_DIFEN						(1)
#define TX_PWM0_EN						(0)

#define DMIC_RXFIFO_CTRL_REG					(0x30)
#define DMIC_RXFIFO_FLUSH					BIT(31)
#define DMIC_RXFIFO_CH1_EN					(1)
#define DMIC_RXFIFO_CH0_EN					(0)

#define TXFIFO_CTRL_REG						(0x34)
#define TXFIFO_FLUSH						BIT(31)
#define TXFIFO_CH1_EN						(1)
#define TXFIFO_CH0_EN						(0)

#define FIFO_INT_EN_REG						(0x38)
#define FIFO_AUDOUT_DRQ_EN					BIT(7)
#define FIFO_DMICIN_DRQ_EN					BIT(3)

#define FIFO_STA_REG						(0x3C)
#define TXFIFO_SPACE_SHIFT					(16)
#define TXFIFO_SPACE_MAX					(0x80)

#define DMIC_RXFIFO_DATA_REG					(0x40)
#define DMIC_RX_CNT_REG						(0x44)
#define TXFIFO_DATA_REG						(0x48)
#define TX_CNT_REG						(0x4C)

#define FADE_CTRL0_REG						(0x58)
#define FADE_CTRL0_STEP_MASK					GENMASK(30, 16)
#define FADE_CTRL0_STEP(step)					((step) << 16)
#define FADE_CTRL0_CH1_EN					(2)
#define FADE_CTRL0_CH0_EN					(1)
#define FADE_CTRL0_EN						(0)
#define FADE_CTRL0_MASK						GENMASK(2, 0)
#define FADE_CTRL0_DISABLE					(0)

#define FADE_CTRL1_REG						(0x5C)
#define FADE_CTRL1_TARGET_VOL_MASK				GENMASK(14, 0)
#define FADE_CTRL1_TARGET_VOL(vol)				((vol) << 0)
#define FADE_CTRL1_GAIN						(0)

#define GLOBE_CTL_REG						(0x60)
#define GLOBE_GLB_RST						BIT(2)
#define GLOBE_TX_GLBEN						1
#define GLOBE_RX_GLBEN						0

#define ADC_IF_CTRL_REG						(0x70)
#define ADC_IF_CTRL_FILT_SEL					(16)
#define ADC_IF_CTRL_ADOUT_SHIFT_EN				(15)
#define ADC_IF_CTRL_ADOUT_SHIFT					(12)
#define ADC_IF_CTRL_ADC_RX_DLT					(10)
#define ADC_IF_CTRL_ADC_RX_DLT_EN				(9)
#define ADC_IF_CTRL_EN_DEC0_MASK				BIT(6)
#define ADC_IF_CTRL_FS_ADC_IN_MASK				GENMASK(3, 1)
#define ADC_IF_CTRL_FS_ADC_IN(fs)				((fs) << 1)
#define ADC_IF_CTRL_RX_CLK_FRE_MASK				BIT(0)
#define ADC_IF_CTRL_RX_CLK_24576KHZ				(0)
#define ADC_IF_CTRL_RX_CLK_22579KHZ				(1)

#define ADC_HPF0_CTRL_REG					(0x74)
#define ADC_HPF0_CTRL_HPF0_EN					(0)

#define ADC_DVC0_CTRL_REG					(0x80)
#define ADC_DVC0_CTRL_DVC0_GAIN					(16)
#define ADC_DVC0_CTRL_DVC0_EN					(0)

#define ADC_RXFIFO_CTRL_REG					(0x84)
#define ADC_RXFIFO_FLUSH					BIT(31)
#define ADC_RXFIFO_RXTH						(8)
#define ADC_RXFIFO_EN						(0)

#define ADC_RXFIFO_INT_EN_REG					(0x88)
#define ADC_RXFIFO_ADCIN_DRQ_EN					BIT(3)

#define ADC_RXFIFO_STA_REG					(0x8C)
#define ADC_RXFIFO_SPACE_CNT					(0)

#define ADC_RXFIFO_DATA_REG					(0x90)
#define ADC_RXFIFO_DATA_CNT_REG					(0x94)

#define ADC_CTL1_REG						(0xA0)
#define ADC_CTL1_MBIAS_EN					(2)
#define ADC_CTL1_PGA_EN						(1)
#define ADC_CTL1_ADC_EN						(0)

#define ADC_CTL2_REG						(0xA4)
#define ADC_CTL2_MBIAS_CTL					(8)
#define ADC_CTL2_PGA_GAIN_SEL					(0)
#define ADC_CTL2_PGA_GAIN_MASK					GENMASK(3, 0)

#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
#define CODEC_VERSION_REG					(0xFC)
#else
#define CODEC_VERSION_REG					(0x64)
#endif
#define DEFAULT_AUDIO_FREQ					(24576000)
static struct gpio_desc *gpiod_pa;

struct aic_codec {
	struct device *dev;
	struct regmap *regmap;
	struct clk *clk;
	struct reset_control *rst;
	struct resource *res;
	struct snd_dmaengine_dai_dma_data  capture_dma_dmic;
	struct snd_dmaengine_dai_dma_data  capture_dma_adc;
	struct snd_dmaengine_dai_dma_data  playback_dma;
};

static void aic_codec_start_playback(struct aic_codec *codec)
{
	regmap_update_bits(codec->regmap, FADE_CTRL0_REG,
			FADE_CTRL0_STEP_MASK, FADE_CTRL0_STEP(0x80));
	regmap_update_bits(codec->regmap, FADE_CTRL1_REG,
			FADE_CTRL1_TARGET_VOL_MASK, 0x7FFF);
	/* Enable AUDOUT DRQ */
	regmap_update_bits(codec->regmap, FIFO_INT_EN_REG,
			FIFO_AUDOUT_DRQ_EN, FIFO_AUDOUT_DRQ_EN);
}

static void aic_codec_stop_playback(struct aic_codec *codec)
{
	unsigned int space_cnt;
	unsigned int cnt = 5000;

	regmap_update_bits(codec->regmap, FADE_CTRL0_REG,
			FADE_CTRL0_STEP_MASK, FADE_CTRL0_STEP(0x3FFF));
	regmap_update_bits(codec->regmap, FADE_CTRL1_REG,
			FADE_CTRL1_TARGET_VOL_MASK, 0);

	/* Disable AUDOUT DRQ */
	regmap_update_bits(codec->regmap, FIFO_INT_EN_REG,
			FIFO_AUDOUT_DRQ_EN, 0);
	/* Waiting for all the data in the FIFO to be sent out */
	do {
		regmap_read(codec->regmap, FIFO_STA_REG, &space_cnt);
		space_cnt &= 0xff << TXFIFO_SPACE_SHIFT;
		space_cnt >>= TXFIFO_SPACE_SHIFT;
	} while (space_cnt != TXFIFO_SPACE_MAX && cnt--);
}

static void aic_codec_dmic_start_capture(struct aic_codec *codec)
{
	/* Enable ADOUT SHIFT */
	regmap_update_bits(codec->regmap, RX_DMIC_IF_CTRL_REG,
			RX_DMIC_IF_ADOUT_SHIFT_EN, RX_DMIC_IF_ADOUT_SHIFT_EN);
	/* Enable DMIC Decimation Filter */
	regmap_update_bits(codec->regmap, RX_DMIC_IF_CTRL_REG,
			RX_DMIC_IF_DEC_EN_MASK, RX_DMIC_IF_DEC_EN_ALL);
	/* Enable DMICIN DRQ */
	regmap_update_bits(codec->regmap, FIFO_INT_EN_REG,
				FIFO_DMICIN_DRQ_EN, FIFO_DMICIN_DRQ_EN);
}

static void aic_codec_dmic_stop_capture(struct aic_codec *codec)
{
	/* Disable DMICIN DRQ */
	regmap_update_bits(codec->regmap, FIFO_INT_EN_REG,
				FIFO_DMICIN_DRQ_EN, 0);
	/* Disable DMIC Decimation Filter */
	regmap_update_bits(codec->regmap, RX_DMIC_IF_CTRL_REG,
				RX_DMIC_IF_DEC_EN_MASK, 0);
}

static void aic_codec_adc_start_capture(struct aic_codec *codec)
{
	/* Enable ADC Decimation Filter */
	regmap_update_bits(codec->regmap, ADC_IF_CTRL_REG,
			ADC_IF_CTRL_EN_DEC0_MASK, ADC_IF_CTRL_EN_DEC0_MASK);
	/* Enable ADC DRQ */
	regmap_update_bits(codec->regmap, ADC_RXFIFO_INT_EN_REG,
			ADC_RXFIFO_ADCIN_DRQ_EN, ADC_RXFIFO_ADCIN_DRQ_EN);
}

static void aic_codec_adc_stop_capture(struct aic_codec *codec)
{
	/* Disable ADC DRQ */
	regmap_update_bits(codec->regmap, ADC_RXFIFO_INT_EN_REG,
				ADC_RXFIFO_ADCIN_DRQ_EN, 0);
	/* Disable ADC Decimation Filter */
	regmap_update_bits(codec->regmap, ADC_IF_CTRL_REG,
				ADC_IF_CTRL_EN_DEC0_MASK, 0);
}

static int aic_codec_trigger(struct snd_pcm_substream *substream,
					int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aic_codec *codec = snd_soc_card_get_drvdata(rtd->card);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			aic_codec_start_playback(codec);
		else if (!dai->id)
			aic_codec_dmic_start_capture(codec);
		else
			aic_codec_adc_start_capture(codec);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			aic_codec_stop_playback(codec);
		else if (!dai->id)
			aic_codec_dmic_stop_capture(codec);
		else
			aic_codec_adc_stop_capture(codec);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int aic_codec_get_mod_freq(struct aic_codec *codec,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *dai)
{
	unsigned int rate = params_rate(params);

	switch (rate) {
	case 48000:
	case 32000:
	case 24000:
	case 16000:
	case 12000:
	case 8000:
		/* Set TX and RX module clock frequency */
		if (!dai->id)
			regmap_update_bits(codec->regmap, RX_DMIC_IF_CTRL_REG,
					RX_DMIC_IF_RX_CLK_MASK,
					RX_DMIC_IF_RX_CLK_24576KHZ);
		else
			regmap_update_bits(codec->regmap, ADC_IF_CTRL_REG,
					ADC_IF_CTRL_RX_CLK_FRE_MASK,
					ADC_IF_CTRL_RX_CLK_24576KHZ);

		regmap_update_bits(codec->regmap, TX_PLAYBACK_CTRL_REG,
					TX_CLK_MASK, TX_CLK_24576KHZ);

		return 24576000;
	case 44100:
	case 22050:
	case 11025:
		/* Set TX and RX module clock frequency */
		if (!dai->id)
			regmap_update_bits(codec->regmap, RX_DMIC_IF_CTRL_REG,
					RX_DMIC_IF_RX_CLK_MASK,
					RX_DMIC_IF_RX_CLK_22579KHZ);
		else
			regmap_update_bits(codec->regmap, ADC_IF_CTRL_REG,
					ADC_IF_CTRL_RX_CLK_FRE_MASK,
					ADC_IF_CTRL_RX_CLK_22579KHZ);

		regmap_update_bits(codec->regmap, TX_PLAYBACK_CTRL_REG,
			TX_CLK_MASK, TX_CLK_22579KHZ);
		return 22579200;
	default:
		return 0;
	}
}

static int aic_codec_get_hw_rate(struct snd_pcm_hw_params *params)
{
	unsigned int rate = params_rate(params);

	switch (rate) {
	case 48000:
	case 44100:
		return 0;
	case 32000:
		return 1;
	case 24000:
	case 22050:
		return 2;
	case 16000:
		return 3;
	case 12000:
	case 11025:
		return 4;
	case 8000:
		return 5;
	default:
		return -EINVAL;
	}
}

static int aic_codec_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aic_codec *codec = snd_soc_card_get_drvdata(rtd->card);
	unsigned int clk_freq, width;
	int ret, hwrate;
	struct clk *parent_clk;
	unsigned long freq;
	int pa_status;

	clk_freq = aic_codec_get_mod_freq(codec, params, dai);
	if (!clk_freq) {
		dev_err(codec->dev, "clk_freq: %d\n", clk_freq);
		return -EINVAL;
	}

	parent_clk = clk_get_parent(codec->clk);
	if (!parent_clk) {
		dev_err(codec->dev, "Cannot get AudioCodec parent clock\n");
		return -EINVAL;
	}

	freq = clk_get_rate(parent_clk);
	if (abs(clk_freq * 20 - freq) > 2000) {
		if (!IS_ERR_OR_NULL(gpiod_pa)) {
			pa_status = gpiod_get_value(gpiod_pa);
			/* If PA is on, disable PA first to prevent pop */
			if (pa_status)
				gpiod_set_value(gpiod_pa, 0);
		}

		/* Set AudioCodec parent clock rate */
		ret = clk_set_rate(parent_clk, clk_freq * 20);
		if (ret)
			return ret;
		msleep(100);

		/* Enable PA after frequency changed */
		if (!IS_ERR_OR_NULL(gpiod_pa))
			gpiod_set_value(gpiod_pa, pa_status);
	}

	/* Set codec module clock frequency */
	ret = clk_set_rate(codec->clk, clk_freq);
	if (ret) {
		dev_err(codec->dev, "ret: %d\n", ret);
		return ret;
	}

	switch (params_channels(params)) {
	case 1:
		width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case 2:
		width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		dev_err(dai->dev, "Unsupported channel num: %d\n",
				params_physical_width(params));
		return -EINVAL;
	}

	codec->playback_dma.addr_width = width;
	codec->capture_dma_dmic.addr_width = width;

	hwrate = aic_codec_get_hw_rate(params);
	if (hwrate < 0) {
		dev_err(codec->dev, "hwrate: %d\n", hwrate);
		return hwrate;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(codec->regmap, TX_PLAYBACK_CTRL_REG,
				TX_FS_OUT_MASK, TX_FS_OUT(hwrate));
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (!dai->id)
			regmap_update_bits(codec->regmap, RX_DMIC_IF_CTRL_REG,
					RX_DMIC_IF_FS_IN_MASK,
					RX_DMIC_IF_FS_IN(hwrate));
		else
			regmap_update_bits(codec->regmap, ADC_IF_CTRL_REG,
					ADC_IF_CTRL_FS_ADC_IN_MASK,
					ADC_IF_CTRL_FS_ADC_IN(hwrate));
	}

	return 0;
}

static int aic_codec_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	int ret;

	/* Make sure that the period bytes are 8/128 bytes aligned according to
	 * the DMA transfer requested.
	 */
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 8);
	if (ret < 0) {
		dev_err(dai->dev, "Could not apply period step: %d\n", ret);
		return ret;
	}

	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 8);
	if (ret < 0) {
		dev_err(dai->dev, "Could not apply buffer step: %d\n", ret);
		return ret;
	}
#else
	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 128);
	if (ret < 0) {
		dev_err(dai->dev, "Could not apply period step: %d\n", ret);
		return ret;
	}

	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 128);
	if (ret < 0) {
		dev_err(dai->dev, "Could not apply buffer step: %d\n", ret);
		return ret;
	}
#endif

	return ret;
}

static const struct snd_soc_dai_ops aic_codec_dai_ops = {
	.startup = aic_codec_startup,
	.trigger = aic_codec_trigger,
	.hw_params = aic_codec_hw_params,
};

static struct snd_soc_dai_driver aic_codec_dai[] = {
	{
		.name = "codec-dmic",
		.ops = &aic_codec_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "Capture-dmic",
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	{
		.name = "codec-adc",
		.ops = &aic_codec_dai_ops,
		.capture = {
			.stream_name = "Capture-adc",
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
#endif
};

static const DECLARE_TLV_DB_SCALE(aic_codec_tlv_range_scale,
					-32767, 1, 1);
static const DECLARE_TLV_DB_SCALE(aic_codec_dvc_scale,
					-12000, 75, 1);
static const DECLARE_TLV_DB_SCALE(aic_mixer_source_gain_scale,
					-600, 600, 0);
static const DECLARE_TLV_DB_SCALE(aic_pga_scale, 0, 200, 0);

static const char * const pwm_output_sel[] = {
	"Single_ended", "Differential"
};

static const char * const pwm_mode[] = {
	"PWM mode", "PDM mode"
};

static const char * const mixer_enable[] = {
	"mixer bypass", "mixer enable"
};

static const char * const hpf_sel[] = {
	"Bypass", "HPF Enable"
};

static const char * const dmicif_data_sel[] = {
	"No Swap Data", "Swap Data"
};

static const char * const dmic_rx_dlt_length_sel[] = {
	"5ms", "10ms", "20ms", "30ms"
};

static const char * const dmic_rx_dlt_en_sel[] = {
	"off", "on"
};

static const char * const fade_sel[] = {
	"Bypass", "Fade Enable"
};

static const char * const mbias_ctl[] = {
	"1.8V", "2.2V", "2.0V", "2.4V"
};

static SOC_ENUM_SINGLE_DECL(pwm0_output_enum, TX_PWM_CTRL_REG,
				TX_PWM0_DIFEN, pwm_output_sel);
static SOC_ENUM_SINGLE_DECL(pwm1_output_enum, TX_PWM_CTRL_REG,
				TX_PWM1_DIFEN, pwm_output_sel);
static SOC_ENUM_SINGLE_DECL(pwm0_mode_enum, TX_PWM_CTRL_REG,
				TX_PWM0_MODE, pwm_mode);
static SOC_ENUM_SINGLE_DECL(pwm1_mode_enum, TX_PWM_CTRL_REG,
				TX_PWM1_MODE, pwm_mode);
static SOC_ENUM_DOUBLE_DECL(hpf_enum, RX_HPF_1_2_CTRL_REG,
				RX_HPF1_EN, RX_HPF2_EN, hpf_sel);
static SOC_ENUM_SINGLE_DECL(dmicif_data_enum, RX_DMIC_IF_CTRL_REG,
				RX_DMIC_IF_DATA_SWAP, dmicif_data_sel);
static SOC_ENUM_SINGLE_DECL(dmic_rx_dlt_length_enum, RX_DMIC_IF_CTRL_REG,
				RX_DMIC_IF_DLT_LENGTH, dmic_rx_dlt_length_sel);
static SOC_ENUM_SINGLE_DECL(dmic_rx_dlt_en_enum, RX_DMIC_IF_CTRL_REG,
				RX_DMIC_IF_DMIC_RX_DLT_EN, dmic_rx_dlt_en_sel);
static SOC_ENUM_SINGLE_DECL(fade0_enum, FADE_CTRL0_REG,
				FADE_CTRL0_CH0_EN, fade_sel);
static SOC_ENUM_SINGLE_DECL(fade1_enum, FADE_CTRL0_REG,
				FADE_CTRL0_CH1_EN, fade_sel);
static SOC_ENUM_SINGLE_DECL(adc_hpf_enum, ADC_HPF0_CTRL_REG,
				ADC_HPF0_CTRL_HPF0_EN, hpf_sel);
static SOC_ENUM_SINGLE_DECL(mic_bias_voltage_enum, ADC_CTL2_REG,
				ADC_CTL2_MBIAS_CTL, mbias_ctl);
static SOC_ENUM_SINGLE_DECL(mixer0_enable_enum, TX_MIXER_CTRL_REG,
				TX_MIXER0_EN, mixer_enable);
static SOC_ENUM_SINGLE_DECL(mixer1_enable_enum, TX_MIXER_CTRL_REG,
				TX_MIXER1_EN, mixer_enable);

static const struct snd_kcontrol_new pwm0_output_mux =
		SOC_DAPM_ENUM("Route", pwm0_output_enum);
static const struct snd_kcontrol_new pwm1_output_mux =
		SOC_DAPM_ENUM("Route", pwm1_output_enum);
static const struct snd_kcontrol_new hpf_mux =
		SOC_DAPM_ENUM("Route", hpf_enum);
static const struct snd_kcontrol_new fade0_mux =
		SOC_DAPM_ENUM("Route", fade0_enum);
static const struct snd_kcontrol_new fade1_mux =
		SOC_DAPM_ENUM("Route", fade1_enum);
static const struct snd_kcontrol_new adc_hpf_mux =
		SOC_DAPM_ENUM("Route", adc_hpf_enum);

static const struct snd_kcontrol_new aic_codec_controls[] = {
	SOC_DOUBLE_TLV("DMICIN Capture Volume", RX_DVC_1_2_CTRL_REG,
					RX_DVC1_GAIN, RX_DVC2_GAIN,
					0xFF, 0, aic_codec_dvc_scale),
	SOC_SINGLE_TLV("AUDIO Playback Volume", FADE_CTRL1_REG,
					FADE_CTRL1_GAIN,
					0x7FFF, 0, aic_codec_tlv_range_scale),
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	SOC_SINGLE_TLV("ADC Capture Volume", ADC_DVC0_CTRL_REG,
					ADC_DVC0_CTRL_DVC0_GAIN,
					0xFF, 0, aic_codec_dvc_scale),
	SOC_SINGLE_TLV("PGA Gain", ADC_CTL2_REG, ADC_CTL2_PGA_GAIN_SEL,
					0xF, 0, aic_pga_scale),
#endif
	SOC_SINGLE_TLV("MIX1AUDL Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER1_AUDOUTL_GAIN, 1,
					1, aic_mixer_source_gain_scale),
	SOC_SINGLE_TLV("MIX1AUDR Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER1_AUDOUTR_GAIN, 1,
					1, aic_mixer_source_gain_scale),
	SOC_SINGLE_TLV("MIX1DMICL Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER1_DMICOUTL_GAIN, 1,
					1, aic_mixer_source_gain_scale),
	SOC_SINGLE_TLV("MIX1DMICR Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER1_DMICOUTR_GAIN, 1,
					1, aic_mixer_source_gain_scale),
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	SOC_SINGLE_TLV("MIX1ADC Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER1_ADCOUT_GAIN, 1,
					1, aic_mixer_source_gain_scale),
#endif
	SOC_SINGLE_TLV("MIX0AUDL Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER0_AUDOUTL_GAIN, 1,
					1, aic_mixer_source_gain_scale),
	SOC_SINGLE_TLV("MIX0AUDR Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER0_AUDOUTR_GAIN, 1,
					1, aic_mixer_source_gain_scale),
	SOC_SINGLE_TLV("MIX0DMICL Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER0_DMICOUTL_GAIN, 1,
					1, aic_mixer_source_gain_scale),
	SOC_SINGLE_TLV("MIX0DMICR Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER0_DMICOUTR_GAIN, 1,
					1, aic_mixer_source_gain_scale),
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	SOC_SINGLE_TLV("MIX0ADC Playback Gain", TX_MIXER_CTRL_REG,
					TX_MIXER0_ADCOUT_GAIN, 1,
					1, aic_mixer_source_gain_scale),
#endif
	SOC_ENUM("Data Swap Switch", dmicif_data_enum),
	SOC_ENUM("RXFIFO Delay Time", dmic_rx_dlt_length_enum),
	SOC_ENUM("RXFIFO Dealy Switch", dmic_rx_dlt_en_enum),
	SOC_ENUM("AMIC bias voltage level", mic_bias_voltage_enum),
	SOC_ENUM("PWM0 mode select", pwm0_mode_enum),
	SOC_ENUM("PWM1 mode select", pwm1_mode_enum),
	SOC_ENUM("MIXER0 swicth", mixer0_enable_enum),
	SOC_ENUM("MIXER1 swicth", mixer1_enable_enum),
};

static const struct snd_kcontrol_new aic_codec_mixer1_controls[] = {
	SOC_DAPM_SINGLE("audoutl switch", TX_MIXER_CTRL_REG,
					TX_MIXER1_AUDOUTL_SEL, 1, 0),
	SOC_DAPM_SINGLE("audoutr switch", TX_MIXER_CTRL_REG,
					TX_MIXER1_AUDOUTR_SEL, 1, 0),
	SOC_DAPM_SINGLE("dmicoutl switch", TX_MIXER_CTRL_REG,
					TX_MIXER1_DMICOUTL_SEL, 1, 0),
	SOC_DAPM_SINGLE("dmicoutr switch", TX_MIXER_CTRL_REG,
					TX_MIXER1_DMICOUTR_SEL, 1, 0),
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	SOC_DAPM_SINGLE("adcout switch", TX_MIXER_CTRL_REG,
					TX_MIXER1_ADCOUT_SEL, 1, 0),
#endif
};

static const struct snd_kcontrol_new aic_codec_mixer0_controls[] = {
	SOC_DAPM_SINGLE("audoutl switch", TX_MIXER_CTRL_REG,
					TX_MIXER0_AUDOUTL_SEL, 1, 0),
	SOC_DAPM_SINGLE("audoutr switch", TX_MIXER_CTRL_REG,
					TX_MIXER0_AUDOUTR_SEL, 1, 0),
	SOC_DAPM_SINGLE("dmicoutl switch", TX_MIXER_CTRL_REG,
					TX_MIXER0_DMICOUTL_SEL, 1, 0),
	SOC_DAPM_SINGLE("dmicoutr switch", TX_MIXER_CTRL_REG,
					TX_MIXER0_DMICOUTR_SEL, 1, 0),
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	SOC_DAPM_SINGLE("adcout switch", TX_MIXER_CTRL_REG,
					TX_MIXER0_ADCOUT_SEL, 1, 0),
#endif
};

static const struct snd_soc_dapm_widget aic_codec_dapm_widgets[] = {
	SND_SOC_DAPM_ADC("DMICIF", "Capture-dmic", RX_DMIC_IF_CTRL_REG,
					RX_DMIC_IF_EN, 0),
	/* MUX */
	SND_SOC_DAPM_MUX("PWM ch0", TX_PWM_CTRL_REG, TX_PWM0_EN,
						0, &pwm0_output_mux),
	SND_SOC_DAPM_MUX("PWM ch1", TX_PWM_CTRL_REG, TX_PWM1_EN,
						0, &pwm1_output_mux),
	SND_SOC_DAPM_MUX("HPF", SND_SOC_NOPM, 0, 0, &hpf_mux),
	SND_SOC_DAPM_MUX("FADE ch0", SND_SOC_NOPM, 0, 0, &fade0_mux),
	SND_SOC_DAPM_MUX("FADE ch1", SND_SOC_NOPM, 0, 0, &fade1_mux),
	SND_SOC_DAPM_MUX("ADC HPF", SND_SOC_NOPM, 0, 0, &adc_hpf_mux),

	/* Sigma-Delta Modulation */
	SND_SOC_DAPM_DAC("SDM ch0", "Playback", TX_SDM_CTRL_REG,
					TX_SDM_CH0_EN, 0),
	SND_SOC_DAPM_DAC("SDM ch1", "Playback", TX_SDM_CTRL_REG,
					TX_SDM_CH1_EN, 0),
	/* PGA */
	SND_SOC_DAPM_PGA("DVC 0", ADC_DVC0_CTRL_REG, ADC_DVC0_CTRL_DVC0_EN,
			0, NULL, 0),
	SND_SOC_DAPM_PGA("DVC 1", RX_DVC_1_2_CTRL_REG, RX_DVC1_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DVC 2", RX_DVC_1_2_CTRL_REG, RX_DVC2_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DVC 3", TX_DVC_3_4_CTRL_REG, TX_DVC3_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DVC 4", TX_DVC_3_4_CTRL_REG, TX_DVC4_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGA", ADC_CTL1_REG, ADC_CTL1_PGA_EN, 0, NULL, 0),

	/* Mixer */
	SND_SOC_DAPM_MIXER("MIXER0", SND_SOC_NOPM, 0, 0,
				aic_codec_mixer0_controls,
				ARRAY_SIZE(aic_codec_mixer0_controls)),
	SND_SOC_DAPM_MIXER("MIXER1", SND_SOC_NOPM, 0, 0,
				aic_codec_mixer1_controls,
				ARRAY_SIZE(aic_codec_mixer1_controls)),

	/* SUPPLY */
	SND_SOC_DAPM_SUPPLY("FADE", FADE_CTRL0_REG, FADE_CTRL0_EN, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("IF", TX_PLAYBACK_CTRL_REG, TX_PLAYBACK_IF_EN,
						0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Bias", ADC_CTL1_REG, ADC_CTL1_MBIAS_EN,
						0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX GLBEN", GLOBE_CTL_REG, GLOBE_TX_GLBEN,
						0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX GLBEN", GLOBE_CTL_REG, GLOBE_RX_GLBEN,
						0, NULL, 0),

	SND_SOC_DAPM_DAC("IF ch0", "Playback", TX_PLAYBACK_CTRL_REG,
					TX_IF_CH0_EN, 0),
	SND_SOC_DAPM_DAC("IF ch1", "Playback", TX_PLAYBACK_CTRL_REG,
					TX_IF_CH1_EN, 0),

	/* AIF OUT */
	SND_SOC_DAPM_AIF_OUT("AUDOUTL", "Playback", 0, TXFIFO_CTRL_REG,
							TXFIFO_CH0_EN, 0),
	SND_SOC_DAPM_AIF_OUT("AUDOUTR", "Playback", 1, TXFIFO_CTRL_REG,
							TXFIFO_CH1_EN, 0),

	/* AIF IN */
	SND_SOC_DAPM_AIF_IN("DMICOUTL", "Capture-dmic", 0, DMIC_RXFIFO_CTRL_REG,
						DMIC_RXFIFO_CH0_EN, 0),
	SND_SOC_DAPM_AIF_IN("DMICOUTR", "Capture-dmic", 1, DMIC_RXFIFO_CTRL_REG,
						DMIC_RXFIFO_CH1_EN, 0),
	SND_SOC_DAPM_AIF_IN("ADCOUT", "Capture-adc", 0, ADC_RXFIFO_CTRL_REG,
						ADC_RXFIFO_EN, 0),
	/* ADC */
	SND_SOC_DAPM_ADC("ADC", "ADC Capture-adc", ADC_CTL1_REG,
						ADC_CTL1_ADC_EN, 0),

	SND_SOC_DAPM_INPUT("AMIC"),
	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT0"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT1"),
};

static const struct snd_soc_dapm_route aic_codec_dapm_route[] = {
	{"DMICOUTL", NULL, "RX GLBEN"},
	{"DMICOUTR", NULL, "RX GLBEN"},
	{"DMICIF", NULL, "DMIC"},
	{"HPF", "Bypass", "DMICIF"},
	{"HPF", "HPF Enable", "DMICIF"},
	{"DVC 1", NULL, "HPF"},
	{"DVC 2", NULL, "HPF"},
	{"DMICOUTL", NULL, "DVC 1"},
	{"DMICOUTR", NULL, "DVC 2"},
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	{"ADCOUT", NULL, "RX GLBEN"},
	{"AMIC", NULL, "Mic Bias"},
	{"PGA", NULL, "AMIC"},
	{"ADC", NULL, "PGA"},
	{"ADC HPF", "Bypass", "ADC"},
	{"ADC HPF", "HPF Enable", "ADC"},
	{"DVC 0", NULL, "ADC HPF"},
	{"ADCOUT", NULL, "DVC 0"},
#endif

	{"AUDOUTL", NULL, "TX GLBEN"},
	{"AUDOUTR", NULL, "TX GLBEN"},
	/* MIXER */
	{"MIXER0", "audoutl switch", "AUDOUTL"},
	{"MIXER0", "audoutr switch", "AUDOUTR"},
	{"MIXER0", "dmicoutl switch", "DMICOUTL"},
	{"MIXER0", "dmicoutr switch", "DMICOUTR"},
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	{"MIXER0", "adcout switch", "ADCOUT"},
#endif

	{"MIXER1", "audoutl switch", "AUDOUTL"},
	{"MIXER1", "audoutr switch", "AUDOUTR"},
	{"MIXER1", "dmicoutl switch", "DMICOUTL"},
	{"MIXER1", "dmicoutr switch", "DMICOUTR"},
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	{"MIXER1", "adcout switch", "ADCOUT"},
#endif

	{"FADE ch0", NULL, "FADE"},
	{"FADE ch1", NULL, "FADE"},
	{"IF ch0", NULL, "IF"},
	{"IF ch1", NULL, "IF"},

	{"DVC 3", NULL, "MIXER0"},
	{"IF ch0", NULL, "DVC 3"},
	{"FADE ch0", "Bypass", "IF ch0"},
	{"FADE ch0", "Fade Enable", "IF ch0"},
	{"SDM ch0", NULL, "FADE ch0"},
	{"PWM ch0", "Single_ended", "SDM ch0"},
	{"PWM ch0", "Differential", "SDM ch0"},
	{"SPK_OUT0", NULL, "PWM ch0"},

	{"DVC 4", NULL, "MIXER1"},
	{"IF ch1", NULL, "DVC 4"},
	{"FADE ch1", "Bypass", "IF ch1"},
	{"FADE ch1", "Fade Enable", "IF ch1"},
	{"SDM ch1", NULL, "FADE ch1"},
	{"PWM ch1", "Single_ended", "SDM ch1"},
	{"PWM ch1", "Differential", "SDM ch1"},
	{"SPK_OUT1", NULL, "PWM ch1"},
};

static const struct snd_soc_component_driver aic_codec_component = {
	.controls = aic_codec_controls,
	.num_controls = ARRAY_SIZE(aic_codec_controls),
	.dapm_widgets = aic_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aic_codec_dapm_widgets),
	.dapm_routes = aic_codec_dapm_route,
	.num_dapm_routes = ARRAY_SIZE(aic_codec_dapm_route),
	.idle_bias_on = 1,
	.use_pmdown_time = 1,
	.endianness = 1,
	.non_legacy_dai_naming = 1,
};

static int aic_codec_dai_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);
	struct aic_codec *codec = snd_soc_card_get_drvdata(card);

	if (!dai->id) {
		/* DMA configuration for DMIIC RX FIFO */
		codec->capture_dma_dmic.addr = codec->res->start + DMIC_RXFIFO_DATA_REG;
		codec->capture_dma_dmic.maxburst = 1;
		/* DMA configuration for TX FIFO */
		codec->playback_dma.addr = codec->res->start + TXFIFO_DATA_REG;
		codec->playback_dma.maxburst = 1;
	} else {
		/* DMA configuration for ADC RX FIFO */
		codec->capture_dma_adc.addr = codec->res->start + ADC_RXFIFO_DATA_REG;
		codec->capture_dma_adc.maxburst = 1;
		codec->capture_dma_adc.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	}

	if (!dai->id)
		snd_soc_dai_init_dma_data(dai, &codec->playback_dma,
					&codec->capture_dma_dmic);
	else
		snd_soc_dai_init_dma_data(dai, &codec->playback_dma,
					&codec->capture_dma_adc);
	return 0;
}

static struct snd_soc_dai_driver dummy_cpu_dai[] = {
	{
		.name = "aic-codec-cpu-dai",
		.probe = aic_codec_dai_probe,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "Capture-dmic",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	{
		.name = "aic-codec-cpu-dai-2",
		.probe = aic_codec_dai_probe,
		.capture = {
			.stream_name = "Capture-adc",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
#endif
};

static const struct snd_soc_component_driver aic_cpu_component = {
	.name = "aic-codec-comp",
};

static const struct snd_pcm_hardware aic_codec_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_INTERLEAVED,
	.buffer_bytes_max	= 128 * 1024,
	.period_bytes_max	= 64 * 1024,
	.period_bytes_min	= 256,
	.periods_max		= 255,
	.periods_min		= 2,
	.fifo_size		= 0,
};

static const struct snd_dmaengine_pcm_config aic_codec_dmaengine_pcm_config = {
	.pcm_hardware = &aic_codec_pcm_hardware,
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
	.prealloc_buffer_size = 128 * 1024,
};

static int aic_codec_spk_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event) && !IS_ERR_OR_NULL(gpiod_pa)) {
		msleep(20);
		gpiod_set_value(gpiod_pa, 1);
		msleep(100);
	} else if (SND_SOC_DAPM_EVENT_OFF(event) && !IS_ERR_OR_NULL(gpiod_pa)) {
		gpiod_set_value(gpiod_pa, 0);
	}

	return 0;
}

static const struct snd_soc_dapm_widget aic_codec_card_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", aic_codec_spk_event),
};

static const struct snd_soc_dapm_route aic_codec_card_dapm_routes[] = {
	{"Speaker", NULL, "SPK_OUT0"},
	{"Speaker", NULL, "SPK_OUT1"},
};

SND_SOC_DAILINK_DEFS(dmic_path,
	DAILINK_COMP_ARRAY(COMP_CPU("aic-codec-cpu-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("aic-codec-dev", "codec-dmic")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("aic-codec-dev")));

#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
SND_SOC_DAILINK_DEFS(adc_path,
	DAILINK_COMP_ARRAY(COMP_CPU("aic-codec-cpu-dai-2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("aic-codec-dev", "codec-adc")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("soc:codec-analog")));
#endif

static struct snd_soc_dai_link aic_codec_dai_link[] = {
	{
		.name = "dmic-path",
		.stream_name = "dmic-pcm",
		.dai_fmt = SND_SOC_DAIFMT_I2S,
		SND_SOC_DAILINK_REG(dmic_path),
		.ignore_pmdown_time = 1,
	},
#ifdef CONFIG_SND_SOC_AIC_CODEC_V1
	{
		.name = "adc-path",
		.stream_name = "adc-pcm",
		.dai_fmt = SND_SOC_DAIFMT_I2S,
		SND_SOC_DAILINK_REG(adc_path),
	},
#endif
};

static struct snd_soc_card aic_card = {
	.name = "aic-SoundCard",
	.owner = THIS_MODULE,
	.dapm_widgets = aic_codec_card_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aic_codec_card_dapm_widgets),
	.dapm_routes = aic_codec_card_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(aic_codec_card_dapm_routes),
	.dai_link = aic_codec_dai_link,
	.num_links = ARRAY_SIZE(aic_codec_dai_link),
};

static bool aic_codec_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TXFIFO_DATA_REG:
		return false;
	default:
		return true;
	}
}

static bool aic_codec_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case DMIC_RXFIFO_DATA_REG:
	case ADC_RXFIFO_DATA_REG:
	case CODEC_VERSION_REG:
		return false;
	default:
		return true;
	}
}

static bool aic_codec_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FIFO_STA_REG:
	case DMIC_RX_CNT_REG:
	case TX_CNT_REG:
	case TXFIFO_CTRL_REG:
	case GLOBE_CTL_REG:
	case ADC_RXFIFO_STA_REG:
	case ADC_CTL1_REG:
		return true;
	default:
		return false;
	}
}

static const struct reg_default aic_codec_reg_default[] = {
	{ RX_DMIC_IF_CTRL_REG, 0x00000000 },
	{ RX_HPF_1_2_CTRL_REG, 0x00000000 },
	{ RX_HPF1_COEFF_REG, 0x00FFAA45 },
	{ RX_HPF2_COEFF_REG, 0x00FFAA45 },
	{ RX_HPF1_GAIN_REG, 0x00FFD522 },
	{ RX_HPF2_GAIN_REG, 0x00FFD522 },
	{ RX_DVC_1_2_CTRL_REG, 0xA0A00000 },
	{ TX_MIXER_CTRL_REG, 0x00000000 },
	{ TX_DVC_3_4_CTRL_REG, 0xA0A00000 },
	{ TX_PLAYBACK_CTRL_REG, 0x00000300 },
	{ TX_SDM_CTRL_REG, 0x01101100 },
	{ TX_PWM_CTRL_REG, 0x00130700 },
	{ DMIC_RXFIFO_CTRL_REG, 0x00004000 },
	{ TXFIFO_CTRL_REG, 0x00004000 },
	{ FIFO_INT_EN_REG, 0x00000000 },
	{ FIFO_STA_REG, 0x01800000 },
	{ FADE_CTRL0_REG, 0x00804000 },
	{ FADE_CTRL1_REG, 0x7FFF7FFF },
	{ ADC_IF_CTRL_REG, 0x00000000 },
	{ ADC_HPF0_CTRL_REG, 0x00000000 },
	{ ADC_DVC0_CTRL_REG, 0x00A00000 },
	{ ADC_RXFIFO_CTRL_REG, 0x00004000 },
	{ ADC_RXFIFO_INT_EN_REG, 0x0 },
	{ CODEC_VERSION_REG, 0x00000100 },
};

static const struct regmap_config aic_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = CODEC_VERSION_REG,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = aic_codec_reg_default,
	.num_reg_defaults = ARRAY_SIZE(aic_codec_reg_default),
	.writeable_reg = aic_codec_wr_reg,
	.readable_reg = aic_codec_rd_reg,
	.volatile_reg = aic_codec_volatile_reg,
};

static int aic_codec_probe(struct platform_device *pdev)
{
	struct aic_codec *codec;
	void __iomem *base;
	int ret;
	struct device_node *np = pdev->dev.of_node;

	if (of_find_property(np, "ignore-pmdown-time", NULL) != NULL) {
		aic_codec_dai_link[0].ignore_pmdown_time = 0;
	}

	codec = devm_kzalloc(&pdev->dev, sizeof(*codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	codec->dev = &pdev->dev;

	codec->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, codec->res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to map the register\n");
		return PTR_ERR(base);
	}

	codec->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					&aic_codec_regmap_config);
	if (IS_ERR(codec->regmap)) {
		dev_err(&pdev->dev, "Failed to create regmap\n");
		return PTR_ERR(codec->regmap);
	}

	/* Get codec reset control */
	codec->rst = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(codec->rst)) {
		dev_err(&pdev->dev, "Failed to get reset control\n");
		return PTR_ERR(codec->rst);
	}

	ret = reset_control_deassert(codec->rst);
	if (ret) {
		dev_err(&pdev->dev, "Failed to deassert the reset control\n");
		return ret;
	}

	/* Get the clock of codec */
	codec->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(codec->clk)) {
		dev_err(&pdev->dev, "Failed to get codec clock\n");
		return PTR_ERR(codec->clk);
	}

	ret = clk_set_rate(codec->clk, DEFAULT_AUDIO_FREQ);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set audio clock rate\n");
		return ret;
	}

	ret = clk_prepare_enable(codec->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable codec clock");
		return -EINVAL;
	}

	/* Reset codec register */
	regmap_update_bits(codec->regmap, GLOBE_CTL_REG,
					GLOBE_GLB_RST, GLOBE_GLB_RST);

	codec->dev->init_name = "aic-codec-dev";

	ret = devm_snd_soc_register_component(&pdev->dev,
						&aic_codec_component,
						aic_codec_dai, ARRAY_SIZE(aic_codec_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register aic codec\n");
		return ret;
	}

	ret = devm_snd_soc_register_component(&pdev->dev,
						&aic_cpu_component,
						dummy_cpu_dai, ARRAY_SIZE(dummy_cpu_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register aic cpu dai\n");
		return ret;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev,
						&aic_codec_dmaengine_pcm_config, 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register dmaengine\n");
		return ret;
	}

	aic_card.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&aic_card, codec);

	ret = snd_soc_register_card(&aic_card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register sound card\n");

	gpiod_pa = devm_gpiod_get(&pdev->dev, "pa", GPIOD_OUT_LOW);
	if (IS_ERR(gpiod_pa))
		dev_warn(&pdev->dev, "Missing PA(Power Amplifier) gpio pin\n");

	return 0;
}

static int aic_codec_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id aic_codec_match[] = {
	{
		.compatible = "artinchip,aic-codec-v1.0",
	},
	{}
};

#ifdef CONFIG_PM
static int aic_codec_pm_suspend(struct device *dev)
{
	struct aic_codec *codec = snd_soc_card_get_drvdata(&aic_card);

	clk_disable_unprepare(codec->clk);
	return 0;
}

static int aic_codec_pm_resume(struct device *dev)
{
	struct aic_codec *codec = snd_soc_card_get_drvdata(&aic_card);

	clk_prepare_enable(codec->clk);
	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_codec_pm_ops, aic_codec_pm_suspend,
					aic_codec_pm_resume);

#define AIC_CODEC_DEV_PM_OPS		(&aic_codec_pm_ops)
#else
#define AIC_CODEC_DEV_PM_OPS		NULL
#endif

MODULE_DEVICE_TABLE(of, aic_codec_match);

static struct platform_driver aic_codec_driver = {
	.probe = aic_codec_probe,
	.remove = aic_codec_remove,
	.driver = {
		.name = "aic-codec",
		.of_match_table = aic_codec_match,
		.pm = AIC_CODEC_DEV_PM_OPS,
	},
};
module_platform_driver(aic_codec_driver);

MODULE_DESCRIPTION("ArtInChip CODEC Driver");
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_LICENSE("GPL");

