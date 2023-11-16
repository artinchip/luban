/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _AC102_H_
#define _AC102_H_

#define CHIP_SOFT_RST			0x00
#define PWR_CTRL1			0x01
#define PWR_CTRL2			0x02
#define SYS_FUNC_CTRL			0x03
#define ADC_CLK_SET			0x04
#define DAC_CLK_SET			0x05
#define SYS_CLK_ENA			0x06
#define I2S_CTRL			0x07
#define I2S_BCLK_CTRL			0x08
#define I2S_LRCK_CTRL1			0x09
#define I2S_LRCK_CTRL2			0x0A
#define I2S_FMT_CTRL1			0x0B
#define I2S_FMT_CTRL2			0x0C
#define I2S_FMT_CTRL3			0x0D
#define I2S_SLOT_CTRL			0x0E
#define I2S_TX_CTRL			0x0F

#define I2S_TXCHMP_CTRL			0x11
#define I2S_TX_MIX_SRC			0x13
#define I2S_RXCHMP_CTRL			0x16
#define I2S_RX_MIX_SRC			0x18
#define ADC_DIG_CTRL			0x19
#define ADC_DVC				0x1A
#define DAC_DIG_CTRL			0x1B
#define DAC_DVC				0x1C
#define DAC_MIX_SRC			0x1D
#define DIG_PADDRV_CTRL			0x1F

#define ADC_ANA_CTRL1			0x20
#define DAC_ANA_CTRL1			0x25
#define DAC_ANA_CTRL2			0x26
#define DAC_ANA_CTRL3			0x27
#define DAC_ANA_CTRL4			0x28

#define AGC_STA				0x30
#define AGC_CTRL			0x31
#define AGC_DEBT			0x32
#define AGC_TGLVL			0x33
#define AGC_MAXG			0x34
#define AGC_AVGC1			0x35
#define AGC_AVGC2			0x36
#define AGC_AVGC3			0x37
#define AGC_AVGC4			0x38
#define AGC_DECAYT1			0x39
#define AGC_DECAYT2			0x3A
#define AGC_ATTACKT1			0x3B
#define AGC_ATTACKT2			0x3C
#define AGC_NTH				0x3D
#define AGC_NAVGC1			0x3E
#define AGC_NAVGC2			0x3F

#define AGC_NAVGC3			0x40
#define AGC_NAVGC4			0x41
#define HPF_COEF1			0x42
#define HPF_COEF2			0x43
#define HPF_COEF3			0x44
#define HPF_COEF4			0x45
#define AGC_OPT				0x46
#define EQ_CTRL				0x4F

#define EQ1_B0_H			0x50
#define EQ1_B0_M			0x51
#define EQ1_B0_L			0x52
#define EQ1_B1_H			0x53
#define EQ1_B1_M			0x54
#define EQ1_B1_L			0x55
#define EQ1_B2_H			0x56
#define EQ1_B2_M			0x57
#define EQ1_B2_L			0x58
#define EQ1_A1_H			0x59
#define EQ1_A1_M			0x5A
#define EQ1_A1_L			0x5B
#define EQ1_A2_H			0x5C
#define EQ1_A2_M			0x5D
#define EQ1_A2_L			0x5E

#define EQ2_B0_H			0x60
#define EQ2_B0_M			0x61
#define EQ2_B0_L			0x62
#define EQ2_B1_H			0x63
#define EQ2_B1_M			0x64
#define EQ2_B1_L			0x65
#define EQ2_B2_H			0x66
#define EQ2_B2_M			0x67
#define EQ2_B2_L			0x68
#define EQ2_A1_H			0x69
#define EQ2_A1_M			0x6A
#define EQ2_A1_L			0x6B
#define EQ2_A2_H			0x6C
#define EQ2_A2_M			0x6D
#define EQ2_A2_L			0x6E

#define EQ3_B0_H			0x70
#define EQ3_B0_M			0x71
#define EQ3_B0_L			0x72
#define EQ3_B1_H			0x73
#define EQ3_B1_M			0x74
#define EQ3_B1_L			0x75
#define EQ3_B2_H			0x76
#define EQ3_B2_M			0x77
#define EQ3_B2_L			0x78
#define EQ3_A1_H			0x79
#define EQ3_A1_M			0x7A
#define EQ3_A1_L			0x7B
#define EQ3_A2_H			0x7C
#define EQ3_A2_M			0x7D
#define EQ3_A2_L			0x7E

/* AC102 codec register bit define */

/* PWR_CTRL1 */
#define ALDO_VCTRL			5
#define DLDO_VCTRL			2
#define MBIAS_VCTRL			0
/* PWR_CTRL1 */
#define IREF_CTRL			5
#define ALDO_EN				4
#define DLDO_EN				3
#define MBIAS_EN			2
#define VREF_EN				1
#define IREF_EN				0
/* SYS_FUNC_CTRL */
#define VREF_SPUP_STA			6
#define DAC_ANA_OUT_EN			0
/* SYS_CLK_ENA */
#define SYSCLK_EN			5
#define I2S_CLK_EN			4
#define EQ_CLK_EN			3
#define DAC_DIGITAL_CLK_EN		2
#define AGC_HPF_CLK_EN			1
#define ADC_DIGITAL_CLK_EN		0
/* I2S_CTRL */
#define BCLK_IOEN			7
#define LRCK_IOEN			6
#define SDO_EN				4
#define TXEN				2
#define RXEN				1
#define I2SGEN				0
/* I2S_BCLK_CTRL */
#define EDGE_TRANSFER			5
#define BCLK_POLARITY			4
#define BCLK_DIV			0
/* I2S_LRCK_CTRL1 */
#define LRCK_POLARITY			4
#define LRCK_PERIODH			0
/* I2S_FMT_CTRL1 */
#define MODE_SEL			4
#define OFFSET				2
#define TX_SLOT_HIZ			1
#define TX_STATE			0
/* I2S_FMT_CTRL2 */
#define I2S_FMT_CTRL2_SW		4
#define I2S_FMT_CTRL2_SR		0
/* I2S_FMT_CTRL3 */
#define TX_RX_MLS			7
#define SEXT				5
#define OUT_MUTE			3
#define LRCK_WIDTH			2
#define TX_RX_PDM			0
/* I2S_SLOT_CTRL */
#define RX_CHSEL			2
#define TX_CHSEL			0
/* I2S_TX_CHMP_CTRL */
#define TX_CH2_MAP			1
#define TX_CH1_MAP			0
/* I2S_TX_MIX_SRC */
#define TX_MIXR_GAIN			6
#define TX_MIXL_GAIN			4
#define TX_MIXR_SRC			2
#define TX_MIXL_SRC			0
/* I2S_RX_CHMP_CTRL */
#define RX_CH2_MAP			2
#define RX_CH1_MAP			0
/* I2S_RX_MIX_SRC */
#define RX_MIX_GAIN			2
#define RX_MIX_SRC			0
/* ADC_DIG_CTRL */
#define ADC_PTN_SEL			4
#define ADOUT_DTS			2
#define ADOUT_DLY_EN			1
#define ADC_DIG_EN			0
/* DAC_DIG_CTRL */
#define DVC_ZCD_EN			6
#define DITHER_SGM			3
#define DAC_PTN_SEL			1
#define DAC_DIG_EN			0
/* DAC_MIX_SRC */
#define DAC_MIX_GAIN			2
#define DAC_MIX_SRC_BIT			0
/* DIG_PADDRV_CTRL */
#define SDOUT_DRV			4
#define LRCK_DRV			2
#define BCLK_DRV			0
/* ADC_ANA_CTRL1 */
#define PGA_GAIN_CTRL			3
#define PGA_CTRL_RCM			1
#define ADC_GEN				0
/* DAC_ANA_CTRL1 */
#define VRDA_EN				5
/* DAC_ANA_CTRL2 */
#define LINEODIFEN			4
#define LINEOAMPGAIN			0

/* AGC_CTRL */
#define AGC_ENABLE			4
#define HPF_ENABLE			3
#define NOISE_DETECT_ENABLE		2
#define AGC_HYS_SET			0

/*********************some config value**********************/

//I2S BCLK POLARITY Control
#define BCLK_NORMAL_DRIVE_N_SAMPLE_P	0
#define BCLK_INVERT_DRIVE_P_SAMPLE_N	1
//I2S LRCK POLARITY Control
#define	LRCK_LEFT_LOW_RIGHT_HIGH	0
#define LRCK_LEFT_HIGH_RIGHT_LOW	1
//I2S Format Selection
#define PCM_FORMAT			0
#define LEFT_JUSTIFIED_FORMAT		1
#define RIGHT_JUSTIFIED_FORMAT		2
//I2S Sign Extend in slot
#define ZERO_OR_AUDIIO_GAIN_PADDING_LSB	0
#define SIGN_EXTENSION_MSB		1
#define TRANSFER_ZERO_AFTER		3
//ADC Digital Debug Control
#define ADC_PTN_NORMAL			0
#define ADC_PTN_0x5A5A5A		1
#define ADC_PTN_0x123456		2
#define ADC_PTN_I2S_RX_DATA		3

#endif
