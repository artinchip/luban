#define LOG_TAG         "Core"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
//#include "cts_sfctrl.h"
//#include "cts_spi_flash.h"
#include "cts_firmware.h"
#include "icnt8xxx_flash.h"

extern struct cts_firmware cts_driver_builtin_firmwares[];

static struct cts_device_hwdata cts_device_hwdatas[] = {
    {
        .name = "ICNT89xx",
        .hwid = CTS_HWID_ICNT89XX,
        .fwid = CTS_FWID_ICNT89XX,
        .num_row = 27,
        .num_col = 32,
        .sram_size = 45* 1024,
        .program_addr_width = 3,
        .ver_offset = 0x114,
    },
    {
        .name = "ICNT86xx",
        .hwid = CTS_HWID_ICNT86XX,
        .fwid = CTS_FWID_ICNT86XX,
        .num_row = 42,
        .num_col = 30,
        .sram_size = 45 * 1024,
        .program_addr_width = 3,
        .ver_offset = 0x100,
    },
    {
        .name = "ICNT88xx",
        .hwid = CTS_HWID_ICNT88XX,
        .fwid = CTS_FWID_ICNT88XX,
        .num_row = 42,
        .num_col = 30,
        .sram_size = 45 * 1024,
        .program_addr_width = 3,
        .ver_offset = 0x100,
    },
    {
        .name = "ICNT87xx",
        .hwid = CTS_HWID_ICNT87XX,
        .fwid = CTS_FWID_ICNT87XX,
        .num_row = 23,
        .num_col = 12,
        .sram_size = 32 * 1024,
        .program_addr_width = 2,
        .ver_offset = 0x100,
    },
    {
        .name = "ICNT85xx",
        .hwid = CTS_HWID_ICNT85XX,
        .fwid = CTS_FWID_ICNT85XX,
        .num_row = 21,
        .num_col = 12,
        .sram_size = 40 * 1024,
        .program_addr_width = 3,
        .ver_offset = 0x100,
    },
    {
        .name = "ICNT82xx",
        .hwid = CTS_HWID_ICNT82XX,
        .fwid = CTS_FWID_ICNT82XX,
        .num_row = 21,
        .num_col = 12,
        .sram_size = 16 * 1024,
        .program_addr_width = 3,
        .ver_offset = 0x114,
    },
    {
        .name = "ICNT81xx",
        .hwid = CTS_HWID_ICNT81XX,
        .fwid = CTS_FWID_ICNT81XX,
        .num_row = 23,
        .num_col = 12,
        .sram_size = 32 * 1024,
        .program_addr_width = 2,
        .ver_offset = 0x100,
    },
};

static int cts_i2c_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8  buff[8];

    cts_dbg("Write to i2c_addr: 0x%02x reg: 0x%0*x val: 0x%02x",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr, b);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writeb invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
            buff, cts_dev->rtdata.addr_width + 1, retry ,delay);
}

static int cts_i2c_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8  buff[8];

    cts_dbg("Write to i2c_addr: 0x%02x reg: 0x%0*x val: 0x%04x",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr, w);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writew invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
}

static int cts_i2c_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8  buff[8];

    cts_dbg("Write to i2c_addr: 0x%02x reg: 0x%0*x val: 0x%08x",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr, l);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        cts_err("Writel invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le32(l, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
            buff, cts_dev->rtdata.addr_width + 4, retry, delay);
}

static int cts_i2c_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
	int ret = 0;
	u8 *data;
	size_t max_xfer_size;
	size_t payload_len;
	size_t xfer_len;

    cts_dbg("Write to i2c_addr: 0x%02x reg: 0x%0*x len: %zu",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    data = cts_plat_get_i2c_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, data);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, data);
        } else {
            cts_err("Writesb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        memcpy(data + cts_dev->rtdata.addr_width, src, payload_len);

        ret = cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
                data, xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c write failed %d", ret);
            return ret;
        }

        src  += payload_len;
        len  -= payload_len;
        addr += payload_len;
    }

    return 0;
}

static int cts_i2c_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    u8 addr_buf[4];

    cts_dbg("Readb from i2c_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readb invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_i2c_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
	int ret = 0;
	u8  addr_buf[4];
	u8  buff[2];

    cts_dbg("Readw from i2c_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readw invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0) {
        *w = get_unaligned_le16(buff);
    }

    return ret;
}

static int cts_i2c_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
	int ret = 0;
	u8  addr_buf[4];
	u8  buff[4];

    cts_dbg("Readl from i2c_addr: 0x%02x reg: 0x%0*x",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        cts_err("Readl invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0) {
        *l = get_unaligned_le32(buff);
    }

    return ret;
}

static int cts_i2c_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
	int ret = 0;
	u8 addr_buf[4];
	size_t max_xfer_size, xfer_len;

    cts_dbg("Readsb from i2c_addr: 0x%02x reg: 0x%0*x len: %zu",
        cts_dev->rtdata.i2c_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            cts_err("Readsb invalid address width %u",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.i2c_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay);
        if (ret) {
            cts_err("Platform i2c read failed %d", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
        addr += xfer_len;
    }
   // cts_dbg("readsb--cts_dev: 0x%p",cts_dev); 
    return 0;
}

int cts_prog_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len)
{
    if (!cts_dev->rtdata.program_mode) {
        cts_err("prog write tansfer not under program mode");
        return -ENODEV;
    }
    return cts_i2c_writesb(cts_dev, reg_addr, src, len, 1, 1);
}

int cts_prog_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len)
{
    if (!cts_dev->rtdata.program_mode) {
        cts_err("prog read tansfer not under program mode");
        return -ENODEV;
    }

    return cts_i2c_readsb(cts_dev, reg_addr, dst, len, 1, 1);
}

int  icn85xx_prog_i2c_txdata(const struct cts_device *cts_dev,
    u32 addr, u8 *txdata, size_t length)
{
    return cts_prog_writesb_retry(cts_dev,addr, txdata, length);
}

int  icn85xx_prog_i2c_rxdata(const struct cts_device *cts_dev,
    u32 addr, u8 *txdata, size_t length)
{
    return cts_prog_readsb_retry(cts_dev,addr, txdata, length);
}

int  icn87xx_prog_i2c_txdata( struct cts_device *cts_dev,
    u32 addr, u8 *txdata, size_t length)
{
	int ret = 0;
	//int addr_width = cts_dev->rtdata.addr_width;
	//cts_dev->rtdata.addr_width      = 2;
	ret = cts_prog_writesb_retry(cts_dev,addr, txdata, length);
	//cts_dev->rtdata.addr_width      = addr_width;
	return ret;
}

int  icn87xx_prog_i2c_rxdata( struct cts_device *cts_dev,
    u32 addr, u8 *txdata, size_t length)
{
	int ret = 0;
	// int addr_width = cts_dev->rtdata.addr_width;
	// cts_dev->rtdata.addr_width      = 2;
	ret = cts_prog_readsb_retry(cts_dev,addr, txdata, length);
	//cts_dev->rtdata.addr_width = addr_width;
	return ret;
}


static int cts_write_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    int i, ret;
    u8    buff[5];

    for (i = 0; i < len; i++) {
        put_unaligned_le32(addr, buff);
        buff[4] = *(u8 *)src;
        
        addr++;
        src++;

        ret = cts_i2c_writesb(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF, buff, 5, retry, delay);
        if (ret) {
            cts_err("Write rDEBUG_INTF len=5B failed %d",
                    ret);
            return ret;
        }
    }

    return 0;
}

int cts_sram_writeb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_writeb(cts_dev, addr, b, retry, delay);
    } else {
        return cts_write_sram_normal_mode(cts_dev, addr, &b, 1, retry, delay);
    }
}

int cts_sram_writew_retry(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8 buff[2];

    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_writew(cts_dev, addr, w, retry, delay);
    } else {
        put_unaligned_le16(w, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
    }
}

int cts_sram_writel_retry(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8 buff[4];

    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_writel(cts_dev, addr, l, retry, delay);
    } else {
        put_unaligned_le32(l, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
    }
}

int cts_sram_writesb_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_writesb(cts_dev, addr, src, len, retry, delay);
    } else {
        return cts_write_sram_normal_mode(cts_dev, addr, src, len, retry, delay);
    }
}

static int cts_read_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        ret = cts_i2c_writel(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF, addr, retry, delay);
        if (ret) {
            cts_err("Write addr to rDEBUG_INTF failed %d", ret);
            return ret;
        }

        ret = cts_i2c_readb(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF + 4, (u8 *)dst, retry, delay);
        if (ret) {
            cts_err("Read value from rDEBUG_INTF + 4 failed %d",
                ret);
            return ret;
        }

        addr++;
        dst++;
    }

    return 0;
}

int cts_sram_readb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_readb(cts_dev, addr, b, retry, delay);
    } else {
        return cts_read_sram_normal_mode(cts_dev, addr, b, 1, retry, delay);
    }
}

int cts_sram_readw_retry(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
	int ret = 0;
	u8 buff[2];

    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_readw(cts_dev, addr, w, retry, delay);
    } else {
        ret = cts_read_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
        if (ret) {
            cts_err("SRAM readw in normal mode failed %d", ret);
            return ret;
        }

        *w = get_unaligned_le16(buff);

        return 0;
    }
}

int cts_sram_readl_retry(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
	int ret = 0;
	u8 buff[4];

	if (cts_dev->rtdata.program_mode) {
		return cts_i2c_readl(cts_dev, addr, l, retry, delay);
	} else {
		ret = cts_read_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
		if (ret) {
			cts_err("SRAM readl in normal mode failed %d", ret);
			return ret;
		}

        *l = get_unaligned_le32(buff);

        return 0;
    }
}

int cts_sram_readsb_retry(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
    } else {
        return cts_read_sram_normal_mode(cts_dev, addr, dst, len, retry, delay);
    }
}

int cts_fw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writeb to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_i2c_writeb(cts_dev, reg_addr, b, retry, delay);
}

int cts_fw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writew to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_i2c_writew(cts_dev, reg_addr, w, retry, delay);
}

int cts_fw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writel to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_i2c_writel(cts_dev, reg_addr, l, retry, delay);
}

int cts_fw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Writesb to fw reg 0x%04x under program mode", reg_addr);
        return -ENODEV;
    }

    return cts_i2c_writesb(cts_dev, reg_addr, src, len, retry, delay);
}

int cts_fw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readb from fw reg under program mode");
        return -ENODEV;
    }

    return cts_i2c_readb(cts_dev, reg_addr, b, retry, delay);
}

int cts_fw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readw from fw reg under program mode");
        return -ENODEV;
    }

    return cts_i2c_readw(cts_dev, reg_addr, w, retry, delay);
}

int cts_fw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readl from fw reg under program mode");
        return -ENODEV;
    }

    return cts_i2c_readl(cts_dev, reg_addr, l, retry, delay);
}

int cts_fw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        cts_err("Readsb from fw reg under program mode");
        return -ENODEV;
    }

    return cts_i2c_readsb(cts_dev, reg_addr, dst, len, retry, delay);
}

int cts_hw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay)
{
    return cts_sram_writeb_retry(cts_dev, reg_addr, b, retry, delay);
}

int cts_hw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay)
{
    return cts_sram_writew_retry(cts_dev, reg_addr, w, retry, delay);
}

int cts_hw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay)
{
    return cts_sram_writel_retry(cts_dev, reg_addr, l, retry, delay);
}

int cts_hw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay)
{
    return cts_sram_writesb_retry(cts_dev, reg_addr, src, len, retry, delay);
}

int cts_hw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay)
{
    return cts_sram_readb_retry(cts_dev, reg_addr, b, retry, delay);
}

int cts_hw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay)
{
    return cts_sram_readw_retry(cts_dev, reg_addr, w, retry, delay);
}

int cts_hw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay)
{
    return cts_sram_readl_retry(cts_dev, reg_addr, l, retry, delay);
}

int cts_hw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay)
{
    return cts_sram_readsb_retry(cts_dev, reg_addr, dst, len, retry, delay);
}

static int cts_init_device_hwdata(struct cts_device *cts_dev,
        u16 hwid, u16 fwid)
{
    int i,ret;
    int flash_id = 0;

    cts_dbg("Init hardware data hwid: %04x fwid: %04x", hwid, fwid);

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (hwid == cts_device_hwdatas[i].hwid ||
            fwid == cts_device_hwdatas[i].fwid) {
            cts_dev->hwdata = &cts_device_hwdatas[i];
            goto init_ic_type;
        }
    }

    return -EINVAL;

init_ic_type:

    //cts_dev->confdata.prog_i2c_addr = CTS_PROGRAM_MODE_I2CADDR;

    cts_dev->confdata.firmware.name = cts_dev->hwdata->name;
    cts_dev->confdata.firmware.hwid = cts_dev->hwdata->hwid;
    cts_dev->confdata.firmware.fwid = cts_dev->hwdata->fwid;
    cts_dev->confdata.firmware.ver_offset = cts_dev->hwdata->ver_offset;

    cts_dev->confdata.hw_sensor_id = 0xff;
    cts_dev->confdata.fw_sensor_id  = 0xff;
    cts_dev->confdata.is_sensor_matched = 0xff;

    if(fwid == cts_dev->hwdata->fwid){
        cts_dev->rtdata.is_chip_empty = false;
#ifdef SUPPORT_SENSOR_ID
        ret = cts_get_sensor_id_info(cts_dev);
        if (ret) {
            cts_err("Get sensor id failed %d", ret);
            return ret;
        }
#endif 
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            return ret;
        }
    }else{
        cts_dev->rtdata.is_chip_empty = true;
    }
    
    if((strcmp(cts_dev->hwdata->name, "ICNT81xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT87xx") == 0)){
        flash_id = icn87xx_read_flashid(cts_dev);
    }else if(strcmp(cts_dev->hwdata->name, "ICNT82xx") == 0){
//
    }else if((strcmp(cts_dev->hwdata->name, "ICNT85xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT86xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT88xx") == 0)){
        flash_id = icn85xx_read_flashid(cts_dev);
//
    }else if(strcmp(cts_dev->hwdata->name, "ICNT89xx") == 0){
        flash_id = icn89xx_read_flashid(cts_dev);

    }

    if((MD25D40_ID1 == flash_id)||(MD25D40_ID2 == flash_id)
        ||(MD25D20_ID1 == flash_id)||(MD25D20_ID2 == flash_id)
        ||(GD25Q10_ID == flash_id)||(MX25L512E_ID == flash_id)
        ||(MD25D05_ID == flash_id)||(MD25D10_ID == flash_id)){
        cts_dev->hwdata->is_with_flash = true;
    }else{
        cts_dev->hwdata->is_with_flash = false;
    }

    if(fwid == cts_dev->hwdata->fwid){
        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
          cts_err("Enter normal mode failed %d", ret);
          return ret;
        }
    }
    cts_info("Init hardware data name: %s hwid: %04x fwid: %04x is_with_flash: %d", 
        cts_dev->hwdata->name, cts_dev->hwdata->hwid, 
        cts_dev->hwdata->fwid, cts_dev->hwdata->is_with_flash);
    return 0;
}

#if 0
static int cts_update_device_fwid(struct cts_device *cts_dev)
{
	int ret = 0;
	u8 temp;

    if(strcmp(cts_dev->hwdata->name, "ICNT86xx") == 0){
        ret = cts_fw_reg_readb_retry(cts_dev,
            CTS_DEVICE_FW_REG_CHIP_TYPE, &temp, 5, 1);
        if(ret){
            cts_err("update device fwid fail");
        }else{
            cts_dev->hwdata->fwid = temp;
            cts_info("update device cts_dev->hwdata->fwid to: 0x%x",temp);
        }
    }
    return 0;
}
#endif

void cts_lock_device(const struct cts_device *cts_dev)
{
    cts_dbg("*** Lock ***");

    rt_mutex_lock(&cts_dev->pdata->dev_lock);
}

void cts_unlock_device(const struct cts_device *cts_dev)
{
    cts_dbg("### Un-Lock ###");

    rt_mutex_unlock(&cts_dev->pdata->dev_lock);
}

int cts_set_work_mode(const struct cts_device *cts_dev, u8 mode)
{
    cts_info("Set work mode to %u", mode);

    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
}

int cts_get_work_mode(const struct cts_device *cts_dev, u8 *mode)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
}

int cts_get_firmware_version(const struct cts_device *cts_dev, u16 *version)
{
    int ret = cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_VERSION, version);

    if (ret) {
        *version = 0;
    } else {
        *version = be16_to_cpup(version);
    }

    return ret;
}

int cts_get_data_ready_flag(const struct cts_device *cts_dev, u8 *flag)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_DATA_READY, flag);
}

int cts_clr_data_ready_flag(const struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_DATA_READY, 0);
}

int cts_send_command(const struct cts_device *cts_dev, u8 cmd)
{
    cts_info("Send command 0x%02x", cmd);

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Send command %u while chip in program mode", cmd);
        return -ENODEV;
    }

    return cts_fw_reg_writeb_retry(cts_dev, CTS_DEVICE_FW_REG_CMD, cmd, 3, 0);
}

static int cts_get_touchinfo(const struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info)
{
    cts_dbg("Get touch info");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get touch info in program mode");
        return -ENODEV;
    }

    if (cts_dev->rtdata.suspended) {
        cts_warn("Get touch info while is suspended");
        return -ENODEV;
    }

    return cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO,
            touch_info, sizeof(*touch_info));
}

int cts_get_panel_param(const struct cts_device *cts_dev,
        void *param, size_t size)
{
    cts_info("Get panel parameter");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get panel parameter in program mode");
        return -ENODEV;
    }

    return cts_fw_reg_readsb(cts_dev,
            CTS_DEVICE_FW_REG_PANEL_PARAM, param, size);
}

int cts_set_panel_param(const struct cts_device *cts_dev,
        const void *param, size_t size)
{
    cts_info("Set panel parameter");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Set panel parameter in program mode");
        return -ENODEV;
    }
    return cts_fw_reg_writesb(cts_dev,
            CTS_DEVICE_FW_REG_PANEL_PARAM, param, size);
}

int cts_get_x_resolution(const struct cts_device *cts_dev, u16 *resolution)
{
    return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_X_RESOLUTION, resolution);
}

int cts_get_y_resolution(const struct cts_device *cts_dev, u16 *resolution)
{
    return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_Y_RESOLUTION, resolution);
}

int cts_get_num_rows(const struct cts_device *cts_dev, u8 *num_rows)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_NUM_TX, num_rows);
}

int cts_get_num_cols(const struct cts_device *cts_dev, u8 *num_cols)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_NUM_RX, num_cols);
}

int cts_update_hw_rows(const struct cts_device *cts_dev, u8 *hw_rows)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_HW_REG_NUM_TX, hw_rows);
}
int cts_update_hw_cols(const struct cts_device *cts_dev, u8 *hw_cols)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_HW_REG_NUM_RX, hw_cols);
}

int cts_get_para_tx_order(struct cts_device *cts_dev, u8 *tx_order)
{
    return cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TX_ORDER, tx_order, 27);
}

int cts_get_para_rx_order(struct cts_device *cts_dev, u8 *rx_order)
{
    return cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_RX_ORDER, rx_order, 36);
}

#if (defined CONFIG_CTS_LEGACY_TOOL) || (defined CONFIG_CTS_SYSFS)
int cts_enable_get_rawdata(const struct cts_device *cts_dev)
{
    cts_info("Enable get raw/diff data");
    return cts_send_command(cts_dev, CTS_CMD_ENABLE_READ_RAWDATA);
}

int cts_disable_get_rawdata(const struct cts_device *cts_dev)
{
    cts_info("Disable get raw/diff data");
    return cts_send_command(cts_dev, CTS_CMD_DISABLE_READ_RAWDATA);
}

static int cts_get_scan_data(const struct cts_device *cts_dev,
        u32 addr, u8 n_col, void *buf, u8 is_diff)
{
    int i, ret;
    int row;
    //int k;
    //s16 *ptr;

    cts_info("addr:%d, n_col:%d, fwdata.rows:%d,fwdata.cols:%d",addr,n_col,cts_dev->fwdata.rows,cts_dev->fwdata.cols);
    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        u8 ready;

        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            cts_err("Get data ready flag failed %d", ret);
            return ret;
        }

        if (ready) {
            goto read_data;
        }
    }

    return -ENODEV;

read_data:
    for (row = 0; row < cts_dev->fwdata.rows; row++) {
        ret = cts_fw_reg_readsb(cts_dev, addr, buf, cts_dev->fwdata.cols * 2);
        if (ret) {
            cts_err("Read raw data failed %d", ret);
            return ret;
        }

        //ptr = buf;
        //for (k = 0; k < cts_dev->fwdata.cols; k++){
        //cts_info("buf[%d]:%x",k,ptr[k]);
        //}
        //cts_info("\n");
		if (is_diff == 0)
		{
        	addr += n_col * 2;
        	buf  += cts_dev->fwdata.cols * 2;
		}
		else 
		{
        	addr += (cts_dev->fwdata.cols +2 )* 2;
        	buf  += cts_dev->fwdata.cols * 2;
		}
    }

    ret = cts_clr_data_ready_flag(cts_dev);
    if (ret) {
        cts_err("Clear data ready flag failed %d", ret);
        return ret;
    }

    return 0;
}

int cts_get_selfcap_rawdata(const struct cts_device *cts_dev, void *buf)
{
    int i, ret;

    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        u8 ready;

        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            cts_err("Get data ready flag failed %d", ret);
            return ret;
        }

        if (ready) {
            goto read_data;
        }
    }

    return -ENODEV;

read_data:
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA_SELF_CAP_RX, 
        buf, cts_dev->fwdata.cols * 2);
    if (ret) {
        cts_err("Read raw data failed %d", ret);
        return ret;
    }

    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA_SELF_CAP_TX, 
        buf + cts_dev->fwdata.cols * 2, cts_dev->fwdata.rows * 2);
    if (ret) {
        cts_err("Read raw data failed %d", ret);
        return ret;
    }

    ret = cts_clr_data_ready_flag(cts_dev);
    if (ret) {
        cts_err("Clear data ready flag failed %d", ret);
        return ret;
    }

    return 0;
}    

int cts_get_selfcap_diffdata(const struct cts_device *cts_dev, void *buf)
{
    int i, ret;

    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        u8 ready;

        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            cts_err("Get data ready flag failed %d", ret);
            return ret;
        }

        if (ready) {
            goto read_data;
        }
    }

    return -ENODEV;

read_data:
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_DIFF_DATA_SELF_CAP, 
        buf, cts_dev->fwdata.cols * 2);
    if (ret) {
        cts_err("Read diff data failed %d", ret);
        return ret;
    }
    ret = cts_fw_reg_readsb(cts_dev, 
        CTS_DEVICE_FW_REG_DIFF_DATA_SELF_CAP+76, 
        buf + cts_dev->fwdata.cols * 2, 
        cts_dev->fwdata.rows * 2);
    if (ret) {
        cts_err("Read diff data failed %d", ret);
        return ret;
    }

    ret = cts_clr_data_ready_flag(cts_dev);
    if (ret) {
        cts_err("Clear data ready flag failed %d", ret);
        return ret;
    }

    return 0;
}

int cts_get_rawdata(const struct cts_device *cts_dev, void *buf)
{
    cts_info("Get raw data");

    return cts_get_scan_data(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA,
            cts_dev->hwdata->num_col, buf, 0);
}

int cts_get_diffdata(const struct cts_device *cts_dev, void *buf)
{
    u8 is_diff_89xx = 0;
	
    cts_info("Get diff data");
    if(strcmp(cts_dev->hwdata->name, "ICNT89xx") == 0){
		is_diff_89xx = 1;
    }
	if (is_diff_89xx == 0)
	{
    	return cts_get_scan_data(cts_dev,
            	CTS_DEVICE_FW_REG_DIFF_DATA + (cts_dev->hwdata->num_col + 2 + 1) * 2,
            	cts_dev->hwdata->num_col + 2, buf, is_diff_89xx);
	}
	else
	{
		return cts_get_scan_data(cts_dev,
				CTS_DEVICE_FW_REG_DIFF_DATA + (cts_dev->fwdata.cols + 2 + 1) * 2,
				cts_dev->hwdata->num_col + 2, buf, is_diff_89xx);
	}
}
#endif /* CONFIG_CTS_LEGACY_TOOL   CONFIG_CTS_SYSFS*/

static int cts_init_fwdata(struct cts_device *cts_dev)
{
	int ret = 0;

    cts_info("Init firmware data");

    if (cts_dev->rtdata.program_mode) {
        cts_err("Init firmware data while in program mode");
        return -EINVAL;
    }

    ret = cts_get_firmware_version(cts_dev, &cts_dev->fwdata.version);
    if (ret) {
        cts_err("Read firmware version failed %d", ret);
        return ret;
    }
    cts_info("  %-12s: %04x", "Version", cts_dev->fwdata.version);

    ret = cts_get_x_resolution(cts_dev, &cts_dev->fwdata.res_x);
    if (ret) {
        cts_err("Read firmware X resoltion failed %d", ret);
        return ret;
    }
    cts_info("  %-12s: %u", "X resolution", cts_dev->fwdata.res_x);

    ret = cts_get_y_resolution(cts_dev, &cts_dev->fwdata.res_y);
    if (ret) {
        cts_err("Read firmware Y resoltion failed %d", ret);
        return ret;
    }
    cts_info("  %-12s: %u", "Y resolution", cts_dev->fwdata.res_y);

    ret = cts_get_num_rows(cts_dev, &cts_dev->fwdata.rows);
    if (ret) {
        cts_err("Read firmware num TX failed %d", ret);
        return ret;
    }
    cts_info("  %-12s: %u", "Num rows", cts_dev->fwdata.rows);

    ret = cts_get_num_cols(cts_dev, &cts_dev->fwdata.cols);
    if (ret) {
        cts_err("Read firmware num RX failed %d", ret);
        return ret;
    }
    cts_info("  %-12s: %u", "Num cols", cts_dev->fwdata.cols);

    if(strcmp(cts_dev->hwdata->name, "ICNT89xx") == 0){
        ret = cts_update_hw_rows(cts_dev, &cts_dev->hwdata->num_row);
        if (ret) {
            cts_err("Update hw rows from firmware failed %d", ret);
            return ret;
        }
        cts_info("  %-12s: %u", "HW Num rows", cts_dev->hwdata->num_row);

        ret = cts_update_hw_cols(cts_dev, &cts_dev->hwdata->num_col);
        if (ret) {
            cts_err("Update hw cols from firmware failed %d", ret);
            return ret;
        }
        cts_info("  %-12s: %u", "HW Num cols", cts_dev->hwdata->num_col);
    }

    return 0;
}

int cts_irq_handler(struct cts_device *cts_dev)
{
	int ret = 0;

    cts_dbg("Enter IRQ handler");

    if (cts_dev->rtdata.program_mode) {
        cts_err("IRQ triggered in program mode");
        return -EINVAL;
    }

    if (unlikely(cts_dev->rtdata.suspended)) {
#ifdef CONFIG_CTS_GESTURE
        if (cts_dev->rtdata.gesture_wakeup_enabled) {
            struct cts_device_gesture_info gesture_info;

            ret = cts_get_gesture_info(cts_dev,
                    &gesture_info, CFG_CTS_GESTURE_REPORT_TRACE);
            if (ret) {
                cts_warn("Get gesture info failed %d", ret);
                //return ret;
            }

            /** - Issure another suspend with gesture wakeup command to device
             * after get gesture info.
             */
            cts_send_command(cts_dev, CTS_CMD_SUSPEND_WITH_GESTURE);

            ret = cts_plat_process_gesture_info(cts_dev->pdata, &gesture_info);
            if (ret) {
                cts_err("Process gesture info failed %d", ret);
                return ret;
            }
        } else {
            cts_warn("IRQ triggered while device suspended "
                    "without gesture wakeup enable");
        }
#endif /* CONFIG_CTS_GESTURE */
    } else {
        struct cts_device_touch_info touch_info;

        ret = cts_get_touchinfo(cts_dev, &touch_info);
        if (ret) {
            cts_err("Get touch info failed %d", ret);
            return ret;
        }
        cts_dbg("Touch info: vkey_state %x, num_msg %u",
            touch_info.vkey_state, touch_info.num_msg);

        ret = cts_plat_process_touch_msg(cts_dev->pdata,
            touch_info.msgs, touch_info.num_msg);
        if (ret) {
            cts_err("Process touch msg failed %d", ret);
            return ret;
        }

#ifdef CONFIG_CTS_VIRTUALKEY
        ret = cts_plat_process_vkey(cts_dev->pdata, touch_info.vkey_state);
        if (ret) {
            cts_err("Process vkey failed %d", ret);
            return ret;
        }
#endif /* CONFIG_CTS_VIRTUALKEY */
    }

    return 0;
}

bool cts_is_device_suspended(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.suspended;
}

int cts_suspend_device(struct cts_device *cts_dev)
{
	int ret = 0;

    cts_info("Suspend device");

    if (cts_dev->rtdata.suspended) {
        cts_warn("Suspend device while already suspended");
        return 0;
    }

    ret = cts_send_command(cts_dev,
        cts_dev->rtdata.gesture_wakeup_enabled ? 
            CTS_CMD_SUSPEND_WITH_GESTURE : CTS_CMD_SUSPEND);

    if (ret){    
        cts_err("Suspend device failed %d", ret);
       
        return ret;
    }

    cts_info("Device suspended ...");
    cts_dev->rtdata.suspended = true;

    return 0;
}

int cts_wakeup_device(struct cts_device *cts_dev)
{
    struct cts_platform_data *pdata = cts_dev->pdata;
    int ret=0;
    u8 prog_i2c_addr;
    //u8 wakeup_cmd[2] = {0x04, 0xFF};

    cts_plat_reset_device(pdata);
    
    #if 0 // used for i2c wakeup
    ret = cts_plat_i2c_write(pdata, CTS_NORMAL_MODE_I2CADDR, wakeup_cmd, sizeof(wakeup_cmd), 5, 2);
    if (ret) {
        cts_err("send i2c wakeup cmd fail");
    }
    #endif
    if(!cts_dev->hwdata->is_with_flash){
        prog_i2c_addr = cts_get_program_i2c_addr(cts_dev);

        cts_dev->rtdata.program_mode = true;
        cts_dev->rtdata.i2c_addr = prog_i2c_addr;
        cts_dev->rtdata.addr_width   = CTS_PROGRAM_MODE_I2C_ADDR_WIDTH;

        if(strcmp(cts_dev->hwdata->name, "ICNT89xx") == 0){

        }else if(strcmp(cts_dev->hwdata->name, "ICNT86xx") == 0){
            ret = icn85xx_bootfrom_sram(cts_dev);
            if(ret){
                cts_err("%s boot from sram fail", cts_dev->hwdata->name);
            }
        }else if(strcmp(cts_dev->hwdata->name, "ICNT87xx") == 0){
            cts_dev->rtdata.addr_width   = 2;
            if(icnt87_sram_crc != icn87xx_calculate_crc(cts_dev,icnt87_sram_length)){ // sram is broken
                cts_err("%s firmware in sram crc err !!!", cts_dev->hwdata->name);
                ret = -1;
            }else{
                ret = icn87xx_boot_sram(cts_dev);
                if(ret){
                    cts_err("%s boot from sram fail", cts_dev->hwdata->name);
                }
            }
        }
        cts_dev->rtdata.program_mode = false;
        cts_dev->rtdata.i2c_addr = CTS_NORMAL_MODE_I2CADDR;
        cts_dev->rtdata.addr_width   = 2;
        
        if(!ret){
            cts_info("%s  boot from sram ok",cts_dev->hwdata->name);
        }
    }
    return 0;

}

int cts_resume_device(struct cts_device *cts_dev)
{
    int ret=0;

    cts_info("Resume device");


    cts_wakeup_device(cts_dev);

    /* Check whether device is in normal mode */
    if (!cts_plat_is_i2c_online(cts_dev->pdata, CTS_NORMAL_MODE_I2CADDR)) {
        const struct cts_firmware *firmware;

        firmware = cts_request_firmware(cts_dev,cts_dev->hwdata->hwid,
                cts_dev->hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(cts_dev, firmware, true);
            cts_release_firmware(firmware);

            if (ret) {
                cts_err("Update default firmware failed %d", ret);
                goto err_set_program_mode;
            }
        } else {
            cts_err("Request default firmware failed %d, "
                    "please update manually!!", ret);

            goto err_set_program_mode;
        }
        
    }

#ifdef CONFIG_CTS_CHARGER_DETECT
	if (cts_is_charger_exist(cts_dev)) {
		cts_charger_plugin(cts_dev);
	}
#endif /* CONFIG_CTS_CHARGER_DETECT */
	
#ifdef CONFIG_CTS_GLOVE
	if (cts_is_glove_enabled(cts_dev)) {
		cts_enter_glove_mode(cts_dev);
	}	 
#endif

	cts_dev->rtdata.suspended = false;
	return 0;

err_set_program_mode:
	//    cts_dev->rtdata.program_mode = true;
	//    cts_dev->rtdata.i2c_addr     = cts_get_program_i2c_addr(cts_dev);//CTS_PROGRAM_MODE_I2CADDR;
	if((strcmp(cts_dev->hwdata->name, "ICNT87xx") == 0)
			||(strcmp(cts_dev->hwdata->name, "ICNT81xx") == 0)){
		cts_dev->rtdata.addr_width   = 2;
	}else{
		//        cts_dev->rtdata.addr_width   = CTS_PROGRAM_MODE_I2C_ADDR_WIDTH;
	}

    return ret;
}

bool cts_is_device_program_mode(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.program_mode;
}

static inline void cts_init_rtdata_with_normal_mode(struct cts_device *cts_dev)
{
    memset(&cts_dev->rtdata, 0, sizeof(cts_dev->rtdata));

    cts_dev->rtdata.i2c_addr        = CTS_NORMAL_MODE_I2CADDR;
    cts_dev->rtdata.addr_width      = 2;
    cts_dev->rtdata.program_mode    = false;
    cts_dev->rtdata.suspended       = false;
    cts_dev->rtdata.updating        = false;
    cts_dev->rtdata.testing         = false;
}

u8 cts_get_program_i2c_addr(struct cts_device *cts_dev)
{
    u8 prog_i2c_addr;
    if(cts_plat_is_i2c_online(cts_dev->pdata, CTS_PROGRAM_MODE_I2CADDR_2)){
        prog_i2c_addr =  CTS_PROGRAM_MODE_I2CADDR_2;
    }
    else if(cts_plat_is_i2c_online(cts_dev->pdata, CTS_PROGRAM_MODE_I2CADDR)){
        prog_i2c_addr =  CTS_PROGRAM_MODE_I2CADDR;
    }else{
        cts_err("!!! Prog mode i2c addr 0x58 and 0x30 is both offline, i2c transfer error !!!");
        cts_dev->confdata.prog_i2c_addr = CTS_PROGRAM_MODE_I2CADDR;
        return CTS_PROGRAM_MODE_I2CADDR;
    }
    cts_info("check chip progmode i2c addr is 0x%x", prog_i2c_addr);
    cts_dev->confdata.prog_i2c_addr = prog_i2c_addr;
    return prog_i2c_addr;
}

int cts_enter_program_mode(struct cts_device *cts_dev)
{
    const static u8 magic_num[] = {0xCC, 0x33, 0x55, 0x5A};
    //u8  boot_mode;
	u8 prog_i2c_addr = 0;
	int ret = 0;

    cts_info("Enter program mode");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Enter program mode while alredy in");
        return 0;
    }

    prog_i2c_addr = cts_get_program_i2c_addr(cts_dev);

    ret = cts_plat_i2c_write(cts_dev->pdata,
            prog_i2c_addr, magic_num, 4, 5, 10);
    if (ret) {
        cts_err("Write magic number to i2c_dev: 0x%02x failed %d",
            prog_i2c_addr, ret);
        //return ret;
        cts_dev->rtdata.i2c_addr     = CTS_PROGRAM_MODE_I2CADDR;
    }else{
        cts_dev->rtdata.i2c_addr     = prog_i2c_addr;
    }

    cts_dev->rtdata.program_mode = true;

    if(cts_dev->hwdata && cts_dev->hwdata->name 
        && ((strcmp(cts_dev->hwdata->name, "ICNT81xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT87xx") == 0))){
        cts_dev->rtdata.addr_width   = 2;
    }else{
         cts_dev->rtdata.addr_width   = CTS_PROGRAM_MODE_I2C_ADDR_WIDTH;
        }
#if 0
    /* Write i2c deglitch register */
    ret = cts_hw_reg_writeb_retry(cts_dev, 0x37001, 0x0F, 5, 1);
    if (ret) {
        cts_err("Write i2c deglitch register failed\n");
        // FALL through
    }

    ret = cts_hw_reg_readb_retry(cts_dev, 0x30010, &boot_mode, 5, 10);
    if (ret) {
        cts_err("Read rBOOT_MODE failed %d", ret);
        return ret;
    }

    if (boot_mode != 2) {
        cts_err("rBOOT_MODE readback %u != PROMGRAM_MODE", boot_mode);
        return -EFAULT;
    }
#endif
    return 0;
}

int cts_enter_normal_mode(struct cts_device *cts_dev)
{
    int ret = 0;
    //u8  boot_mode;

    cts_info("Enter normal mode");

    if (!cts_dev->rtdata.program_mode) {
        cts_warn("Enter normal mode while already in");
        return 0;
    }

    if(strcmp(cts_dev->hwdata->name, "ICNT81xx") == 0){
        ret = cts_hw_reg_writeb_retry(cts_dev, 0xf008, 0x7f, 5, 5); //boot from flash
    }
    else if(strcmp(cts_dev->hwdata->name, "ICNT87xx") == 0){
        ret = cts_hw_reg_writeb_retry(cts_dev, 0xf400, 0x03, 5, 5); //boot from sram
    }
    else if(strcmp(cts_dev->hwdata->name, "ICNT82xx") == 0){
        //
    }else if((strcmp(cts_dev->hwdata->name, "ICNT85xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT86xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT88xx") == 0)){
        ret = cts_hw_reg_writeb_retry(cts_dev, 0x40400, 0x03, 5, 5);//boot from sram
        
    }else if(strcmp(cts_dev->hwdata->name, "ICNT89xx") == 0){
        ret = cts_hw_reg_writeb_retry(cts_dev, 0x40010, 0x03, 5, 5);

    }
   // ret = cts_hw_reg_writeb_retry(cts_dev, 0x30010, 0x03, 5, 5);
    if (ret) {
        cts_err("Enter normal mode fail");
        //goto err_init_i2c_program_mode;
    } else {
        mdelay(30);
    }

    if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_NORMAL_MODE_I2CADDR)) {
        cts_dev->rtdata.program_mode = false;
        cts_dev->rtdata.i2c_addr     = CTS_NORMAL_MODE_I2CADDR;
        cts_dev->rtdata.addr_width   = 2;
    } else {
        goto err_init_i2c_normal_mode;
    }

#if 0
    ret = cts_init_fwdata(cts_dev);
    if (ret) {
        cts_err("Device init firmware data failed %d", ret);
        return ret;
    }
#endif

#ifdef SUPPORT_SENSOR_ID
    ret = cts_get_sensor_id_info(cts_dev);
    if (ret) {
        cts_err("Device init sensor id info failed %d", ret);
        return ret;
    }
#endif	
    return 0;

err_init_i2c_normal_mode:
    #if 0
    cts_dev->rtdata.program_mode = true;
    cts_dev->rtdata.i2c_addr     = cts_dev->confdata.prog_i2c_addr;
    //cts_dev->rtdata.i2c_addr     = CTS_PROGRAM_MODE_I2CADDR;
    //cts_dev->rtdata.addr_width   = CTS_PROGRAM_MODE_I2C_ADDR_WIDTH;
    if(cts_dev->hwdata->name 
        && ((strcmp(cts_dev->hwdata->name, "ICNT81xx") == 0)
        ||(strcmp(cts_dev->hwdata->name, "ICNT87xx") == 0))){
        cts_dev->rtdata.addr_width   = 2;
    }else{
        cts_dev->rtdata.addr_width   = CTS_PROGRAM_MODE_I2C_ADDR_WIDTH;
    }
    #endif
    cts_dev->rtdata.program_mode = false;
    cts_dev->rtdata.i2c_addr     = CTS_NORMAL_MODE_I2CADDR;
    cts_dev->rtdata.addr_width   = 2;

    return ret;
}

bool cts_is_device_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->enabled;
}

int cts_start_device(struct cts_device *cts_dev)
{
#ifdef CONFIG_CTS_ESD_PROTECTION
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
#endif /* CONFIG_CTS_ESD_PROTECTION */
	int ret = 0;

    cts_info("Start device...");

    if (cts_is_device_enabled(cts_dev)) {
        cts_warn("Start device while already started");
        return 0;
    }

#ifdef CONFIG_CTS_ESD_PROTECTION
    ret = cts_enable_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

    if ((ret = cts_plat_enable_irq(cts_dev->pdata)) < 0) {
        cts_err("Enable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = true;
    
    if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_NORMAL_MODE_I2CADDR)) {
        cts_info("Start device successfully");
    }else{
        cts_err("Start device fail.");
    }

    return 0;
}

int cts_stop_device(struct cts_device *cts_dev)
{
    struct chipone_ts_data *cts_data =
        container_of(cts_dev, struct chipone_ts_data, cts_dev);
	int ret = 0;

    cts_info("Stop device...");

    if (!cts_is_device_enabled(cts_dev)) {
        cts_warn("Stop device while halted");
        return 0;
    }

    if (cts_is_firmware_updating(cts_dev)) {
        cts_warn("Stop device while firmware updating, please try again");
        return -EAGAIN;
    }

    if ((ret = cts_plat_disable_irq(cts_dev->pdata)) < 0) {
        cts_err("Disable IRQ failed %d", ret);
        return ret;
    }

    cts_dev->enabled = false;

#ifdef CONFIG_CTS_ESD_PROTECTION
    ret = cts_disable_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

    if(!cts_dev->rtdata.esd_checking)
        flush_workqueue(cts_data->workqueue);

    ret = cts_plat_release_all_touch(cts_dev->pdata);
    if (ret) {
        cts_err("Release all touch failed %d", ret);
        return ret;
    }
    //flush_workqueue(cts_data->workqueue);

#ifdef CONFIG_CTS_VIRTUALKEY
    ret = cts_plat_release_all_vkey(cts_dev->pdata);
    if (ret) {
        cts_err("Release all vkey failed %d", ret);
        return ret;
    }
#endif /* CONFIG_CTS_VIRTUALKEY */

    return 0;
}

static bool cts_is_fwid_valid(u16 fwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].fwid  == fwid ) {
            return true;
        }
    }

    return false;
}

static bool cts_is_hwid_valid(u16 hwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].hwid == hwid) {
            return true;
        }
    }

    return false;
}

int cts_get_fwid(struct cts_device *cts_dev, u16 *fwid)
{
	int ret = 0;
	u8 temp = 0;

    cts_info("Get device firmware id");

    if (cts_dev->hwdata) {
        *fwid = cts_dev->hwdata->fwid;
        return 0;
    }

    if (cts_dev->rtdata.program_mode) {
        cts_err("Get device firmware id while in program mode");
        ret = -ENODEV;
        goto err_out;
    }

    ret = cts_fw_reg_readb_retry(cts_dev,
            CTS_DEVICE_FW_REG_CHIP_TYPE, &temp, 5, 1);
    if (ret) {
        goto err_out;
    }

    //*fwid = be16_to_cpu(*fwid);
    *fwid = temp;

    cts_info("Device firmware id: %04x", *fwid);

    if (!cts_is_fwid_valid(*fwid)) {
        cts_warn("Get invalid firmware id %04x", *fwid);
        ret = -EINVAL;
        goto err_out;
    }

    return 0;

err_out:
    *fwid = CTS_FWID_INVALID;
    return ret;
}

int cts_get_hwid(struct cts_device *cts_dev, u16 *hwid)
{
	int ret = 0;
    u32 u32temp = 0;
    u16 u16temp = 0;
	u16 addr_width = 0;
    u8 buff[4];

    cts_info("Get device hardware id");

    if (cts_dev->hwdata) {
        *hwid = cts_dev->hwdata->hwid;
        return 0;
    }

    cts_info("Device hardware data not initialized, try to read from register");

    if (!cts_dev->rtdata.program_mode) {
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            goto err_out;
        }
    }
    ret = cts_hw_reg_writel_retry(cts_dev, 0x40000, u32temp, 1, 0);
    if (ret) {
        goto err_out;
    }
    ret = cts_hw_reg_readl_retry(cts_dev, 0x40000, &u32temp, 5, 0);
    if (ret) {
        goto err_out;
    }

    buff[0]  = (u8) (u32temp & 0xff);
    buff[1]  = (u8) ((u32temp>>8) & 0xff);
    buff[2]  = (u8) ((u32temp>>16) & 0xff);
    buff[3]  = (u8) ((u32temp>>24) & 0xff);

    *hwid = (buff[1]<<8) | buff[2];

   // *hwid = le16_to_cpu(*hwid);

    cts_info("Get device hardware id from 0x040000: %04x", *hwid);

    if (!cts_is_hwid_valid(*hwid)) {
        // check if 87 or 81 ic.....
        addr_width = cts_dev->rtdata.addr_width;
        cts_dev->rtdata.addr_width   = 2;

        ret = cts_hw_reg_readw_retry(cts_dev, 0xf001, &u16temp, 5, 0);
        if (ret) {
            cts_dev->rtdata.addr_width   = addr_width;
            goto err_out;
        }
        //cts_dev->rtdata.addr_width   = addr_width;

        //u16temp == 0x87xx
        u16temp = be16_to_cpu(u16temp) & 0xff;
        *hwid = le16_to_cpu(u16temp);

        cts_info("Get device hardware id from 0xf001:  %04x", *hwid);

        if (!cts_is_hwid_valid(*hwid)) {
            cts_warn("Device hardware id %04x invalid", *hwid);
            ret = -EINVAL;
            goto err_out;
        }
    }

    return 0;

err_out:
    *hwid = CTS_HWID_INVALID;
    return ret;
}

#ifdef  SUPPORT_SENSOR_ID

int cts_get_hw_sensor_id(struct cts_device *cts_dev, u8 *hw_sensor_id)
{
    return  cts_fw_reg_readb_retry(cts_dev,
                    CTS_DEVICE_FW_REG_HW_SENSOR_ID, hw_sensor_id, 5, 0);
}

int cts_get_fw_sensor_id(struct cts_device *cts_dev, u8 *fw_sensor_id)
{
    return  cts_fw_reg_readb_retry(cts_dev,
                    CTS_DEVICE_FW_REG_FW_SENSOR_ID, fw_sensor_id, 5, 0);
}

int cts_get_is_sensor_id_match(struct cts_device *cts_dev, u8 *is_sensor_id_match)
{
    return  cts_fw_reg_readb_retry(cts_dev,
                    CTS_DEVICE_FW_REG_IS_SENSOR_ID_MATCH, is_sensor_id_match, 5, 0);
}

int cts_get_sensor_id_info(struct cts_device *cts_dev)
{
	int ret = 0;

    ret = cts_get_hw_sensor_id(cts_dev, &cts_dev->confdata.hw_sensor_id);
    if (ret) {
        cts_err("Get device hw sensor id failed %d", ret);
        return ret;
    }else{
        cts_info("Get device hw sensor id:0x%02x", cts_dev->confdata.hw_sensor_id);
    }

    ret = cts_get_fw_sensor_id(cts_dev, &cts_dev->confdata.fw_sensor_id);
    if (ret) {
        cts_err("Get device fw sensor id failed %d", ret);
        return ret;
    }else{
        cts_info("Get device fw sensor id:0x%02x", cts_dev->confdata.fw_sensor_id);
    }

    ret = cts_get_is_sensor_id_match(cts_dev, &cts_dev->confdata.is_sensor_matched);
    if (ret) {
        cts_err("Check if device sensor id matched failed %d", ret);
        return ret;
    }else{
        cts_info("Check if device sensor id matched:0x%02x", cts_dev->confdata.is_sensor_matched);
    }

    return 0;
}

#endif

//struct timeval start_tv;
//struct timeval end_tv;

int cts_probe_device(struct cts_device *cts_dev)
{
    int  ret, retries = 0;
    u16  fwid = CTS_FWID_INVALID;
    u16  hwid = CTS_HWID_INVALID;
    u16  device_fw_ver = 0;
    struct cts_firmware *firmware = NULL;

    cts_info("Probe device");

read_fwid:
    if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_NORMAL_MODE_I2CADDR)) {
        cts_init_rtdata_with_normal_mode(cts_dev);

        ret = cts_get_fwid(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id failed %d, retries %d", ret, retries);
        } else {
            ret = cts_fw_reg_readw_retry(cts_dev,
                    CTS_DEVICE_FW_REG_VERSION, &device_fw_ver, 5, 0);
            if (ret) {
                cts_err("Read firmware version failed %d", ret);
                device_fw_ver = 0;
                // TODO: Handle this error
            } else {
                device_fw_ver = be16_to_cpu(device_fw_ver);
                cts_info("Device firmware version: %04x", device_fw_ver);
            }
            goto init_hwdata;
        }
    } else {
        cts_warn("Normal mode i2c addr is offline");
    }

    /** - Try to read hardware id,
        it will enter program mode as normal */
    ret = cts_get_hwid(cts_dev, &hwid);
    if (ret || hwid == CTS_HWID_INVALID) {
        retries++;

        cts_err("Get hardware id failed %d retries %d", ret, retries);

        if (retries < 3) {
            cts_plat_reset_device(cts_dev->pdata);
            goto read_fwid;
        } else {
            return -ENODEV;
        }
    }

init_hwdata:
    ret = cts_init_device_hwdata(cts_dev, hwid, fwid);
    if (ret) {
        cts_err("Device hwid: %04x fwid: %04x not found", hwid, fwid);
        return -ENODEV;
    }

#ifdef SUPPORT_SENSOR_ID
request_firmware:
#endif
#ifdef CFG_CTS_FIRMWARE_FORCE_UPDATE
    cts_warn("Force update firmware");
    firmware = cts_request_firmware(cts_dev, hwid, fwid,  0);
#else /* CFG_CTS_FIRMWARE_FORCE_UPDATE */
    firmware = cts_request_firmware(cts_dev, hwid, fwid, device_fw_ver);
#endif /* CFG_CTS_FIRMWARE_FORCE_UPDATE */

    retries = 0;
update_firmware:
    if (firmware) {
        cts_info("firmware->name:%s,",firmware->name);

		//do_gettimeofday(&start_tv);
        //cts_info("update start time>>>>>>>>>>>>> %ldS.%4ldms",start_tv.tv_sec,start_tv.tv_usec/1000);

        ++retries;
        ret = cts_update_firmware(cts_dev, firmware, true);
        if (ret) {
            cts_err("Update firmware failed %d retries %d", ret, retries);

            if (retries < 3) {
                cts_plat_reset_device(cts_dev->pdata);
                goto update_firmware;
            } else {
                cts_release_firmware(firmware);
                return ret;
            }
        } 
        else {
  	    	//do_gettimeofday(&end_tv);
            //cts_info("update end time<<<<<<<<<<< %ldS.%4ldms",end_tv.tv_sec,end_tv.tv_usec/1000);
            //cts_info(">>>>>> update usage time = %4ldms <<<<<<",end_tv.tv_sec*1000+end_tv.tv_usec/1000-start_tv.tv_sec*1000-start_tv.tv_usec/1000);
            cts_release_firmware(firmware);
        }
    } else {
        if (fwid == CTS_FWID_INVALID) {
            /* Device without firmware running && no updatable firmware found */
            return -ENODEV;
        } else {
            goto Not_newer_firmware_found;
        }
    }
    msleep(30);

#ifdef SUPPORT_SENSOR_ID
    if(cts_dev->rtdata.is_chip_empty){
        ret = cts_get_fwid(cts_dev, &fwid);
        if (ret) {
            cts_err("Get firmware id again failed %d, retries %d", ret, retries);
        } else {
            ret = cts_fw_reg_readw_retry(cts_dev,
                CTS_DEVICE_FW_REG_VERSION, &device_fw_ver, 5, 0);
            if (ret) {
                cts_err("Read device firmware version again failed %d", ret);
                device_fw_ver = 0;
                // TODO: Handle this error
            } else {
                device_fw_ver = be16_to_cpu(device_fw_ver);
                cts_info("Read device firmware version again: %04x", device_fw_ver);
            }
        }
        cts_dev->rtdata.is_chip_empty = false;

        ret = cts_get_sensor_id_info(cts_dev);
        if (ret) {
            cts_err("Get sensor id failed %d", ret);
            return ret;
        }
        goto request_firmware;
    }

#endif

Not_newer_firmware_found:
    //cts_update_device_fwid(cts_dev);
    ret = cts_init_fwdata(cts_dev);
    if (ret) {
         cts_err("Device init firmware data failed %d", ret);
         return ret;
    }
    return 0;
}

#ifdef CONFIG_CTS_GESTURE
void cts_enable_gesture_wakeup(struct cts_device *cts_dev)
{
    cts_info("Enable gesture wakeup");
    cts_dev->rtdata.gesture_wakeup_enabled = true;
}

void cts_disable_gesture_wakeup(struct cts_device *cts_dev)
{
    cts_info("Disable gesture wakeup");
    cts_dev->rtdata.gesture_wakeup_enabled = false;
}

bool cts_is_gesture_wakeup_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.gesture_wakeup_enabled;
}

int cts_get_gesture_info(const struct cts_device *cts_dev,
        void *gesture_info, bool trace_point)
{
    int    ret;

    cts_info("Get gesture info");

    if (cts_dev->rtdata.program_mode) {
        cts_warn("Get gesture info in program mode");
        return -ENODEV;
    }

    if (!cts_dev->rtdata.suspended) {
        cts_warn("Get gesture info while not suspended");
        return -ENODEV;
    }

    if (!cts_dev->rtdata.gesture_wakeup_enabled) {
        cts_warn("Get gesture info while gesture wakeup not enabled");
        return -ENODEV;
    }

    ret = cts_fw_reg_readsb(cts_dev,
            CTS_DEVICE_FW_REG_GESTURE_INFO, gesture_info, 2);
    if(ret) {
        cts_err("Read gesture info header failed %d", ret);
        return ret;
    }

    if (trace_point) {
        ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_GESTURE_INFO + 2,
                gesture_info + 2,
                (((u8 *)gesture_info))[1] * sizeof(struct cts_device_gesture_point));
        if(ret) {
            cts_err("Read gesture trace points failed %d", ret);
            return ret;
        }
    }

    return 0;
}
#endif /* CONFIG_CTS_GESTURE */

#ifdef CONFIG_CTS_ESD_PROTECTION
static int cts_esd_recover(struct cts_device *cts_dev)
{
	struct cts_firmware *firmware;
	int ret = 0;

    cts_info("ESD recover... ");

    if(cts_dev->hwdata->is_with_flash){
        cts_plat_reset_device(cts_dev->pdata);
    }else{
        firmware = cts_request_firmware(cts_dev,cts_dev->hwdata->hwid,
                cts_dev->hwdata->fwid, 0);
        if (firmware) {
            ret = cts_update_firmware(cts_dev, firmware, true);
            cts_release_firmware(firmware);
            if (ret) {
                cts_err("ESD recover update default firmware failed %d", ret);
            }
        } else {
            cts_plat_reset_device(cts_dev->pdata);
        }
    }
    
    return 0;
}

static void cts_esd_protection_work(struct work_struct *work)
{
    struct chipone_ts_data *cts_data;
	int ret = 0;
	u16 version = 0;

    cts_info("ESD protection work");
    
    cts_data = container_of(work, struct chipone_ts_data, esd_work.work);
    cts_data->cts_dev.rtdata.esd_checking = 1;

    cts_lock_device(&cts_data->cts_dev);

    ret = cts_get_firmware_version(&cts_data->cts_dev, &version);
    if (ret) {
        cts_err("ESD protection read rVERSION failed %d", ret);
        cts_data->esd_check_fail_cnt++;

        if (cts_data->esd_check_fail_cnt >= 3) {
            cts_warn("ESD protection check failed, reset chip !!!");

            cts_stop_device(&cts_data->cts_dev);

            cts_esd_recover(&cts_data->cts_dev);
            
            cts_start_device(&cts_data->cts_dev);
            cts_data->esd_check_fail_cnt = 0;
            cts_unlock_device(&cts_data->cts_dev);
            cts_data->cts_dev.rtdata.esd_checking = 0;
            return;

        }
    } else {
        cts_data->esd_check_fail_cnt = 0;
    }

    cts_unlock_device(&cts_data->cts_dev);
    cts_data->cts_dev.rtdata.esd_checking = 0;

    queue_delayed_work(cts_data->workqueue,
        &cts_data->esd_work, CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);
}

int cts_enable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->workqueue && !cts_data->esd_enabled) {
        cts_info("ESD protection enable");

        cts_data->esd_enabled = true;
        queue_delayed_work(cts_data->workqueue,
            &cts_data->esd_work, CFG_CTS_ESD_PROTECTION_CHECK_PERIOD);
    }
    return 0;
}

int cts_disable_esd_protection(struct chipone_ts_data *cts_data)
{
    if (cts_data->workqueue && cts_data->esd_enabled) {
        cts_info("ESD protection disable");
        cts_data->esd_enabled = false;
        cancel_delayed_work(&cts_data->esd_work);
    }
    return 0;
}

void cts_init_esd_protection(struct chipone_ts_data *cts_data)
{
    cts_info("Init ESD protection");

    INIT_DELAYED_WORK(&cts_data->esd_work, cts_esd_protection_work);

    cts_data->esd_enabled = false;
    cts_data->esd_check_fail_cnt = 0;
}

void cts_deinit_esd_protection(struct chipone_ts_data *cts_data)
{
    cts_info("De-Init ESD protection");

    if (cts_data->workqueue && cts_data->esd_enabled) {
        cts_data->esd_enabled = false;
        cancel_delayed_work(&cts_data->esd_work);
    }
}
#endif /* CONFIG_CTS_ESD_PROTECTION */

#ifdef CONFIG_CTS_GLOVE
int cts_enter_glove_mode(struct cts_device *cts_dev)
{
	int ret = 0;
    cts_info("Enter glove mode");
    ret=  cts_fw_reg_writeb(cts_dev, 0x0004, 0x90);
    if(!ret){
        cts_dev->rtdata.glove_mode_enabled = true;
    }
    return ret;
}

int cts_exit_glove_mode(struct cts_device *cts_dev)
{
	int ret = 0;
    cts_info("Exit glove mode");
    ret = cts_fw_reg_writeb(cts_dev, 0x0004, 0x91);
    if(!ret){
        cts_dev->rtdata.glove_mode_enabled = false;
    }
    return ret;
}

int cts_is_glove_enabled(struct cts_device *cts_dev)
{
    return cts_dev->rtdata.glove_mode_enabled;    
}   
#endif /* CONFIG_CTS_GLOVE */

#ifdef CONFIG_CTS_CHARGER_DETECT
bool cts_is_charger_exist(struct cts_device *cts_dev)
{
    return cts_dev->rtdata.charger_exist;
}

int cts_charger_plugin(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Charger plugin");
    ret = cts_send_command(cts_dev, CTS_CMD_CHARGER_PLUG_IN);
    if (ret) {
        cts_err("Send CMD_CHARGER_PLUG_IN failed %d", ret);
    } else {
        cts_dev->rtdata.charger_exist = true;
    }
    return 0;
}

int cts_charger_plugout(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Charger plugout");
    ret = cts_send_command(cts_dev, CTS_CMD_CHARGER_PLUG_OUT);
    if (ret) {
        cts_err("Send CMD_CHARGER_PLUG_OUT failed %d", ret);
    } else {
        cts_dev->rtdata.charger_exist = false;
    }
    return 0;
}
#endif /* CONFIG_CTS_CHARGER_DETECT */

