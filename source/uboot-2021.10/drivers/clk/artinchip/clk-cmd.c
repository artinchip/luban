// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2024 ArtInChip Inc.
 * Authors: ArtInChip
 */
#include <common.h>
#include <command.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dt-structs.h>
#include <errno.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include <linux/delay.h>
#include <div64.h>
#include "clk-aic.h"
#include "ctype.h"


#define CLK_NAME(name) { name, #name }

#if CONFIG_IS_ENABLED(CMD_CLK)
#ifdef CONFIG_CLK_ARTINCHIP_CMU_V1_0
#include <dt-bindings/clock/artinchip,aic-cmu.h>
struct aic_clks aic_clk_names[] = {
	/* Fixed rate clock */
	{ CLK_DUMMY,    "CLK_DUMMY" },
	{ CLK_OSC24M,   "CLK_OSC24M" },
	{ CLK_OSC32K,   "CLK_OSC32K" },
	{ CLK_RC1M,     "CLK_RC1M" },
	/* PLL clock */
	{ CLK_PLL_INT0, "CLK_PLL_INT0" },
	{ CLK_PLL_INT1, "CLK_PLL_INT1" },
	{ CLK_PLL_FRA0, "CLK_PLL_FRA0" },
	{ CLK_PLL_FRA1, "CLK_PLL_FRA1" },
	{ CLK_PLL_FRA2, "CLK_PLL_FRA2" },
	/* system clock */
	{ CLK_AXI0,     "CLK_AXI0" },
	{ CLK_AHB0,     "CLK_AHB0" },
	{ CLK_APB0,     "CLK_APB0" },
	{ CLK_APB1,     "CLK_APB1" },
	{ CLK_CPU,      "CLK_CPU" },
	/* Peripheral clock */
	{ CLK_DMA,      "CLK_DMA" },
	{ CLK_CE,       "CLK_CE" },
	{ CLK_USBD,     "CLK_USBD" },
	{ CLK_USBH0,    "CLK_USBH0" },
	{ CLK_USBH1,    "CLK_USBH1" },
	{ CLK_USB_PHY0, "CLK_USB_PHY0" },
	{ CLK_USB_PHY1, "CLK_USB_PHY1" },
	{ CLK_GMAC0,    "CLK_GMAC0" },
	{ CLK_GMAC1,    "CLK_GMAC1" },
	{ CLK_SPI0,     "CLK_SPI0" },
	{ CLK_SPI1,     "CLK_SPI1" },
	{ CLK_SPI2,     "CLK_SPI2" },
	{ CLK_SPI3,     "CLK_SPI3" },
	{ CLK_SDMC0,    "CLK_SDMC0" },
	{ CLK_SDMC1,    "CLK_SDMC1" },
	{ CLK_SDMC2,    "CLK_SDMC2" },
	{ CLK_SYSCFG,   "CLK_SYSCFG" },
	{ CLK_RTC,      "CLK_RTC" },
	{ CLK_SPIENC,   "CLK_SPIENC" },
	{ CLK_I2S0,     "CLK_I2S0" },
	{ CLK_I2S1,     "CLK_I2S1" },
	{ CLK_CODEC,    "CLK_CODEC" },
	{ CLK_RGB,      "CLK_RGB" },
	{ CLK_LVDS,     "CLK_LVDS" },
	{ CLK_MIPIDSI,  "CLK_MIPIDSI" },
	{ CLK_DE,       "CLK_DE" },
	{ CLK_GE,       "CLK_GE" },
	{ CLK_VE,       "CLK_VE" },
	{ CLK_WDOG,     "CLK_WDOG" },
	{ CLK_SID,      "CLK_SID" },
	{ CLK_GTC,      "CLK_GTC" },
	{ CLK_GPIO,     "CLK_GPIO" },
	{ CLK_UART0,    "CLK_UART0" },
	{ CLK_UART1,    "CLK_UART1" },
	{ CLK_UART2,    "CLK_UART2" },
	{ CLK_UART3,    "CLK_UART3" },
	{ CLK_UART4,    "CLK_UART4" },
	{ CLK_UART5,    "CLK_UART5" },
	{ CLK_UART6,    "CLK_UART6" },
	{ CLK_UART7,    "CLK_UART7" },
	{ CLK_I2C0,     "CLK_I2C0" },
	{ CLK_I2C1,     "CLK_I2C1" },
	{ CLK_I2C2,     "CLK_I2C2" },
	{ CLK_I2C3,     "CLK_I2C3" },
	{ CLK_CAN0,     "CLK_CAN0" },
	{ CLK_CAN1,     "CLK_CAN1" },
	{ CLK_PWM,      "CLK_PWM" },
	{ CLK_ADCIM,    "CLK_ADCIM" },
	{ CLK_GPAI,     "CLK_GPAI" },
	{ CLK_RTP,      "CLK_RTP" },
	{ CLK_TSEN,     "CLK_TSEN" },
	{ CLK_CIR,      "CLK_CIR" },
	{ CLK_DVP,      "CLK_DVP" },
	{ CLK_PBUS,     "CLK_PBUS" },
	{ CLK_MTOP,      "CLK_MTOP" },
	{ CLK_DM,       "CLK_DM" },
	{ CLK_PWMCS,    "CLK_PWMCS" },
	{ CLK_PSADC,    "CLK_PSADC" },
	{ CLK_DDR,      "CLK_DDR" },
	/* Display clock */
	{ CLK_PIX,      "CLK_PIX" },
	{ CLK_SCLK,     "CLK_SCLK" },
	/* Output clock */
	{ CLK_OUT0,     "CLK_OUT0" },
	{ CLK_OUT1,     "CLK_OUT1" },
	{ CLK_OUT2,     "CLK_OUT2" },
	{ CLK_OUT3,     "CLK_OUT3" },
};
#endif /* CLK_ARTINCHIP_V1_0 */

#ifdef CONFIG_CLK_ARTINCHIP_CMU_V2_0
#include <dt-bindings/clock/artinchip,aic-cmu-v20.h>
struct aic_clks aic_clk_names[] = {
	/* Fixed rate clock */
	CLK_NAME(CLK_24M),
	CLK_NAME(CLK_32K),
	/* PLL clock */
	CLK_NAME(CLK_PLL_FRA0),
	CLK_NAME(CLK_PLL_FRA1),
	CLK_NAME(CLK_PLL_FRA2),
	CLK_NAME(CLK_PLL_FRA3),
	CLK_NAME(CLK_PLL_FRA4),
	CLK_NAME(CLK_PLL_FRA5),
	CLK_NAME(CLK_PLL_FRA6),
	CLK_NAME(CLK_PLL_FRA7),
	/* system clock */
	CLK_NAME(CLK_CORE_CPU),
	CLK_NAME(CLK_APB0),
	CLK_NAME(CLK_APB2),
	/* Peripheral clock */
	CLK_NAME(CLK_SYSCFG),
	CLK_NAME(CLK_DMA0),
	CLK_NAME(CLK_CORE_WDOG),
	CLK_NAME(CLK_USBH0),
	CLK_NAME(CLK_USBH1),
	CLK_NAME(CLK_USB_PHY0),
	CLK_NAME(CLK_USB_PHY1),
	CLK_NAME(CLK_SPI0),
	CLK_NAME(CLK_SPI1),
	CLK_NAME(CLK_SPI2),
	CLK_NAME(CLK_SPI3),
	CLK_NAME(CLK_SPI4),
	CLK_NAME(CLK_SDMC0),
	CLK_NAME(CLK_SDMC1),
	CLK_NAME(CLK_I2S0),
	CLK_NAME(CLK_AUDIO),
	CLK_NAME(CLK_UART0),
	CLK_NAME(CLK_UART1),
	CLK_NAME(CLK_UART2),
	CLK_NAME(CLK_UART3),
	CLK_NAME(CLK_UART4),
	CLK_NAME(CLK_UART5),
	CLK_NAME(CLK_LCD),
	CLK_NAME(CLK_LVDS),
	CLK_NAME(CLK_DE),
	CLK_NAME(CLK_SID),
	CLK_NAME(CLK_I2C0),
	CLK_NAME(CLK_I2C1),
	CLK_NAME(CLK_I2C2),
	CLK_NAME(CLK_I2C3),
	CLK_NAME(CLK_I2C4),
	CLK_NAME(CLK_ADCIM),
	CLK_NAME(CLK_GPAI),
	CLK_NAME(CLK_THS),
	/* Display clock */
	CLK_NAME(CLK_PIX),
	CLK_NAME(CLK_SCLK),
	/* Output clock */
	CLK_NAME(CLK_OUT0),
	CLK_NAME(CLK_OUT1),
	CLK_NAME(CLK_OUT2),
	CLK_NAME(CLK_OUT3),
};
#endif	/*CLK_ARTINCHIP_V2_0*/

#ifdef CONFIG_CLK_ARTINCHIP_CMU_V3_0
#include <dt-bindings/clock/artinchip,aic-cmu-v30.h>
struct aic_clks aic_clk_names[] = {
	/* Fixed rate clock */
	CLK_NAME(CLK_24M),
	CLK_NAME(CLK_32K),
	/* PLL clock */
	CLK_NAME(CLK_PLL_FRA0),
	CLK_NAME(CLK_PLL_FRA1),
	CLK_NAME(CLK_PLL_FRA2),
	CLK_NAME(CLK_PLL_INT0),
	CLK_NAME(CLK_PLL_INT1),
	/* system clock */
	CLK_NAME(CLK_CPU0),
	CLK_NAME(CLK_CPU1),
	CLK_NAME(CLK_AHB),
	CLK_NAME(CLK_AXI),
	CLK_NAME(CLK_APB),
	/* Peripheral clock */
	CLK_NAME(CLK_SYSCFG),
	CLK_NAME(CLK_DMA0),
	CLK_NAME(CLK_DMA1),
	CLK_NAME(CLK_WDOG),
	CLK_NAME(CLK_USBH0),
	CLK_NAME(CLK_USBH1),
	CLK_NAME(CLK_USB_PHY0),
	CLK_NAME(CLK_USB_PHY1),
	CLK_NAME(CLK_QSPI0),
	CLK_NAME(CLK_QSPI1),
	CLK_NAME(CLK_QSPI2),
	CLK_NAME(CLK_QSPI3),
	CLK_NAME(CLK_SDMC0),
	CLK_NAME(CLK_SDMC1),
	CLK_NAME(CLK_SDMC2),
	CLK_NAME(CLK_SPIENC),
	CLK_NAME(CLK_I2S0),
	CLK_NAME(CLK_I2S1),
	CLK_NAME(CLK_I2S2),
	CLK_NAME(CLK_AUDIO),
	CLK_NAME(CLK_UART0),
	CLK_NAME(CLK_UART1),
	CLK_NAME(CLK_UART2),
	CLK_NAME(CLK_UART3),
	CLK_NAME(CLK_UART4),
	CLK_NAME(CLK_UART5),
	CLK_NAME(CLK_UART6),
	CLK_NAME(CLK_UART7),
	CLK_NAME(CLK_LCD0),
	CLK_NAME(CLK_LVDS0),
	CLK_NAME(CLK_LCD1),
	CLK_NAME(CLK_LVDS1),
	CLK_NAME(CLK_DE),
	CLK_NAME(CLK_SID),
	CLK_NAME(CLK_I2C0),
	CLK_NAME(CLK_I2C1),
	CLK_NAME(CLK_I2C2),
	CLK_NAME(CLK_ADCIM),
	CLK_NAME(CLK_GPAI),
	CLK_NAME(CLK_THS),
	/* Display clock */
	CLK_NAME(CLK_PIX0),
	CLK_NAME(CLK_SCLK0),
	CLK_NAME(CLK_PIX1),
	CLK_NAME(CLK_SCLK1),
	/* Output clock */
	CLK_NAME(CLK_OUT0),
	CLK_NAME(CLK_OUT1),
	CLK_NAME(CLK_OUT2),
	CLK_NAME(CLK_OUT3),
};
#endif	/*CLK_ARTINCHIP_V3_0*/

#define GET_NUM 4

int soc_clk_dump(void)
{
	struct udevice *dev;
	struct clk clk;
	ulong rate;
	int i, ret;
	struct aic_clk_priv *priv;

	ret = uclass_get_device_by_driver(UCLASS_CLK,
				DM_DRIVER_GET(artinchip_cmu), &dev);
	if (ret)
		return ret;

	priv = dev_get_priv(dev);

	printf("------------------------------------\n");
	printf("Clk-ID\t|      NAME      |    Hz\n");
	for (i = 0; i < sizeof(aic_clk_names) / sizeof(aic_clk_names[0]); i++) {
		clk.id = aic_clk_names[i].id;
		clk.dev = dev;

		rate = clk_get_rate(&clk);
		printf("  %lu\t  %-14s   %lu\n", clk.id, aic_clk_names[i].name, rate);
	}
	printf("\n------------------------------------\n");
	return 0;
}

static int aic_clk_get(ulong *ch)
{
	int m;
	for (m = 0; m < sizeof(aic_clk_names) / sizeof(aic_clk_names[0]); m++) {
		if (*ch == aic_clk_names[m].id) {
			*ch = m;
			return m;
		}

	}
	return -1;
}


static int aic_clk_dump_mux(u32 clk_start, u32 clk_cnt)
{
	struct udevice *dev;
	struct clk clk;
	ulong index = clk_start;
	ulong rate;
	int i = 0;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_CLK,
				DM_DRIVER_GET(artinchip_cmu), &dev);
	if (ret)
		return ret;

	printf("------------------------------------\n");
	printf("Clk-ID\t|      NAME      |    Hz\n");

	aic_clk_get(&index);

	for (i = 0; i < clk_cnt; i++) {
		clk.id = aic_clk_names[index + i].id;
		clk.dev = dev;
		rate = clk_get_rate(&clk);
		printf("  %lu\t  %-14s   %lu\n", aic_clk_names[index + i].id,
			aic_clk_names[index + i].name, rate);
	}

	printf("\n------------------------------------\n");
	printf("0: Fixed rate clock     1: PLL clock         2: System clock\n");
	printf("3: Peripheral clock     4: Display clock     5: Output clock\n");
	printf("none: all clocks\n");
	return 0;
}

static int do_aic_clk_dump(struct cmd_tbl *cmdtp, int flag, int argc,
		char *const argv[])
{
	struct udevice *dev;
	struct aic_clk_priv *priv;
	struct aic_clk_tree *tree;
	int ret;
	ulong ch;

	if (argc < 2) {
		ret = aic_clk_dump_mux(0, sizeof(aic_clk_names) / sizeof(aic_clk_names[0]));
		return 0;
	}

	ret = uclass_get_device_by_driver(UCLASS_CLK,
			DM_DRIVER_GET(artinchip_cmu), &dev);
	if (ret)
		return ret;

	priv = dev_get_priv(dev);
	tree = priv->tree;

	printf("------------------------------------\n");
	ch = dectoul(argv[1], NULL);
	switch (ch) {
	case AIC_CLK_FIXED_RATE:
		printf("\tFixed rate clock info\n");
		aic_clk_dump_mux(tree->fixed_rate_base, tree->fixed_rate_cnt);
		break;
	case AIC_CLK_PLL:
		printf("\tPLL clock info\n");
		aic_clk_dump_mux(tree->pll_base, tree->pll_cnt);
		break;
	case AIC_CLK_SYSTEM:
		printf("\tSystem clock info\n");
		aic_clk_dump_mux(tree->system_base, tree->system_cnt);
		break;
	case AIC_CLK_PERIPHERAL:
		printf("\tPeripheral clock\n");
		aic_clk_dump_mux(tree->periph_base, tree->periph_cnt);
		break;
	case AIC_CLK_DISP:
		printf("\tDisplay clock info\n");
		aic_clk_dump_mux(tree->disp_base, tree->disp_cnt);
		break;
	case AIC_CLK_OUTPUT:
		printf("\tOutput clock info\n");
		aic_clk_dump_mux(tree->clkout_base, tree->clkout_cnt);
		break;
	default:
		ret = aic_clk_dump_mux(0, sizeof(aic_clk_names) / sizeof(aic_clk_names[0]));
		break;
	}

	if (ret < 0) {
		printf("Clock dump error %d\n", ret);
		ret = CMD_RET_FAILURE;
	}

	return 0;
}

static int aic_clk_get_byid(struct udevice *dev, ulong id)
{
	struct clk clk;
	ulong ch = id;
	ulong rate;
	int ret;

	if (ch >= AIC_CLK_END)
		return -1;

	ret = aic_clk_get(&ch);
	if (ret < 0) {
		printf("u-boot does not support controlling "
			"clock with id: %ld !\n", id);
		return -1;
	}

	clk.id = aic_clk_names[ch].id;
	clk.dev = dev;
	rate = clk_get_rate(&clk);
	printf("  %lu\t  %-14s   %lu\n", aic_clk_names[ch].id,
		aic_clk_names[ch].name, rate);

	return 0;
}


static int aic_clk_get_byname(struct udevice *dev, char *name)
{
	struct aic_clks aic_clks;
	struct clk clk;
	ulong rate;
	int i, ret;

	for (i = 0; i < sizeof(aic_clk_names) / sizeof(aic_clk_names[0]); i++) {
		aic_clks.name = aic_clk_names[i].name;
		aic_clks.name += 4;
		ret = strncmp(aic_clks.name, name, strlen(name));
		if (ret == 0) {
			clk.id = aic_clk_names[i].id;
			clk.dev = dev;
			rate = clk_get_rate(&clk);
			printf("  %lu\t  %-14s   %lu\n", aic_clk_names[i].id,
				aic_clk_names[i].name, rate);
		}
	}
	return 0;
}

static int do_aic_clk_get(struct cmd_tbl *cmdtp, int falg, int argc,
		char *const argv[])
{
	struct udevice *dev;
	int ret, j;
	ulong ch;

	if (argc < 2) {
		printf("please input [module name] or [clk id]. such:SPI SPI0 ...\n");
		return CMD_RET_USAGE;
	} else if (argc > (GET_NUM + 1)) {
		printf("You can only enter %d indexes at most!\n", GET_NUM);
		return CMD_RET_USAGE;
	}
	ret = uclass_get_device_by_driver(UCLASS_CLK,
			DM_DRIVER_GET(artinchip_cmu), &dev);
	if (ret)
		return ret;

	printf("------------------------------------\n");
	printf("Clk-ID\t|      NAME      |    Hz\n");

	for (j = 1; j < argc; j++) {
		if (isalpha(argv[j][0]) == 0) {
			ch = dectoul(argv[j], NULL);
			ret = aic_clk_get_byid(dev, ch);
			if (ret < 0)
				break;
		} else {
			aic_clk_get_byname(dev, argv[j]);
		}
	}

	printf("\n------------------------------------\n");
	return 0;
}

static int do_aic_clk_set(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct udevice *dev;
	struct clk clk;
	int ret;
	ulong freq, pre_freq, index;

	if (argc < 3)
		return CMD_RET_USAGE;

	index = dectoul(argv[1], NULL);
	freq = dectoul(argv[2], NULL);

	ret = uclass_get_device_by_driver(UCLASS_CLK,
			DM_DRIVER_GET(artinchip_cmu), &dev);
	if (ret)
		return ret;

	printf("------------------------------------\n");
	clk.id = index;
	clk.dev = dev;
	aic_clk_get(&index);
	pre_freq = clk_get_rate(&clk);
	ret = clk_set_rate(&clk, freq);
	if (ret == 0)
		printf("Clock frequency is set successsfully!\n");
	printf("Clk-ID\t|      NAME      |    Hz\n");
	printf("  %lu\t  %-14s   %lu  ---->  %lu\n", aic_clk_names[index].id,
			aic_clk_names[index].name, pre_freq, freq);
	freq = clk_get_rate(&clk);
	printf("-\n");
	printf("  %lu\t  %-14s   %lu\n", aic_clk_names[index].id,
			aic_clk_names[index].name, freq);
	printf("\n------------------------------------\n");

	return 0;
}

static char clk_help_text[] =
	"ArtInChip clk  command\n"
	"------------------------------------\n"
	"aic_clk dump      [type]  -all clock info	\n"
	"                  [type]  0: Fixed rate clock  1: PLL cloc        2: System clock\n"
	"                          3: Peripheral clock  4: Display clock   5: Output clock\n"
	"                          none: all clocks\n"
	"aic_clk get       [id] or [name]  -get clk info  [name]: SPI, SPI0...\n"
	"aic_clk set       [id] [Hz]       -set clock frequency\n";

U_BOOT_CMD_WITH_SUBCMDS(aic_clk, "ArtInChip clk command", clk_help_text,
			U_BOOT_SUBCMD_MKENT(dump, 2, 0, do_aic_clk_dump),
			U_BOOT_SUBCMD_MKENT(get, GET_NUM + 2, 1, do_aic_clk_get),
			U_BOOT_SUBCMD_MKENT(set, 3, 0, do_aic_clk_set));
#endif
