static void TP2825B_output(unsigned char chip)
{
	if (MUX656_8BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x00);
		tp28xx_byte_write(chip, 0x08, 0xf0);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4C, 0x40);
		tp28xx_byte_write(chip, 0x4e, 0x05); //port1
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x00);
	} else if (SEP656_8BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x00);
		tp28xx_byte_write(chip, 0x08, 0xf0);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4C, 0x33);
		tp28xx_byte_write(chip, 0x4e, 0x05); //port1
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x00);
	} else if (EMB422_16BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x00);
		tp28xx_byte_write(chip, 0x08, 0xf0);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4C, 0x40);
		tp28xx_byte_write(chip, 0x4e, 0x0d);
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x44);
	} else if (SEP422_16BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x00);
		tp28xx_byte_write(chip, 0x08, 0xf0);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4C, 0x43);
		tp28xx_byte_write(chip, 0x4e, 0x0d);
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x44);
	}
}
static void TP2850_output(unsigned char chip)
{
	if (MUX656_8BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x00);
		tp28xx_byte_write(chip, 0x08, 0xf0);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4C, 0x40);
		tp28xx_byte_write(chip, 0x4e, 0x05); //port1
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x00);
	} else if (SEP656_8BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x00);
		tp28xx_byte_write(chip, 0x02, 0x00);
		tp28xx_byte_write(chip, 0x08, 0xf0);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4C, 0x43);
		tp28xx_byte_write(chip, 0x4e, 0x05); //port1
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x00);
	} else if (MIPI_2LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x01, 0xf8);
		tp28xx_byte_write(chip, 0x02, 0x01);
		tp28xx_byte_write(chip, 0x08, 0x03);
		tp28xx_byte_write(chip, 0x20, 0x12);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4e, 0x00);
	}
}
static void TP2860_output(unsigned char chip)
{
	if (MUX656_8BIT == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x00, 0x44);
		tp28xx_byte_write(chip, 0x02, 0xb0);
		tp28xx_byte_write(chip, 0x03, 0xb0);
		tp28xx_byte_write(chip, 0x04, 0xb0);
		tp28xx_byte_write(chip, 0x05, 0xb0);
		tp28xx_byte_write(chip, 0x06, 0xb0);

		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x42, 0x00);
		tp28xx_byte_write(chip, 0x4e, 0x1d); //port1
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0xf7, 0x44);
		tp28xx_byte_write(chip, 0xfa, 0x88);
		tp28xx_byte_write(chip, 0xfd, 0x80);
		tp28xx_byte_write(chip, 0xf6, 0x00);
		tp28xx_byte_write(chip, 0x1b, 0x01);

	} else if (SEP656_8BIT == output[chip]) {

	} else if (MIPI_2LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x02, 0x79);
		tp28xx_byte_write(chip, 0x03, 0x71);
		tp28xx_byte_write(chip, 0x04, 0x71);
		tp28xx_byte_write(chip, 0x13, 0xef);
		tp28xx_byte_write(chip, 0x20, 0x00);
		tp28xx_byte_write(chip, 0x21, 0x12);
		tp28xx_byte_write(chip, 0x23, 0x9e);

		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4e, 0x00);
		tp28xx_byte_write(chip, 0x54, 0x00);

	} else if (MIPI_1LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x02, 0x79);
		tp28xx_byte_write(chip, 0x03, 0x71);
		tp28xx_byte_write(chip, 0x04, 0x71);
		tp28xx_byte_write(chip, 0x13, 0xef);
		tp28xx_byte_write(chip, 0x20, 0x00);
		tp28xx_byte_write(chip, 0x21, 0x11);
		tp28xx_byte_write(chip, 0x23, 0x9e);

		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0x4e, 0x00);
		tp28xx_byte_write(chip, 0x54, 0x00);
	}
}
#if 0
static void TP2860_SYSCLK_144M(unsigned char chip)
{
	if (MIPI_2LANES == output[chip]) {

		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x9e);
		tp28xx_byte_write(chip, 0x13, 0x27);
		tp28xx_byte_write(chip, 0x14, 0x41);
		tp28xx_byte_write(chip, 0x15, 0x02);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x05);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else if (MIPI_1LANES == output[chip]) {

		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x9e);
		tp28xx_byte_write(chip, 0x13, 0x27);
		tp28xx_byte_write(chip, 0x14, 0x00);
		tp28xx_byte_write(chip, 0x15, 0x02);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x05);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else {

	}
}
#endif

static void TP2860_SYSCLK_94500K(unsigned char chip)
{
	if (MIPI_2LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x5a);
		tp28xx_byte_write(chip, 0x13, 0x0f);
		tp28xx_byte_write(chip, 0x14, 0x41);
		tp28xx_byte_write(chip, 0x15, 0x02);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x03);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else if (MIPI_1LANES == output[chip]) {

		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x5a);
		tp28xx_byte_write(chip, 0x13, 0x0f);
		tp28xx_byte_write(chip, 0x14, 0x00);
		tp28xx_byte_write(chip, 0x15, 0x02);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x03);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else {

	}
}
static void TP2860_SYSCLK_111375K(unsigned char chip)
{
	if (MIPI_2LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x5f);
		tp28xx_byte_write(chip, 0x13, 0x0f);
		tp28xx_byte_write(chip, 0x14, 0x41);
		tp28xx_byte_write(chip, 0x15, 0x02);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x03);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else if (MIPI_1LANES == output[chip]) {

		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x5f);
		tp28xx_byte_write(chip, 0x13, 0x0f);
		tp28xx_byte_write(chip, 0x14, 0x00);
		tp28xx_byte_write(chip, 0x15, 0x02);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x03);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else {

	}
}
static void TP2860_SYSCLK_A720P(unsigned char chip)
{
	if (MIPI_2LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x14, 0x50);
		tp28xx_byte_write(chip, 0x15, 0x09);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x03);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else if (MIPI_1LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x14, 0x47);
		tp28xx_byte_write(chip, 0x15, 0x09);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x03);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x14, 0x50);
		tp28xx_byte_write(chip, 0x15, 0x09);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0xf6, 0x04);
		tp28xx_byte_write(chip, 0xfa, 0x03);
	}

}
static void TP2860_SYSCLK_A1080P(unsigned char chip)
{
	if (MIPI_2LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x14, 0x40);
		tp28xx_byte_write(chip, 0x15, 0x05);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x06);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else if (MIPI_1LANES == output[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x14, 0x07);
		tp28xx_byte_write(chip, 0x15, 0x05);

		tp28xx_byte_write(chip, 0x2a, 0x04);
		tp28xx_byte_write(chip, 0x2b, 0x06);
		tp28xx_byte_write(chip, 0x2c, 0x03);
		tp28xx_byte_write(chip, 0x2e, 0x02);

		tp28xx_byte_write(chip, 0x10, 0xa0);
		tp28xx_byte_write(chip, 0x10, 0x20);
		tp28xx_byte_write(chip, 0x28, 0x02);
		tp28xx_byte_write(chip, 0x28, 0x00);
		tp28xx_byte_write(chip, 0x40, 0x00);
	} else {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x14, 0x40);
		tp28xx_byte_write(chip, 0x15, 0x05);
		tp28xx_byte_write(chip, 0x40, 0x00);
		tp28xx_byte_write(chip, 0xf6, 0x04);
		tp28xx_byte_write(chip, 0xfa, 0x03);
	}

}
////////////////////////////////////////////////////
static void TP2825B_SYSCLK_V2(unsigned char chip)
{
	if (TP2860 == id[chip]) {
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x41);
			tp28xx_byte_write(chip, 0x15, 0x12);

			// 960x480/960x576
			if (TP2802_PAL == mode || TP2802_NTSC == mode) {
				tp28xx_byte_write(chip, 0x14, 0x62);
				tp28xx_byte_write(chip, 0x15, 0x07);
			}

			tp28xx_byte_write(chip, 0x2a, 0x04);
			tp28xx_byte_write(chip, 0x2b, 0x03);
			tp28xx_byte_write(chip, 0x2c, 0x03);
			tp28xx_byte_write(chip, 0x2e, 0x02);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else if (MIPI_1LANES == output[chip])	{
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x00);
			tp28xx_byte_write(chip, 0x15, 0x12);

			// 960x480/960x576
			if (TP2802_PAL == mode || TP2802_NTSC == mode) {
				tp28xx_byte_write(chip, 0x14, 0x51);
				tp28xx_byte_write(chip, 0x15, 0x07);
			}

			tp28xx_byte_write(chip, 0x2a, 0x04);
			tp28xx_byte_write(chip, 0x2b, 0x03);
			tp28xx_byte_write(chip, 0x2c, 0x03);
			tp28xx_byte_write(chip, 0x2e, 0x02);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x12, 0x54);
			tp28xx_byte_write(chip, 0x13, 0xef);
			tp28xx_byte_write(chip, 0x14, 0x41);
			tp28xx_byte_write(chip, 0x15, 0x12);

			tp28xx_byte_write(chip, 0x40, 0x00);
			tp28xx_byte_write(chip, 0xf6, 0x00);
			tp28xx_byte_write(chip, 0xfa, 0x00);
		}
	} else {
		//TP2850/TP825B
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x24);
			tp28xx_byte_write(chip, 0x14, 0x46);

			// 960x480/960x576
			if (TP2802_PAL == mode || TP2802_NTSC == mode)
				tp28xx_byte_write(chip, 0x14, 0x57);

			tp28xx_byte_write(chip, 0x15, 0x09);
			tp28xx_byte_write(chip, 0x39, 0x00);

			tp28xx_byte_write(chip, 0x25, 0x08);
			tp28xx_byte_write(chip, 0x26, 0x01);
			tp28xx_byte_write(chip, 0x27, 0x0e);

			tp28xx_byte_write(chip, 0x10, 0x88);
			tp28xx_byte_write(chip, 0x10, 0x08);
			tp28xx_byte_write(chip, 0x23, 0x02);
			tp28xx_byte_write(chip, 0x23, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x24);
			tp28xx_byte_write(chip, 0x14, 0x73);
			tp28xx_byte_write(chip, 0x40, 0x00);
			//tp28xx_byte_write(chip, 0x35, 0x25);
		}
	}
}

static void TP2825B_SYSCLK_V1(unsigned char chip)
{
	if (TP2860 == id[chip]) {
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x41);
			tp28xx_byte_write(chip, 0x15, 0x02);

			tp28xx_byte_write(chip, 0x2a, 0x04);
			tp28xx_byte_write(chip, 0x2b, 0x06);
			tp28xx_byte_write(chip, 0x2c, 0x03);
			tp28xx_byte_write(chip, 0x2e, 0x02);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else if (MIPI_1LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x00);
			tp28xx_byte_write(chip, 0x15, 0x02);

			tp28xx_byte_write(chip, 0x2a, 0x04);
			tp28xx_byte_write(chip, 0x2b, 0x06);
			tp28xx_byte_write(chip, 0x2c, 0x03);
			tp28xx_byte_write(chip, 0x2e, 0x02);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x12, 0x54);
			tp28xx_byte_write(chip, 0x13, 0xef);
			tp28xx_byte_write(chip, 0x14, 0x41);
			tp28xx_byte_write(chip, 0x15, 0x02);

			tp28xx_byte_write(chip, 0x40, 0x00);
			tp28xx_byte_write(chip, 0xf6, 0x00);
			tp28xx_byte_write(chip, 0xfa, 0x00);
		}
	} else {
		//TP2850/TP825B
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x04);
			tp28xx_byte_write(chip, 0x14, 0x46);
			tp28xx_byte_write(chip, 0x15, 0x09);
			tp28xx_byte_write(chip, 0x39, 0x00);

			tp28xx_byte_write(chip, 0x25, 0x08);
			tp28xx_byte_write(chip, 0x26, 0x04);
			tp28xx_byte_write(chip, 0x27, 0x0c);

			tp28xx_byte_write(chip, 0x10, 0x88);
			tp28xx_byte_write(chip, 0x10, 0x08);
			tp28xx_byte_write(chip, 0x23, 0x02);
			tp28xx_byte_write(chip, 0x23, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x04);
			tp28xx_byte_write(chip, 0x14, 0x73);
			tp28xx_byte_write(chip, 0x40, 0x00);
			//tp28xx_byte_write(chip, 0x35, 0x05);
		}
	}
}

static void TP2825B_SYSCLK_V3(unsigned char chip)
{
	unsigned char tmp;

	if (TP2860 == id[chip]) {
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x00);
			tp28xx_byte_write(chip, 0x15, 0x01);

			tp28xx_byte_write(chip, 0x2a, 0x08);
			tp28xx_byte_write(chip, 0x2b, 0x06);
			tp28xx_byte_write(chip, 0x2c, 0x11);
			tp28xx_byte_write(chip, 0x2e, 0x0a);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else if (MIPI_1LANES == output[chip])	{
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x00);
			tp28xx_byte_write(chip, 0x15, 0x02);

			tp28xx_byte_write(chip, 0x2a, 0x08);
			tp28xx_byte_write(chip, 0x2b, 0x06);
			tp28xx_byte_write(chip, 0x2c, 0x11);
			tp28xx_byte_write(chip, 0x2e, 0x0a);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);

			tmp = tp28xx_byte_read(chip, 0x35);
			tmp |= 0x40;
			tp28xx_byte_write(chip, 0x35, tmp);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x12, 0x54);
			tp28xx_byte_write(chip, 0x13, 0xef);
			tp28xx_byte_write(chip, 0x14, 0x00);
			tp28xx_byte_write(chip, 0x15, 0x01);
			tp28xx_byte_write(chip, 0x40, 0x00);
			tp28xx_byte_write(chip, 0xf6, 0x04);
			tp28xx_byte_write(chip, 0xfa, 0x09);
		}
	} else {
		//TP2850/TP825B
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x04);
			tp28xx_byte_write(chip, 0x14, 0x05);
			tp28xx_byte_write(chip, 0x15, 0x04);
			tp28xx_byte_write(chip, 0x39, 0x01);

			tp28xx_byte_write(chip, 0x25, 0x08);
			tp28xx_byte_write(chip, 0x26, 0x04);
			tp28xx_byte_write(chip, 0x27, 0x10);

			tp28xx_byte_write(chip, 0x10, 0x88);
			tp28xx_byte_write(chip, 0x10, 0x08);
			tp28xx_byte_write(chip, 0x23, 0x02);
			tp28xx_byte_write(chip, 0x23, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x04);
			tp28xx_byte_write(chip, 0x14, 0x73);
			tp28xx_byte_write(chip, 0x40, 0x00);

			if (MUX656_8BIT == output[chip] || SEP656_8BIT == output[chip]) {
				tmp = tp28xx_byte_read(chip, 0x35);
				tmp |= 0x40;
				tp28xx_byte_write(chip, 0x35, tmp);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////
static void TP2825B_SYSCLK_CVBS(unsigned char chip)
{
	if (TP2860 == id[chip]) {
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);

			// 960x480/960x576
			// if(TP2802_PAL == mode || TP2802_NTSC == mode)
			{
				tp28xx_byte_write(chip, 0x14, 0x62);
				tp28xx_byte_write(chip, 0x15, 0x07);
			}

			tp28xx_byte_write(chip, 0x2a, 0x04);
			tp28xx_byte_write(chip, 0x2b, 0x03);
			tp28xx_byte_write(chip, 0x2c, 0x03);
			tp28xx_byte_write(chip, 0x2e, 0x02);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else if (MIPI_1LANES == output[chip])	{
			tp28xx_byte_write(chip, 0x40, 0x08);

			// 960x480/960x576
			//if(TP2802_PAL == mode || TP2802_NTSC == mode)
			{
				tp28xx_byte_write(chip, 0x14, 0x51);
				tp28xx_byte_write(chip, 0x15, 0x07);
			}

			tp28xx_byte_write(chip, 0x2a, 0x04);
			tp28xx_byte_write(chip, 0x2b, 0x03);
			tp28xx_byte_write(chip, 0x2c, 0x03);
			tp28xx_byte_write(chip, 0x2e, 0x02);

			tp28xx_byte_write(chip, 0x10, 0xa0);
			tp28xx_byte_write(chip, 0x10, 0x20);
			tp28xx_byte_write(chip, 0x28, 0x02);
			tp28xx_byte_write(chip, 0x28, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x14, 0x41);
			tp28xx_byte_write(chip, 0x15, 0x12);
			tp28xx_byte_write(chip, 0x40, 0x00);
			//tp28xx_byte_write(chip, 0x35, 0x25);
		}
	} else {
		//TP2850/TP825B
		if (MIPI_2LANES == output[chip]) {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x24);

			// 960x480/960x576
			//if(TP2802_PAL == mode || TP2802_NTSC == mode)
			{
				tp28xx_byte_write(chip, 0x14, 0x57);
			}

			tp28xx_byte_write(chip, 0x15, 0x09);
			tp28xx_byte_write(chip, 0x39, 0x00);

			tp28xx_byte_write(chip, 0x25, 0x08);
			tp28xx_byte_write(chip, 0x26, 0x01);
			tp28xx_byte_write(chip, 0x27, 0x0e);

			tp28xx_byte_write(chip, 0x10, 0x88);
			tp28xx_byte_write(chip, 0x10, 0x08);
			tp28xx_byte_write(chip, 0x23, 0x02);
			tp28xx_byte_write(chip, 0x23, 0x00);
			tp28xx_byte_write(chip, 0x40, 0x00);
		} else {
			tp28xx_byte_write(chip, 0x40, 0x08);
			tp28xx_byte_write(chip, 0x13, 0x24);
			tp28xx_byte_write(chip, 0x14, 0x73);
			tp28xx_byte_write(chip, 0x40, 0x00);
			//tp28xx_byte_write(chip, 0x35, 0x25);
		}
	}
}
//////////////////////////////////////////////////////////////////////
void TP2825B_StartTX(unsigned char chip, unsigned char ch)
{
	unsigned char tmp;
	int i;
	tp28xx_byte_write(chip, 0xB6, 0x01); //clear flag

	tmp = tp28xx_byte_read(chip, 0x6f);
	tmp |= 0x01;
	tp28xx_byte_write(chip, 0x6f, tmp); //TX enable
	tmp &= 0xfe;
	tp28xx_byte_write(chip, 0x6f, tmp); //TX disable

	i = 10;
	while (i--) {
		if (0x01 & tp28xx_byte_read(chip, 0xb6))
			break;
		//udelay(1000);
		msleep(20);
	}
}

static void TP2825B_RX_init(unsigned char chip, unsigned char mod)
{
	int i, index = 0;
	unsigned char regA7 = 0x00;
	unsigned char regA8 = 0x00;

	//regC9~D7
	static const unsigned char PTZ_RX_dat[][15] = {
		{0x00, 0x00, 0x07, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x60, 0x10, 0x06, 0xbe, 0x39, 0x27}, //TVI command
		{0x00, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x04, 0x00, 0x00, 0x60, 0x10, 0x06, 0xbe, 0x39, 0x27}, //TVI burst
		{0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x05, 0xbf, 0x11, 0x60, 0x0b, 0x04, 0xf0, 0xd8, 0x2f}, //ACP1 0.525
		{0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x02, 0xdf, 0x88, 0x60, 0x10, 0x04, 0xf0, 0xd8, 0x17}, //ACP2 0.6
		{0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x04, 0xec, 0xe9, 0x60, 0x10, 0x04, 0xf0, 0xd8, 0x17}, //ACP3 0.35
		{0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x03, 0x52, 0x53, 0x60, 0x10, 0x04, 0xf0, 0xd8, 0x17} //test
	};

	if (PTZ_RX_TVI_CMD == mod) {
		index = 0;
		regA7 = 0x03;
		regA8 = 0x00;
	} else if (PTZ_RX_TVI_BURST == mod) {
		index = 1;
		regA7 = 0x03;
		regA8 = 0x00;
	} else if (PTZ_RX_ACP1 == mod) {
		index = 2;
		regA7 = 0x03;
		regA8 = 0x00;
	} else if (PTZ_RX_ACP2 == mod) {
		index = 3;
		regA7 = 0x27;
		regA8 = 0x0f;
	} else if (PTZ_RX_ACP3 == mod) {
		index = 4;
		regA7 = 0x27;
		regA8 = 0x0f;
	} else if (PTZ_RX_TEST == mod) {
		index = 5;
		regA7 = 0x03;
		regA8 = 0x00;
	}

	for (i = 0; i < 15; i++) {
		tp28xx_byte_write(chip, 0xc9 + i, PTZ_RX_dat[index][i]);
		tp28xx_byte_write(chip, 0xa8, regA8);
		tp28xx_byte_write(chip, 0xa7, regA7);
	}

}
static void TP2825B_PTZ_mode(unsigned char chip, unsigned char ch, unsigned char mod)
{
	unsigned int tmp, i, index = 0;

	static const unsigned char PTZ_reg[12] = {
		0x6f, 0x70, 0x71, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8
	};
	static const unsigned char PTZ_dat[][12] = {
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x19, 0x78, 0x21}, //TVI1.0
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x33, 0xf0, 0x21}, //TVI2.0
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0e, 0x0f, 0x10, 0x11, 0x26, 0xf0, 0x57}, //A1080p for 2826 0.525
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0e, 0x0f, 0x00, 0x00, 0x26, 0xb0, 0x6f}, //A720p for 2826 0.525
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0f, 0x10, 0x00, 0x00, 0x4a, 0xf0, 0x6f}, //960H for 2826
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x15, 0xb8, 0xa2}, //HDC for 2826
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0f, 0x10, 0x11, 0x12, 0x2c, 0xf0, 0x57}, //ACP 3M18 for 2826 8+0.6
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x0f, 0x10, 0x11, 0x12, 0x19, 0xd0, 0x17}, //ACP 3M2530 for 2826 4+0.35
		{0x46, 0x5f, 0x00, 0x00, 0x00, 0x0f, 0x10, 0x12, 0x16, 0x16, 0xd0, 0x57}, //ACP 4M2530_5M20 for 2826 7+0.3
		{0x46, 0x5f, 0x00, 0x00, 0x00, 0x0f, 0x10, 0x12, 0x16, 0x2c, 0xf0, 0x97}, //ACP 4M15 5M12.5 for 2826 8+0.6
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x16, 0x17, 0x18, 0x19, 0x2a, 0xf0, 0x1a}, //HDC QHD25/30 for 2827C
		{0x4a, 0x5d, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x15, 0xb8, 0x9f}, //HDC for 2827C FIFO mode
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x36, 0x50, 0x62}, //HDC 4K12.5 for 2827C
		{0x42, 0x40, 0x00, 0x00, 0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x36, 0x50, 0x5a} //HDC 4K15 for 2827C
	};

	if (PTZ_TVI == mod) {
		/*
		tmp = tp28xx_byte_read(chip, 0xf5); //check TVI 1 or 2
		if ((tmp >>ch) & 0x01)
		    index = 1;
		else
		    index = 0;
		*/
		index = 0;
	} else if (PTZ_HDA_1080P == mod) {
		index = 2;
	} else if (PTZ_HDA_720P == mod) {
		index = 3;
	} else if (PTZ_HDA_CVBS == mod) {
		index = 4;
	} else if (PTZ_HDC == mod) {
		index = 5;
	} else if (PTZ_HDA_3M18 == mod) {
		index = 6;
	} else if (PTZ_HDA_3M25 == mod) {
		index = 7;
	} else if (PTZ_HDA_4M25 == mod) {
		index = 8;
	} else if (PTZ_HDA_4M15 == mod) {
		index = 9;
	} else if (PTZ_HDC_QHD == mod) {
		index = 10;
	} else if (PTZ_HDC_FIFO == mod) {
		index = 11;
	} else if (PTZ_HDC_8M12 == mod) {
		index = 12;
	} else if (PTZ_HDC_8M15 == mod) {
		index = 13;
	}

	for (i = 0; i < 12; i++)
		tp28xx_byte_write(chip, PTZ_reg[i], PTZ_dat[index][i]);

	if (PTZ_PIN_TXD == ch) {
		tmp = tp28xx_byte_read(chip, 0x6f);
		tmp |= 0x40;
		tp28xx_byte_write(chip, 0x6f, tmp);
	} else if (PTZ_PIN_PTZ1 == ch) {
		tmp = tp28xx_byte_read(chip, 0xf0);
		tmp &= 0xcf;
		tmp |= 0x10;
		tp28xx_byte_write(chip, 0xf0, tmp);
	} else if (PTZ_PIN_PTZ2 == ch) {
		tmp = tp28xx_byte_read(chip, 0xf0);
		tmp &= 0xcf;
		tmp |= 0x20;
		tp28xx_byte_write(chip, 0xf0, tmp);
	}
}

static void TP2825B_reset_default(unsigned char chip, unsigned char ch)
{
	unsigned int tmp;

	if (TP2860 == id[chip]) {
		tp28xx_byte_write(chip, 0x40, 0x08);
		tp28xx_byte_write(chip, 0x12, 0x54);
		tp28xx_byte_write(chip, 0x13, 0xef);
		tp28xx_byte_write(chip, 0x40, 0x00);
	}

	tp28xx_byte_write(chip, 0x07, 0xC0);
	tp28xx_byte_write(chip, 0x0B, 0xC0);
	tmp = tp28xx_byte_read(chip, 0x26);
	tmp &= 0xfe;
	tp28xx_byte_write(chip, 0x26, tmp);
	tmp = tp28xx_byte_read(chip, 0xa7);
	tmp &= 0xfe;
	tp28xx_byte_write(chip, 0xa7, tmp);
	tmp = tp28xx_byte_read(chip, 0x06);
	tmp &= 0xfb;
	tp28xx_byte_write(chip, 0x06, tmp);
}

//////////////////////////////////////////////////////////////
static void TP2825B_NTSC_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x13);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x40);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x70);
	tp28xx_byte_write(chip, 0x2c, 0x2a);
	tp28xx_byte_write(chip, 0x2d, 0x68);
	tp28xx_byte_write(chip, 0x2e, 0x57);

	tp28xx_byte_write(chip, 0x30, 0x62);
	tp28xx_byte_write(chip, 0x31, 0xbb);
	tp28xx_byte_write(chip, 0x32, 0x96);
	tp28xx_byte_write(chip, 0x33, 0xc0);
	tp28xx_byte_write(chip, 0x35, 0x25);
	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x04);
	tp28xx_byte_write(chip, 0x3a, 0x32);
	tp28xx_byte_write(chip, 0x3B, 0x26);

	tp28xx_byte_write(chip, 0x18, 0x12);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
static void TP2825B_PAL_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x13);
	tp28xx_byte_write(chip, 0x0d, 0x51);

	tp28xx_byte_write(chip, 0x20, 0x48);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x37);
	tp28xx_byte_write(chip, 0x23, 0x3f);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x70);
	tp28xx_byte_write(chip, 0x2c, 0x2a);
	tp28xx_byte_write(chip, 0x2d, 0x64);
	tp28xx_byte_write(chip, 0x2e, 0x56);

	tp28xx_byte_write(chip, 0x30, 0x7a);
	tp28xx_byte_write(chip, 0x31, 0x4a);
	tp28xx_byte_write(chip, 0x32, 0x4d);
	tp28xx_byte_write(chip, 0x33, 0xf0);
	tp28xx_byte_write(chip, 0x35, 0x25);
	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x04);
	tp28xx_byte_write(chip, 0x3a, 0x32);
	tp28xx_byte_write(chip, 0x3B, 0x26);

	tp28xx_byte_write(chip, 0x18, 0x17);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
static void TP2825B_V1_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x30);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x30);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0xbb);
	tp28xx_byte_write(chip, 0x32, 0x2e);
	tp28xx_byte_write(chip, 0x33, 0x90);
	tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x1c);
	tp28xx_byte_write(chip, 0x3a, 0x32);
	tp28xx_byte_write(chip, 0x3B, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
static void TP2825B_V2_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x13);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x30);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x30);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0xbb);
	tp28xx_byte_write(chip, 0x32, 0x2e);
	tp28xx_byte_write(chip, 0x33, 0x90);
	tp28xx_byte_write(chip, 0x35, 0x25);
	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x18);
	tp28xx_byte_write(chip, 0x3a, 0x32);
	tp28xx_byte_write(chip, 0x3B, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}

/////HDA QHD30
static void TP2825B_AQHDP30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tp28xx_byte_write(chip, 0x15, 0x23);
	tp28xx_byte_write(chip, 0x16, 0x16);
	tp28xx_byte_write(chip, 0x18, 0x32);

	tp28xx_byte_write(chip, 0x20, 0x80);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x36);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2d, 0xa0);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0x6a);
	tp28xx_byte_write(chip, 0x32, 0xbe);
	tp28xx_byte_write(chip, 0x33, 0x80);
	//tp28xx_byte_write(chip, 0x35, 0x15);
	tp28xx_byte_write(chip, 0x39, 0x40);
}

/////HDA QHD25
static void TP2825B_AQHDP25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tp28xx_byte_write(chip, 0x15, 0x23);
	tp28xx_byte_write(chip, 0x16, 0x16);
	tp28xx_byte_write(chip, 0x18, 0x32);

	tp28xx_byte_write(chip, 0x20, 0x80);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x36);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2d, 0xa0);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0x6f);
	tp28xx_byte_write(chip, 0x32, 0xb5);
	tp28xx_byte_write(chip, 0x33, 0x80);
	//tp28xx_byte_write(chip, 0x35, 0x15);
	tp28xx_byte_write(chip, 0x39, 0x40);
}

/////HDA QXGA30
static void TP2825B_AQXGAP30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x00);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x90);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x01);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0xa0);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0x68);
	tp28xx_byte_write(chip, 0x32, 0xbe);
	tp28xx_byte_write(chip, 0x33, 0x80);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x40);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);
}

/////HDA QXGA25
static void TP2825B_AQXGAP25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x60;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x90);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x01);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0xa0);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0x6c);
	tp28xx_byte_write(chip, 0x32, 0xbe);
	tp28xx_byte_write(chip, 0x33, 0x80);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x40);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);
}


/////HDC QHD30
static void TP2825B_CQHDP30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x40);
	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0xfa);
	tp28xx_byte_write(chip, 0x18, 0x38);
	tp28xx_byte_write(chip, 0x1c, 0x0c);
	tp28xx_byte_write(chip, 0x1d, 0x80);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x38);

	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);

	tp28xx_byte_write(chip, 0x2d, 0x6c);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x31);

	tp28xx_byte_write(chip, 0x39, 0x48);
}

/////HDC QHD25
static void TP2825B_CQHDP25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x40);
	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0xd8);
	tp28xx_byte_write(chip, 0x18, 0x30);
	tp28xx_byte_write(chip, 0x1c, 0x0c);
	tp28xx_byte_write(chip, 0x1d, 0x80);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x38);

	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);

	tp28xx_byte_write(chip, 0x2d, 0x6c);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x3b);

	tp28xx_byte_write(chip, 0x39, 0x48);
}

///////HDA QHD15
static void TP2825B_AQHDP15_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x01);

	tp28xx_byte_write(chip, 0x2d, 0x44);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x29);
	tp28xx_byte_write(chip, 0x31, 0x68);
	tp28xx_byte_write(chip, 0x32, 0x78);
	tp28xx_byte_write(chip, 0x33, 0x10);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x40);
	tp28xx_byte_write(chip, 0x3a, 0x12);
}

/////HDA QXGA18
static void TP2825B_AQXGAP18_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0x10);
	tp28xx_byte_write(chip, 0x18, 0x68);

	tp28xx_byte_write(chip, 0x20, 0x48);
	tp28xx_byte_write(chip, 0x21, 0x46);
	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x05);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2d, 0x52);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x29);
	tp28xx_byte_write(chip, 0x31, 0x65);
	tp28xx_byte_write(chip, 0x32, 0x2b);
	tp28xx_byte_write(chip, 0x33, 0xd0);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x40);
	tp28xx_byte_write(chip, 0x3a, 0x12);
}

/////TVI QHD30/QHD25
static void TP2825B_QHDP30_25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x23);
	tp28xx_byte_write(chip, 0x16, 0x1b);
	tp28xx_byte_write(chip, 0x18, 0x38);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x58);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0x58);
	tp28xx_byte_write(chip, 0x32, 0x9f);
	tp28xx_byte_write(chip, 0x33, 0x60);

	tp28xx_byte_write(chip, 0x35, 0x15);
	tp28xx_byte_write(chip, 0x36, 0xdc);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);
}

/////TVI 5M20
static void TP2825B_5MP20_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x54);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0xa7);
	tp28xx_byte_write(chip, 0x32, 0x18);
	tp28xx_byte_write(chip, 0x33, 0x50);

	//tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}

/////HDA 5M20
static void TP2825B_A5MP20_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x20, 0x80);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x36);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2d, 0xA0);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0x77);
	tp28xx_byte_write(chip, 0x32, 0x0e);
	tp28xx_byte_write(chip, 0x33, 0xa0);
	tp28xx_byte_write(chip, 0x39, 0x48);
}

/////TVI 8M15
static void TP2825B_8MP15_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x60);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x58);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0x59);
	tp28xx_byte_write(chip, 0x32, 0xbd);
	tp28xx_byte_write(chip, 0x33, 0x60);

	//tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
///////HDA 5M12.5
static void TP2825B_A5MP12_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x01);

	tp28xx_byte_write(chip, 0x2d, 0x44);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x29);
	tp28xx_byte_write(chip, 0x31, 0x68);
	tp28xx_byte_write(chip, 0x32, 0x72);
	tp28xx_byte_write(chip, 0x33, 0xb0);

	tp28xx_byte_write(chip, 0x16, 0x10);
	tp28xx_byte_write(chip, 0x18, 0x1a);
	tp28xx_byte_write(chip, 0x1d, 0xb8);
	tp28xx_byte_write(chip, 0x36, 0xbc);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x40);
	tp28xx_byte_write(chip, 0x3a, 0x12);
}
///////////////////////////////////////////////////////////////////
static void TP2825B_A720P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x02);
	tmp |= 0x04;
	tp28xx_byte_write(chip, 0x02, tmp);

	tp28xx_byte_write(chip, 0x0d, 0x70);

	tp28xx_byte_write(chip, 0x20, 0x40);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x01);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x5a);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x9d);
	tp28xx_byte_write(chip, 0x31, 0xca);
	tp28xx_byte_write(chip, 0x32, 0x01);
	tp28xx_byte_write(chip, 0x33, 0xd0);
}
static void TP2825B_A720P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x02);
	tmp |= 0x04;
	tp28xx_byte_write(chip, 0x02, tmp);

	tp28xx_byte_write(chip, 0x0d, 0x71);

	tp28xx_byte_write(chip, 0x20, 0x40);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x01);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x5a);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x9e);
	tp28xx_byte_write(chip, 0x31, 0x20);
	tp28xx_byte_write(chip, 0x32, 0x10);
	tp28xx_byte_write(chip, 0x33, 0x90);
}
static void TP2825B_A1080P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x02);
	tmp |= 0x04;
	tp28xx_byte_write(chip, 0x02, tmp);

	tp28xx_byte_write(chip, 0x15, 0x01);
	tp28xx_byte_write(chip, 0x16, 0xf0);

	tp28xx_byte_write(chip, 0x0d, 0x72);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x54);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0xa5);
	tp28xx_byte_write(chip, 0x31, 0x95);
	tp28xx_byte_write(chip, 0x32, 0xe0);
	tp28xx_byte_write(chip, 0x33, 0x60);
}
static void TP2825B_A1080P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x02);
	tmp |= 0x04;
	tp28xx_byte_write(chip, 0x02, tmp);

	tp28xx_byte_write(chip, 0x15, 0x01);
	tp28xx_byte_write(chip, 0x16, 0xf0);

	tp28xx_byte_write(chip, 0x0d, 0x73);

	tp28xx_byte_write(chip, 0x20, 0x3c);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x54);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0xa5);
	tp28xx_byte_write(chip, 0x31, 0x86);
	tp28xx_byte_write(chip, 0x32, 0xfb);
	tp28xx_byte_write(chip, 0x33, 0x60);
}
static void TP2825B_1080P60_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xe2);
	tp28xx_byte_write(chip, 0x17, 0x80);
	tp28xx_byte_write(chip, 0x18, 0x27);
	tp28xx_byte_write(chip, 0x19, 0x38);
	tp28xx_byte_write(chip, 0x1a, 0x47);
	tp28xx_byte_write(chip, 0x1c, 0x08);
	tp28xx_byte_write(chip, 0x1d, 0x96);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x40);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0x9b);
	tp28xx_byte_write(chip, 0x32, 0xa5);
	tp28xx_byte_write(chip, 0x33, 0xe0);

	//tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
//HDC 8M15
static void TP2825B_C8MP15_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x40);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x38);

	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);

	tp28xx_byte_write(chip, 0x2d, 0x84);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x31);

	tp28xx_byte_write(chip, 0x39, 0x48);
}

/////HDC 8M12
static void TP2825B_C8MP12_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x40);
	tp28xx_byte_write(chip, 0x15, 0x23);
	tp28xx_byte_write(chip, 0x16, 0xf8);
	tp28xx_byte_write(chip, 0x18, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x68);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x84);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x32);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);
}
/////HDA 8M15
static void TP2825B_A8MP15_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0x74);
	//tp28xx_byte_write(chip, 0x18, 0x32);

	tp28xx_byte_write(chip, 0x20, 0x50);
	//tp28xx_byte_write(chip, 0x21, 0x86);
	//tp28xx_byte_write(chip, 0x22, 0x36);

	//tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2d, 0x58);
	tp28xx_byte_write(chip, 0x2e, 0x48);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0x68);
	tp28xx_byte_write(chip, 0x32, 0x43);
	tp28xx_byte_write(chip, 0x33, 0x00);
	//tp28xx_byte_write(chip, 0x35, 0x15);
	tp28xx_byte_write(chip, 0x39, 0x40);
}
static void TP2825B_1080P50_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xe2);
	tp28xx_byte_write(chip, 0x17, 0x80);
	tp28xx_byte_write(chip, 0x18, 0x27);
	tp28xx_byte_write(chip, 0x19, 0x38);
	tp28xx_byte_write(chip, 0x1a, 0x47);
	tp28xx_byte_write(chip, 0x1c, 0x0A);
	tp28xx_byte_write(chip, 0x1d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x40);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0x9b);
	tp28xx_byte_write(chip, 0x32, 0xa5);
	tp28xx_byte_write(chip, 0x33, 0xe0);

	//tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
//HDC 5M20
static void TP2825B_C5MP20_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x40);

	tp28xx_byte_write(chip, 0x15, 0x33);
	tp28xx_byte_write(chip, 0x16, 0x9c);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x38);

	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);

	tp28xx_byte_write(chip, 0x2d, 0x74);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x30);

	tp28xx_byte_write(chip, 0x39, 0x48);
}
//HDC 6M20
static void TP2825B_C6MP20_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	tp28xx_byte_write(chip, 0x13, 0x40);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x38);

	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);

	tp28xx_byte_write(chip, 0x2d, 0x74);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x30);

	tp28xx_byte_write(chip, 0x39, 0x48);
}
///////////////////////////////////////////////////////////////////
static void TP2860_A720P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x60;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xfe);
	tp28xx_byte_write(chip, 0x1c, 0x86);
	tp28xx_byte_write(chip, 0x1d, 0x70);

	tp28xx_byte_write(chip, 0x0d, 0x70);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x01);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x48);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x4e);
	tp28xx_byte_write(chip, 0x31, 0xe5);
	tp28xx_byte_write(chip, 0x32, 0x00);
	tp28xx_byte_write(chip, 0x33, 0xf0);
	tp28xx_byte_write(chip, 0x35, 0x25);
}
static void TP2860_A720P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x60;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xfe);
	tp28xx_byte_write(chip, 0x1c, 0x87);
	tp28xx_byte_write(chip, 0x1d, 0xba);

	tp28xx_byte_write(chip, 0x0d, 0x70);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x01);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x48);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x4f);
	tp28xx_byte_write(chip, 0x31, 0x10);
	tp28xx_byte_write(chip, 0x32, 0x08);
	tp28xx_byte_write(chip, 0x33, 0x40);
	tp28xx_byte_write(chip, 0x35, 0x25);
}
static void TP2860_A1080P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x60;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xd0);
	tp28xx_byte_write(chip, 0x1c, 0x88);
	tp28xx_byte_write(chip, 0x1d, 0x96);

	tp28xx_byte_write(chip, 0x0d, 0x70);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x48);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x52);
	tp28xx_byte_write(chip, 0x31, 0xca);
	tp28xx_byte_write(chip, 0x32, 0xf0);
	tp28xx_byte_write(chip, 0x33, 0x20);
	tp28xx_byte_write(chip, 0x35, 0x25);
}
static void TP2860_A1080P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);
	tp28xx_byte_write(chip, 0x13, 0x00);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xd0);
	tp28xx_byte_write(chip, 0x1c, 0x8a);
	tp28xx_byte_write(chip, 0x1d, 0x4e);

	tp28xx_byte_write(chip, 0x0d, 0x70);

	tp28xx_byte_write(chip, 0x20, 0x3c);
	tp28xx_byte_write(chip, 0x21, 0x46);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);
	tp28xx_byte_write(chip, 0x27, 0xad);

	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x48);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0x52);
	tp28xx_byte_write(chip, 0x31, 0xc3);
	tp28xx_byte_write(chip, 0x32, 0x7d);
	tp28xx_byte_write(chip, 0x33, 0xa0);
	tp28xx_byte_write(chip, 0x35, 0x25);
}
static void TP2860_960P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0x16);
	tp28xx_byte_write(chip, 0x17, 0x00);
	tp28xx_byte_write(chip, 0x18, 0xa0);
	tp28xx_byte_write(chip, 0x19, 0xc0);
	tp28xx_byte_write(chip, 0x1a, 0x35);
	tp28xx_byte_write(chip, 0x1c, 0x06);
	tp28xx_byte_write(chip, 0x1d, 0x72);

	tp28xx_byte_write(chip, 0x20, 0x30);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x01);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x30);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x43);
	tp28xx_byte_write(chip, 0x31, 0x3b);
	tp28xx_byte_write(chip, 0x32, 0x79);
	tp28xx_byte_write(chip, 0x33, 0x90);

	tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x18);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
static void TP2860_960P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0x16);
	tp28xx_byte_write(chip, 0x17, 0x00);
	tp28xx_byte_write(chip, 0x18, 0xa0);
	tp28xx_byte_write(chip, 0x19, 0xc0);
	tp28xx_byte_write(chip, 0x1a, 0x35);
	tp28xx_byte_write(chip, 0x1c, 0x07);
	tp28xx_byte_write(chip, 0x1d, 0xbc);

	tp28xx_byte_write(chip, 0x20, 0x30);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x30);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0xba);
	tp28xx_byte_write(chip, 0x32, 0x2e);
	tp28xx_byte_write(chip, 0x33, 0x90);

	tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x18);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}

/////TVI 5M20V2
static void TP2831_5MP20V2_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x23);
	tp28xx_byte_write(chip, 0x16, 0x14);
	tp28xx_byte_write(chip, 0x17, 0x90);
	tp28xx_byte_write(chip, 0x18, 0x43);
	tp28xx_byte_write(chip, 0x19, 0x7c);
	tp28xx_byte_write(chip, 0x1a, 0x6b);
	tp28xx_byte_write(chip, 0x1c, 0x10);
	tp28xx_byte_write(chip, 0x1d, 0x96);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x58);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0xa7);
	tp28xx_byte_write(chip, 0x32, 0x18);
	tp28xx_byte_write(chip, 0x33, 0x50);

	tp28xx_byte_write(chip, 0x35, 0x16);
	tp28xx_byte_write(chip, 0x36, 0xd4);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x68);
	tp28xx_byte_write(chip, 0x3a, 0x13);
	tp28xx_byte_write(chip, 0x3b, 0x25);

	tp28xx_byte_write(chip, 0x80, 0x52);
	tp28xx_byte_write(chip, 0x81, 0x18);
	tp28xx_byte_write(chip, 0x82, 0x40);
	tp28xx_byte_write(chip, 0x83, 0xcc);
	tp28xx_byte_write(chip, 0x84, 0x28);
	tp28xx_byte_write(chip, 0x85, 0x0e);
	tp28xx_byte_write(chip, 0x86, 0x1f);
	tp28xx_byte_write(chip, 0x87, 0x4a);
	tp28xx_byte_write(chip, 0x88, 0x58);

}
#if 0
/////TVI 8M7.5
static void TP2831_A8MP7_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp |= 0x40;
	tp28xx_byte_write(chip, 0x14, tmp);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xc0);
	tp28xx_byte_write(chip, 0x17, 0x00);
	tp28xx_byte_write(chip, 0x18, 0x50);
	tp28xx_byte_write(chip, 0x19, 0x70);
	tp28xx_byte_write(chip, 0x1a, 0x8f);
	tp28xx_byte_write(chip, 0x1c, 0x0f);
	tp28xx_byte_write(chip, 0x1d, 0xa0);


	tp28xx_byte_write(chip, 0x20, 0x30);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x38);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x24);
	tp28xx_byte_write(chip, 0x31, 0x34);
	tp28xx_byte_write(chip, 0x32, 0x21);
	tp28xx_byte_write(chip, 0x33, 0x80);

	tp28xx_byte_write(chip, 0x35, 0x19);
	tp28xx_byte_write(chip, 0x36, 0xab);

	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x1c);
	tp28xx_byte_write(chip, 0x3a, 0x32);
	tp28xx_byte_write(chip, 0x3b, 0x25);

}

//HDC 5M25
static void TP2831_C5MP25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x13, 0x40);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);


	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x23);
	tp28xx_byte_write(chip, 0x16, 0x4a);
	tp28xx_byte_write(chip, 0x17, 0x40);
	tp28xx_byte_write(chip, 0x18, 0x19);
	tp28xx_byte_write(chip, 0x19, 0x54);
	tp28xx_byte_write(chip, 0x1a, 0x6b);
	tp28xx_byte_write(chip, 0x1c, 0x0e);
	tp28xx_byte_write(chip, 0x1d, 0x0e);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x86);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xda);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x50);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x75);
	tp28xx_byte_write(chip, 0x31, 0x39);
	tp28xx_byte_write(chip, 0x32, 0xc0);
	tp28xx_byte_write(chip, 0x33, 0x30);

	tp28xx_byte_write(chip, 0x35, 0x16);
	tp28xx_byte_write(chip, 0x36, 0x72);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x48);

}
#endif

/////TVI 5M12V2
static void TP2831_5MP12V2_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0x6e);
	tp28xx_byte_write(chip, 0x17, 0x90);
	tp28xx_byte_write(chip, 0x18, 0x42);
	tp28xx_byte_write(chip, 0x19, 0x80);
	tp28xx_byte_write(chip, 0x1a, 0x6b);
	tp28xx_byte_write(chip, 0x1c, 0x0d);
	tp28xx_byte_write(chip, 0x1d, 0x48);

	tp28xx_byte_write(chip, 0x20, 0x40);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x34);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x48);
	tp28xx_byte_write(chip, 0x31, 0xba);
	tp28xx_byte_write(chip, 0x32, 0x2e);
	tp28xx_byte_write(chip, 0x33, 0x90);

	tp28xx_byte_write(chip, 0x35, 0x16);
	tp28xx_byte_write(chip, 0x36, 0xd3);

	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x1c);
	tp28xx_byte_write(chip, 0x3a, 0x32);
	tp28xx_byte_write(chip, 0x3b, 0x25);;

}
/////TVI 8M15V2
static void TP2831_8MP15V2_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x13);
	tp28xx_byte_write(chip, 0x16, 0xbd);
	tp28xx_byte_write(chip, 0x17, 0xc0);
	tp28xx_byte_write(chip, 0x18, 0x14);
	tp28xx_byte_write(chip, 0x19, 0x90);
	tp28xx_byte_write(chip, 0x1a, 0x9c);
	tp28xx_byte_write(chip, 0x1c, 0x0f);
	tp28xx_byte_write(chip, 0x1d, 0x76);

	tp28xx_byte_write(chip, 0x20, 0x50);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xff);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x0a);
	tp28xx_byte_write(chip, 0x2d, 0x54);
	tp28xx_byte_write(chip, 0x2e, 0x70);

	tp28xx_byte_write(chip, 0x30, 0x74);
	tp28xx_byte_write(chip, 0x31, 0x59);
	tp28xx_byte_write(chip, 0x32, 0xbd);
	tp28xx_byte_write(chip, 0x33, 0x60);

	tp28xx_byte_write(chip, 0x35, 0x19);
	tp28xx_byte_write(chip, 0x36, 0xc4);

	tp28xx_byte_write(chip, 0x38, 0x40);
	tp28xx_byte_write(chip, 0x39, 0x68);
	tp28xx_byte_write(chip, 0x3a, 0x13);
	tp28xx_byte_write(chip, 0x3b, 0x25);
}

static void TP2860_A960P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x50);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xe6);
	tp28xx_byte_write(chip, 0x17, 0x00);
	tp28xx_byte_write(chip, 0x18, 0x89);
	tp28xx_byte_write(chip, 0x19, 0xc0);
	tp28xx_byte_write(chip, 0x1a, 0x35);
	tp28xx_byte_write(chip, 0x1c, 0x8a);
	tp28xx_byte_write(chip, 0x1d, 0x8a);

	tp28xx_byte_write(chip, 0x20, 0x40);
	tp28xx_byte_write(chip, 0x21, 0x46);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x05);
	tp28xx_byte_write(chip, 0x27, 0xad);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x50);
	tp28xx_byte_write(chip, 0x2e, 0x58);

	tp28xx_byte_write(chip, 0x30, 0x52);
	tp28xx_byte_write(chip, 0x31, 0xd2);
	tp28xx_byte_write(chip, 0x32, 0x1c);
	tp28xx_byte_write(chip, 0x33, 0x00);

	tp28xx_byte_write(chip, 0x35, 0x34);
	tp28xx_byte_write(chip, 0x36, 0x4c);

	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x18);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
static void TP2860_A960P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x05, 0x01);
	tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x76);
	tp28xx_byte_write(chip, 0x0e, 0x12);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0x63);
	tp28xx_byte_write(chip, 0x17, 0x00);
	tp28xx_byte_write(chip, 0x18, 0x9a);
	tp28xx_byte_write(chip, 0x19, 0xc0);
	tp28xx_byte_write(chip, 0x1a, 0x35);
	tp28xx_byte_write(chip, 0x1c, 0x85);
	tp28xx_byte_write(chip, 0x1d, 0x78);

	tp28xx_byte_write(chip, 0x20, 0x14);
	tp28xx_byte_write(chip, 0x21, 0x46);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x3a);
	tp28xx_byte_write(chip, 0x2d, 0x1e);
	tp28xx_byte_write(chip, 0x2e, 0x50);

	tp28xx_byte_write(chip, 0x30, 0x29);
	tp28xx_byte_write(chip, 0x31, 0x01);
	tp28xx_byte_write(chip, 0x32, 0x76);
	tp28xx_byte_write(chip, 0x33, 0x80);

	tp28xx_byte_write(chip, 0x35, 0x14);
	tp28xx_byte_write(chip, 0x36, 0x65);

	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x88);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
}
#if 0
static void TP2860_144_A1080P25_DataSet(unsigned char chip)
{
	unsigned char tmp;

	//tp28xx_byte_write(chip, 0x07, 0xc0);
	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x73);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xf0);
	tp28xx_byte_write(chip, 0x17, 0x80);
	tp28xx_byte_write(chip, 0x18, 0x2a);
	tp28xx_byte_write(chip, 0x19, 0x38);
	tp28xx_byte_write(chip, 0x1a, 0x47);
	tp28xx_byte_write(chip, 0x1c, 0x0a);
	tp28xx_byte_write(chip, 0x1d, 0x50);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x30);
	tp28xx_byte_write(chip, 0x2d, 0x48);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0xaa);
	tp28xx_byte_write(chip, 0x31, 0xb3);
	tp28xx_byte_write(chip, 0x32, 0x33);
	tp28xx_byte_write(chip, 0x33, 0x30);

	tp28xx_byte_write(chip, 0x35, 0x05);
	tp28xx_byte_write(chip, 0x36, 0x4c);

	tp28xx_byte_write(chip, 0x38, 0x00);
	tp28xx_byte_write(chip, 0x39, 0x8c);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
	tmp = tp28xx_byte_read(chip, 0x02);
	tmp |= 0x04;
	tp28xx_byte_write(chip, 0x02, tmp);
}

static void TP2860_144_A1080P30_DataSet(unsigned char chip)
{
	unsigned char tmp;

	tp28xx_byte_write(chip, 0x0b, 0xc0);
	tp28xx_byte_write(chip, 0x0c, 0x03);
	tp28xx_byte_write(chip, 0x0d, 0x72);

	tp28xx_byte_write(chip, 0x15, 0x03);
	tp28xx_byte_write(chip, 0x16, 0xf0);
	tp28xx_byte_write(chip, 0x17, 0x80);
	tp28xx_byte_write(chip, 0x18, 0x2a);
	tp28xx_byte_write(chip, 0x19, 0x38);
	tp28xx_byte_write(chip, 0x1a, 0x47);
	tp28xx_byte_write(chip, 0x1c, 0x08);
	tp28xx_byte_write(chip, 0x1d, 0x98);

	tp28xx_byte_write(chip, 0x20, 0x38);
	tp28xx_byte_write(chip, 0x21, 0x84);
	tp28xx_byte_write(chip, 0x22, 0x36);
	tp28xx_byte_write(chip, 0x23, 0x3c);

	tp28xx_byte_write(chip, 0x25, 0xfe);
	tp28xx_byte_write(chip, 0x26, 0x0d);
	tp28xx_byte_write(chip, 0x27, 0x2d);
	tp28xx_byte_write(chip, 0x28, 0x00);

	tp28xx_byte_write(chip, 0x2b, 0x60);
	tp28xx_byte_write(chip, 0x2c, 0x30);
	tp28xx_byte_write(chip, 0x2d, 0x48);
	tp28xx_byte_write(chip, 0x2e, 0x40);

	tp28xx_byte_write(chip, 0x30, 0xaa);
	tp28xx_byte_write(chip, 0x31, 0xc2);
	tp28xx_byte_write(chip, 0x32, 0x9f);
	tp28xx_byte_write(chip, 0x33, 0x60);

	tp28xx_byte_write(chip, 0x35, 0x05);

	tp28xx_byte_write(chip, 0x39, 0x8c);
	tp28xx_byte_write(chip, 0x3a, 0x12);
	tp28xx_byte_write(chip, 0x3b, 0x26);

	tp28xx_byte_write(chip, 0x13, 0x00);
	tmp = tp28xx_byte_read(chip, 0x14);
	tmp &= 0x9f;
	tp28xx_byte_write(chip, 0x14, tmp);
	tmp = tp28xx_byte_read(chip, 0x02);
	tmp |= 0x04;
	tp28xx_byte_write(chip, 0x02, tmp);
}
#endif
