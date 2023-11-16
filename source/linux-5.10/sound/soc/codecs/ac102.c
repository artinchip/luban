// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <linux/of.h>
#include <sound/tlv.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include "ac102.h"

static const struct reg_default ac102_reg_default[] = {
	{ 0x00, 0x12 },
	{ 0x01, 0x73 },
	{ 0x02, 0x1b },
	{ 0x03, 0x34 },
	{ 0x04, 0x01 },
	{ 0x05, 0x01 },
	{ 0x06, 0x3f },
	{ 0x07, 0x17 },
	{ 0x08, 0x05 },
	{ 0x09, 0x00 },
	{ 0x0a, 0x0f },
	{ 0x0b, 0x15 },
	{ 0x0c, 0x33 },
	{ 0x0d, 0x00 },
	{ 0x0e, 0x05 },
	{ 0x0f, 0x03 },
	{ 0x11, 0x0a },
	{ 0x13, 0x05 },
	{ 0x16, 0xe4 },
	{ 0x18, 0x0f },
	{ 0x19, 0x0a },
	{ 0x1a, 0x81 },
	{ 0x1b, 0x60 },
	{ 0x1c, 0x81 },
	{ 0x1d, 0x01 },
	{ 0x1f, 0x15 },
	{ 0x20, 0x60 },
	{ 0x25, 0x00 },
	{ 0x26, 0x00 },
	{ 0x27, 0xb6 },
	{ 0x28, 0x33 },
	{ 0x30, 0x00 },
	{ 0x31, 0x01 },
	{ 0x32, 0xc5 },
	{ 0x33, 0x32 },
	{ 0x34, 0x28 },
	{ 0x35, 0x00 },
	{ 0x36, 0x05 },
	{ 0x37, 0x1e },
	{ 0x38, 0xb8 },
	{ 0x39, 0x00 },
	{ 0x3a, 0x5f },
	{ 0x3b, 0x00 },
	{ 0x3c, 0x1f },
	{ 0x3d, 0x14 },
	{ 0x3e, 0x00 },
	{ 0x3f, 0x05 },
	{ 0x40, 0x1e },
	{ 0x41, 0xb8 },
	{ 0x42, 0x00 },
	{ 0x43, 0xff },
	{ 0x44, 0xfa },
	{ 0x45, 0xc1 },
	{ 0x46, 0x09 },
	{ 0x4f, 0x00 },
	{ 0x50, 0x01 },
	{ 0x51, 0x00 },
	{ 0x52, 0x00 },
	{ 0x53, 0x00 },
	{ 0x54, 0x00 },
	{ 0x55, 0x00 },
	{ 0x56, 0x00 },
	{ 0x57, 0x00 },
	{ 0x58, 0x00 },
	{ 0x59, 0x00 },
	{ 0x5a, 0x00 },
	{ 0x5b, 0x00 },
	{ 0x5c, 0x00 },
	{ 0x5d, 0x00 },
	{ 0x5e, 0x00 },
	{ 0x60, 0x01 },
	{ 0x61, 0x00 },
	{ 0x62, 0x00 },
	{ 0x63, 0x00 },
	{ 0x64, 0x00 },
	{ 0x65, 0x00 },
	{ 0x66, 0x00 },
	{ 0x67, 0x00 },
	{ 0x68, 0x00 },
	{ 0x69, 0x00 },
	{ 0x6a, 0x00 },
	{ 0x6b, 0x00 },
	{ 0x6c, 0x00 },
	{ 0x6d, 0x00 },
	{ 0x6e, 0x00 },
	{ 0x70, 0x01 },
	{ 0x71, 0x00 },
	{ 0x72, 0x00 },
	{ 0x73, 0x00 },
	{ 0x74, 0x00 },
	{ 0x75, 0x00 },
	{ 0x76, 0x00 },
	{ 0x77, 0x00 },
	{ 0x78, 0x00 },
	{ 0x79, 0x00 },
	{ 0x7a, 0x00 },
	{ 0x7b, 0x00 },
	{ 0x7c, 0x00 },
	{ 0x7d, 0x00 },
	{ 0x7e, 0x00 },
};

struct ac102_adc_dac_clk_div {
	u8 div;
	u8 val;
};

static const struct ac102_adc_dac_clk_div ac102_adcdac_clkdiv[] = {
	{.val = 0, .div = 1},
	{.val = 1, .div = 2},
	{.val = 2, .div = 3},
	{.val = 3, .div = 4},
	{.val = 4, .div = 6},
	{.val = 5, .div = 8},
	{.val = 6, .div = 12},
	{.val = 7, .div = 16},
	{.val = 8, .div = 24},
};

struct ac102_priv {
	struct regmap *regmap;
	unsigned int mclk_freq;
	unsigned int f_128fs;
	unsigned int format;
	unsigned int slots;
	unsigned int slot_width;
};

static DECLARE_TLV_DB_SCALE(adc_dvc_scale, -6400, 50, 1);
static DECLARE_TLV_DB_SCALE(dac_dvc_scale, -6400, 50, 1);
static DECLARE_TLV_DB_SCALE(pga_gain_scale, 0, 100, 0);
static DECLARE_TLV_DB_SCALE(line_out_amp_gain, 0, 300, 0);

static const struct snd_kcontrol_new ac102_codec_controls[] = {
	SOC_SINGLE_TLV("ADC Digital Volume", ADC_DVC, 0, 0xff, 0,
					adc_dvc_scale),
	SOC_SINGLE_TLV("DAC Digital Volume", DAC_DVC, 0, 0xff, 0,
					dac_dvc_scale),
	SOC_SINGLE_TLV("PGA gain", ADC_ANA_CTRL1, 3, 0x1f, 0,
					pga_gain_scale),
	SOC_SINGLE_TLV("Line out amplifier gain", DAC_ANA_CTRL2,
					0, 0xf, 0, line_out_amp_gain),
};

static const struct snd_soc_dapm_widget	ac102_dapm_widgets[] = {
	/* Digital parts of the DAC */
	SND_SOC_DAPM_SUPPLY("DAC", DAC_DIG_CTRL, 0, 0, NULL, 0),
	/* Analog parts of the DAC */
	SND_SOC_DAPM_DAC("DAC_OUT", "Playback", SYS_FUNC_CTRL, 0, 0),
	/* Analog parts of the ADC */
	SND_SOC_DAPM_ADC("ADC_PGA", "Capture", ADC_ANA_CTRL1, 0, 0),

	/* Input/Output widget */
	SND_SOC_DAPM_INPUT("MICP"),
	SND_SOC_DAPM_INPUT("MICN"),
	SND_SOC_DAPM_OUTPUT("LOUTP"),
	SND_SOC_DAPM_OUTPUT("LOUTN"),
};

static const struct snd_soc_dapm_route ac102_dapm_routes[] = {
	{"ADC_PGA", NULL, "MICP"},
	{"ADC_PGA", NULL, "MICN"},

	{"DAC_OUT", NULL, "DAC"},
	{"LOUTP", NULL, "DAC_OUT"},
	{"LOUTN", NULL, "DAC_OUT"},
};

static int ac102_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct ac102_priv *ac102 = snd_soc_component_get_drvdata(component);

	u8 mode = snd_soc_component_read(component, I2S_FMT_CTRL1) & 0x3;
	/* Set Transfer Mode selection */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAI_FORMAT_DSP_A:
		mode |= 0x4;
		break;
	case SND_SOC_DAI_FORMAT_DSP_B:
		break;
	case SND_SOC_DAI_FORMAT_I2S:
		mode |= 0x14;
		break;
	case SND_SOC_DAI_FORMAT_LEFT_J:
		mode |= 0x10;
		break;
	case SND_SOC_DAI_FORMAT_RIGHT_J:
		mode |= 0x20;
		break;
	default:
		dev_err(component->dev, "Unsupported transfer mode of AC102\n");
		return -EINVAL;
	}
	snd_soc_component_write(component, I2S_FMT_CTRL1, mode);
	ac102->format = fmt;

	return 0;
}

static int ac102_get_sr_sw_div(unsigned int width)
{
	return (width - 8) / 4 + 1;
}

static int ac102_get_adcdac_clkval(unsigned char clkdiv)
{
	int i;
	const struct ac102_adc_dac_clk_div *dividers = &ac102_adcdac_clkdiv[0];

	for (i = 0; i < ARRAY_SIZE(ac102_adcdac_clkdiv); i++) {
		const struct ac102_adc_dac_clk_div *clkdivp = &dividers[i];

		if (clkdivp->div == clkdiv)
			return clkdivp->val;
	}
	return -EINVAL;
}

static int ac102_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *codec_dai)
{
	struct snd_soc_component *component = codec_dai->component;
	struct ac102_priv *ac102 = snd_soc_component_get_drvdata(component);
	unsigned int word_size = params_width(params);
	unsigned int slot_width = params_physical_width(params);
	unsigned int slots = params_channels(params);
	unsigned int sample_rate = params_rate(params);
	u8 adc_dac_clkdiv = 12288000 / 128 / sample_rate;
	u8 adc_dac_clkval = 0;
	unsigned int lrck_period = 0;
	u8 sr, sw;
	u8 test_data;
	u8 i;

	adc_dac_clkval = ac102_get_adcdac_clkval(adc_dac_clkdiv);
	/* Set ADC or DAC clock divider */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_component_write(component, DAC_CLK_SET, adc_dac_clkval);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_component_write(component, ADC_CLK_SET, adc_dac_clkval);

	/* Set LRCK Period */
	switch (ac102->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAI_FORMAT_I2S:
	case SND_SOC_DAI_FORMAT_LEFT_J:
	case SND_SOC_DAI_FORMAT_RIGHT_J:
		lrck_period = slots * slot_width / 2 - 1;
		break;
	case SND_SOC_DAI_FORMAT_DSP_A:
	case SND_SOC_DAI_FORMAT_DSP_B:
	default:	/* I2S default Mode is PCM mode */
		lrck_period = slots * slot_width - 1;
		break;
	}
	snd_soc_component_write(component, I2S_LRCK_CTRL2, lrck_period);

	/* Set sample resolution and slot width */
	sr = ac102_get_sr_sw_div(word_size);
	sw = ac102_get_sr_sw_div(slot_width);
	sr |= (sw << 4);
	snd_soc_component_write(component, I2S_FMT_CTRL2, sr);

	/****************Test MicBias enable or not************************/
	test_data = snd_soc_component_read(component, PWR_CTRL2);
	for (i = 0; i <= 0x31; i++) {
		test_data = snd_soc_component_read(component, i);
		dev_dbg(component->dev, "0x%02x: 0x%02x\n", i, test_data);
	}

	return 0;
}

static const struct snd_soc_dai_ops ac102_dai_ops = {
	.set_fmt = ac102_set_fmt,
	.hw_params = ac102_hw_params,
};

static struct snd_soc_dai_driver ac102_dai = {
	.name = "ac102-dai",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &ac102_dai_ops,
};

static const struct snd_soc_component_driver soc_component_dev_ac102 = {
	.controls = ac102_codec_controls,
	.num_controls = ARRAY_SIZE(ac102_codec_controls),
	.dapm_widgets = ac102_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ac102_dapm_widgets),
	.dapm_routes = ac102_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(ac102_dapm_routes),
	.non_legacy_dai_naming = 1,
	.idle_bias_on = 1,
	.use_pmdown_time = 1,
	.endianness = 1,
};

static const struct regmap_config ac102_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = EQ3_A2_L,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = ac102_reg_default,
	.num_reg_defaults = ARRAY_SIZE(ac102_reg_default),
};

static int ac102_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct ac102_priv *ac102;
	int ret;
	unsigned int test_data = 0;

	ac102 = devm_kzalloc(&i2c->dev, sizeof(struct ac102_priv), GFP_KERNEL);
	if (!ac102)
		return -ENOMEM;

	ac102->regmap = devm_regmap_init_i2c(i2c, &ac102_regmap_config);
	if (IS_ERR(ac102->regmap)) {
		ret = PTR_ERR(ac102->regmap);
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, ac102);

	/* Disable DAC analog part */
	ret = regmap_write(ac102->regmap, SYS_FUNC_CTRL, 0x34);
	if (ret != 0) {
		dev_err(&i2c->dev,
			"Failed to disable DAC analog part: %d\n", ret);
		return ret;
	}

	/* Reset the codec */
	ret = regmap_write(ac102->regmap, CHIP_SOFT_RST, 0x34);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to reset codec ac102: %d\n", ret);
		return ret;
	}

	/* turn down volume */
	ret = regmap_write(ac102->regmap, DAC_ANA_CTRL2, 5);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to reset codec ac102: %d\n", ret);
		return ret;
	}

	/* Enable MICBIAS */
	ret = regmap_write(ac102->regmap, PWR_CTRL2, 0x1F);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to enable ac102 MICBIAS: %d\n", ret);
		return ret;
	}
	regmap_read(ac102->regmap, PWR_CTRL2, &test_data);

	/* Enable HPF */
	ret = regmap_write(ac102->regmap, AGC_CTRL, 0x09);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to enable ac102 HPF: %d\n", ret);
		return ret;
	}
	regmap_read(ac102->regmap, AGC_CTRL, &test_data);

	/* Enable ADC */
	ret = regmap_write(ac102->regmap, ADC_DIG_CTRL, 0x01);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to enable ac102 ADC: %d\n", ret);
		return ret;
	}

	ret = devm_snd_soc_register_component(&i2c->dev,
						&soc_component_dev_ac102,
						&ac102_dai, 1);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		return ret;
	}
	return 0;
}

static int ac102_i2c_detect(struct i2c_client *client,
						struct i2c_board_info *info)
{
	strlcpy(info->type, "ac102", I2C_NAME_SIZE);
	return 0;
}

static const unsigned short ac102_i2c_addr = 0x33;

static const struct i2c_board_info ac102_i2c_board_info[] = {
	{I2C_BOARD_INFO("ac102", 0x33),},
};

static const struct i2c_device_id ac102_i2c_id[] = {
	{"ac102", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ac102_i2c_id);

static const struct of_device_id ac102_of_match[] = {
	{.compatible = "allwinner,ac102"},
	{}
};

MODULE_DEVICE_TABLE(of, ac102_of_match);

static struct i2c_driver ac102_i2c_driver = {
	.driver = {
		.name = "ac102",
		.of_match_table = ac102_of_match,
	},
	.probe = ac102_i2c_probe,
	.id_table = ac102_i2c_id,
	.address_list = &ac102_i2c_addr,
	.detect = ac102_i2c_detect,
};

module_i2c_driver(ac102_i2c_driver);
MODULE_DESCRIPTION("Asoc AC102 codec driver");
MODULE_AUTHOR("dwj weijie.ding@artinchip.com");
MODULE_LICENSE("GPL");
