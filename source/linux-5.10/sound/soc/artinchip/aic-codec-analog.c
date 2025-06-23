// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 ArtInChip Inc.
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

static int aic_codec_analog_probe(struct platform_device *pdev)
{
	int ret;

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register analog dmaengine\n");
		return ret;
	}
	dev_info(&pdev->dev, "AIC-CODEC analog dmaengine registered\n");
	return 0;
}

static int aic_codec_analog_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id aic_codec_analog_match[] = {
	{
		.compatible = "artinchip,codec-analog",
	},
	{}
};
MODULE_DEVICE_TABLE(of, aic_codec_analog_match);

static struct platform_driver aic_codec_analog_driver = {
	.probe = aic_codec_analog_probe,
	.remove = aic_codec_analog_remove,
	.driver = {
		.name = "aic-codec-analog",
		.of_match_table = aic_codec_analog_match,
	}
};
module_platform_driver(aic_codec_analog_driver);

MODULE_DESCRIPTION("ArtInChip CODEC Analog Driver");
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_LICENSE("GPL");

