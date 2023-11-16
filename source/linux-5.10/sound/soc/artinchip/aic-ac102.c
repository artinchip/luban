// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../codecs/ac102.h"

#define MCLK_FREQ	12288000

static int aic_ac102_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, MCLK_FREQ, 0);
	if (ret) {
		dev_err(cpu_dai->dev, "Failed to set MCLK frequency\n");
		return ret;
	}
	return 0;
}

static const struct snd_soc_ops aic_ac102_snd_ops = {
	.hw_params = aic_ac102_hw_params,
};

SND_SOC_DAILINK_DEFS(aic_ac102,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "ac102-dai")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link aic_ac102_dai = {
	.name = "aic_ac102",
	.stream_name = "aic_ac102 PCM",
	.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_CBS_CFS |
			   SND_SOC_DAIFMT_NB_NF,
	.ops = &aic_ac102_snd_ops,
	SND_SOC_DAILINK_REG(aic_ac102),
};

static struct snd_soc_card aic_ac102 = {
	.name = "aic_ac102",
	.owner = THIS_MODULE,
	.dai_link = &aic_ac102_dai,
	.num_links = 1,
};

static int aic_ac102_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;

	aic_ac102.dev = &pdev->dev;

	aic_ac102_dai.codecs->of_node = of_parse_phandle(np, "aic,codec-chip", 0);
	if (!aic_ac102_dai.codecs->of_node) {
		dev_err(&pdev->dev,
			"Property 'aic,codec-chip' missing or invalid\n");
		return -EINVAL;
	}

	aic_ac102_dai.cpus->of_node = of_parse_phandle(np, "aic,i2s-controller", 0);
	if (!aic_ac102_dai.cpus->of_node) {
		dev_err(&pdev->dev,
			"Property 'aic,i2s-controller' missing or invalid\n");
		ret = -EINVAL;
		goto put_codec_of_node;
	}

	aic_ac102_dai.platforms->of_node = aic_ac102_dai.cpus->of_node;

	ret = snd_soc_register_card(&aic_ac102);
	if (ret) {
		dev_err(&pdev->dev, "Sound card register failed %d\n", ret);
		goto put_cpu_of_node;
	}

	return ret;

put_cpu_of_node:
	of_node_put(aic_ac102_dai.cpus->of_node);
	aic_ac102_dai.cpus->of_node = NULL;
put_codec_of_node:
	of_node_put(aic_ac102_dai.codecs->of_node);
	aic_ac102_dai.codecs->of_node = NULL;

	return ret;
}

static int aic_ac102_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&aic_ac102);
	return 0;
}

static const struct of_device_id aic_ac102_dt_ids[] = {
	{.compatible = "artinchip,aic-ac102",},
	{/* sentinel */}
};

static struct platform_driver aic_ac102_driver = {
	.driver = {
		.name = "aic_ac102-audio",
		.of_match_table = aic_ac102_dt_ids,
	},
	.probe = aic_ac102_probe,
	.remove = aic_ac102_remove,
};

module_platform_driver(aic_ac102_driver);

MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_DESCRIPTION("ALSA SoC driver for aic");
MODULE_LICENSE("GPL");

