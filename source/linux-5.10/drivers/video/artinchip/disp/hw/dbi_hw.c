// SPDX-License-Identifier: GPL-2.0-only
#include "dbi_reg.h"
#include "reg_util.h"

void i8080_cmd_ctl(void __iomem *base, u32 first_line, u32 other_line)
{
	reg_set_bits(base + I8080_COMMAND_CTL,
			FIRST_LINE_COMMAND_MASK,
			FIRST_LINE_COMMAND(first_line));
	reg_set_bit(base + I8080_COMMAND_CTL, FIRST_LINE_COMMAND_CTL);

	reg_set_bits(base + I8080_COMMAND_CTL,
			OTHER_LINE_COMMAND_MASK,
			OTHER_LINE_COMMAND(other_line));
	reg_set_bit(base + I8080_COMMAND_CTL, OTHER_LINE_COMMAND_CTL);
}

void i8080_wr_cmd(void __iomem *base, u32 cmd)
{
	reg_write(base + I8080_WR_CMD, cmd);
}

void i8080_wr_data(void __iomem *base, u32 data)
{
	reg_write(base + I8080_WR_DATA, data);
}

void i8080_wr_ctl(void __iomem *base, u32 count, u32 start)
{
	reg_write(base + I8080_WR_CTL, count << 8 | start);
}

void i8080_rd_ctl(void __iomem *base, u32 count, u32 start)
{
	reg_write(base + I8080_RD_CTL, count << 8 | start);
}

u32 i8080_rd_data(void __iomem *base)
{
	return reg_read(base + I8080_RD_DATA);
}

u32 i8080_rd_fifo_depth(void __iomem *base)
{
	return reg_rd_bits(base + I8080_FIFO_DEPTH,
			DBI_I8080_RD_FIFO_DEPTH_MASK,
			DBI_I8080_RD_FIFO_DEPTH_SHIFT);
}

u32 i8080_wr_fifo_depth(void __iomem *base)
{
	return reg_rd_bits(base + I8080_FIFO_DEPTH,
			DBI_I8080_WR_FIFO_DEPTH_MASK,
			DBI_I8080_WR_FIFO_DEPTH_SHIFT);
}

void i8080_rd_fifo_flush(void __iomem *base)
{
	reg_set_bit(base + I8080_FIFO_DEPTH, DBI_I8080_RD_FIFO_FLUSH);
}

void i8080_wr_fifo_flush(void __iomem *base)
{
	reg_set_bit(base + I8080_FIFO_DEPTH, DBI_I8080_RD_FIFO_FLUSH);
}

void i8080_cmd_wr(void __iomem *base, u32 code, u32 count, const u8 *data)
{
	int i;

	i8080_wr_cmd(base, code);

	for (i = 0; i < count; i++)
		i8080_wr_data(base, *(data + i));

	i8080_wr_ctl(base, count, 1);

	while (!reg_rd_bit(base + I8080_STATUS, DBI_I8080_TX_FIFO_EMPTY,
		DBI_I8080_TX_FIFO_EMPTY_SHIFT))
		;
}

void qspi_code_cfg(void __iomem *base, u32 code1, u32 code2, u32 code3)
{
	reg_write(base + QSPI_CODE, code1 << 16 | code2 << 8 | code3);
}

void qspi_mode_cfg(void __iomem *base, u32 code1_cfg, u32 vbp_num, u32 qspi_mode)
{
	reg_set_bits(base + QSPI_MODE,
			CODE1_CFG_MASK,
			CODE1_CFG(code1_cfg));

	reg_set_bits(base + QSPI_MODE,
			VBP_NUM_MASK,
			VBP_NUM(vbp_num));
	if (qspi_mode)
		reg_set_bit(base + QSPI_MODE, QSPI_MODE_MASK);
	else
		reg_clr_bit(base + QSPI_MODE, QSPI_MODE_MASK);
}

void spi_cmd_ctl(void __iomem *base, u32 first_line, u32 other_line)
{
	reg_set_bits(base + SPI_COMMAND_CTL,
			FIRST_LINE_COMMAND_MASK,
			FIRST_LINE_COMMAND(first_line));
	reg_set_bit(base + SPI_COMMAND_CTL, FIRST_LINE_COMMAND_CTL);

	reg_set_bits(base + SPI_COMMAND_CTL,
			OTHER_LINE_COMMAND_MASK,
			OTHER_LINE_COMMAND(other_line));
	reg_set_bit(base + SPI_COMMAND_CTL, OTHER_LINE_COMMAND_CTL);
}

void spi_scl_cfg(void __iomem *base, u32 phase, u32 pol)
{
	reg_clr_bit(base + SPI_SCL_CFG, SCL_PHASE_CFG);

	if (phase)
		reg_set_bit(base + SPI_SCL_CFG, SCL_PHASE_CFG);
	else
		reg_clr_bit(base + SPI_SCL_CFG, SCL_PHASE_CFG);

	if (pol)
		reg_set_bit(base + SPI_SCL_CFG, SCL_POL);
	else
		reg_clr_bit(base + SPI_SCL_CFG, SCL_POL);
}

void spi_wr_cmd(void __iomem *base, u32 cmd)
{
	reg_write(base + SPI_WR_CMD, cmd);
}

void spi_wr_data(void __iomem *base, u32 data)
{
	reg_write(base + SPI_WR_DATA, data);
}

void spi_wr_ctl(void __iomem *base, u32 count, u32 start)
{
	reg_write(base + SPI_WR_CTL, count << 8 | start);
}

void spi_rd_ctl(void __iomem *base, u32 count, u32 start)
{
	reg_write(base + SPI_RD_CTL, count << 8 | start);
}

u32 spi_rd_data(void __iomem *base)
{
	return reg_read(base + SPI_RD_DATA);
}

u32 spi_rd_fifo_depth(void __iomem *base)
{
	return reg_rd_bits(base + SPI_FIFO_DEPTH,
			DBI_SPI_RD_FIFO_DEPTH_MASK,
			DBI_SPI_RD_FIFO_DEPTH_SHIFT);
}

u32 spi_wr_fifo_depth(void __iomem *base)
{
	return reg_rd_bits(base + SPI_FIFO_DEPTH,
			DBI_SPI_WR_FIFO_DEPTH_MASK,
			DBI_SPI_WR_FIFO_DEPTH_SHIFT);
}

void spi_rd_fifo_flush(void __iomem *base)
{
	reg_set_bit(base + SPI_FIFO_DEPTH, DBI_SPI_RD_FIFO_FLUSH);
}

void spi_wr_fifo_flush(void __iomem *base)
{
	reg_set_bit(base + SPI_FIFO_DEPTH, DBI_SPI_WR_FIFO_FLUSH);
}

void spi_cmd_wr(void __iomem *base, u32 code, u32 count, const u8 *data)
{
	int i;

	spi_wr_cmd(base, code);

	for (i = 0; i < count; i++)
		spi_wr_data(base, *(data + i));

	spi_wr_ctl(base, count, 1);

	while (!reg_rd_bit(base + SPI_STATUS, DBI_SPI_TX_FIFO_EMPTY,
		DBI_SPI_TX_FIFO_EMPTY_SHIFT))
		;
}
