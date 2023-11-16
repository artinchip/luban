// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <dm.h>
#include <misc.h>
#include <hexdump.h>
#include <asm/io.h>
#include <linux/delay.h>

#ifndef CONFIG_SPL_BUILD

static struct udevice *efuse_dev;
static u8 *g_fake_efuse;

static int do_efuse_list(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	int ret = 0;

	printf("bank list:\n");
	printf("  disread                       : 0x00 ~ 0x07\n");
	printf("  diswrite                      : 0x08 ~ 0x0F\n");
	printf("  chipid                        : 0x10 ~ 0x1F\n");
	printf("  cali                          : 0x20 ~ 0x2F\n");
	printf("  brom                          : 0x30 ~ 0x37\n");
	printf("  secure                        : 0x38 ~ 0x3F\n");
	printf("  rotpk                         : 0x40 ~ 0x4F\n");
	printf("  ssk                           : 0x50 ~ 0x5F\n");
	printf("  huk                           : 0x60 ~ 0x6F\n");
	printf("  psk0                          : 0x70 ~ 0x77\n");
	printf("  psk1                          : 0x78 ~ 0x7F\n");
	printf("  psk2                          : 0x80 ~ 0x87\n");
	printf("  psk3                          : 0x88 ~ 0x8F\n");
	printf("  nvcntr                        : 0x90 ~ 0x9F\n");
	printf("  spienc_key                    : 0xA0 ~ 0xAF\n");
	printf("  spienc_nonce                  : 0xB0 ~ 0xB7\n");
	printf("  pnk                           : 0xB8 ~ 0xBF\n");
	printf("  customer                      : 0xC0 ~ 0xFF\n");

	printf("\nbits list:\n");
	printf("  brom.primary  : 1(NAND), 2(NOR), 3(eMMC), 4(SDCard)\n");
	printf("  brom.secondary: 1(NAND), 2(NOR), 3(eMMC), 4(SDCard)\n");
	printf("  brom.skip_sd_phase\n");
	printf("  brom.checksum_dis\n");
	printf("  brom.spi_boot_intf\n");
	printf("  secure.jtag_lock\n");
	printf("  secure.secure_boot_en\n");
	printf("  secure.encrypt_boot_en\n");
	printf("  secure.anti_rollback_en\n");
	printf("  secure.spi_enc_en\n");
	return ret;
}

static int efuse_read(struct udevice *dev, int offset, void *buf, int size)
{
	int ret = size;

	if (g_fake_efuse)
		memcpy(buf, g_fake_efuse + offset, size);
	else
		ret = misc_read(dev, offset, buf, size);
	return ret;
}

static int efuse_write(struct udevice *dev, int offset, void *buf, int size)
{
	int ret = size;

	if (g_fake_efuse)
		memcpy(g_fake_efuse + offset, buf, size);
	else
		ret = misc_write(dev, offset, buf, size);
	return ret;
}

static int do_efuse_set_fake(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	unsigned long addr;
	char *pe;

	if (argc != 2)
		return CMD_RET_USAGE;
	addr = ustrtoul(argv[1], &pe, 0);

	g_fake_efuse = (void *)addr;

	if (g_fake_efuse)
		memset(g_fake_efuse, 0, 256);

	return 0;
}

static int get_bank_base(char *const bank, u32 *base, u32 *len)
{
	if (strcmp(bank, "disread") == 0) {
		*base = 0x0;
		*len = 0x8;
		return 0;
	} else if (strcmp(bank, "diswrite") == 0) {
		*base = 0x8;
		*len = 0x8;
		return 0;
	} else if (strcmp(bank, "chipid") == 0) {
		*base = 0x10;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "cali") == 0) {
		*base = 0x20;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "brom") == 0) {
		*base = 0x30;
		*len = 0x8;
		return 0;
	} else if (strcmp(bank, "secure") == 0) {
		*base = 0x38;
		*len = 0x8;
		return 0;
	} else if (strcmp(bank, "rotpk") == 0) {
		*base = 0x40;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "ssk") == 0) {
		*base = 0x50;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "huk") == 0) {
		*base = 0x60;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "psk0") == 0) {
		*base = 0x70;
		*len = 0x08;
		return 0;
	} else if (strcmp(bank, "psk1") == 0) {
		*base = 0x78;
		*len = 0x08;
		return 0;
	} else if (strcmp(bank, "psk2") == 0) {
		*base = 0x80;
		*len = 0x08;
		return 0;
	} else if (strcmp(bank, "psk3") == 0) {
		*base = 0x88;
		*len = 0x08;
		return 0;
	} else if (strcmp(bank, "nvcntr") == 0) {
		*base = 0x90;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "spienc_key") == 0) {
		*base = 0xA0;
		*len = 0x10;
		return 0;
	} else if (strcmp(bank, "spienc_nonce") == 0) {
		*base = 0xB0;
		*len = 0x8;
		return 0;
	} else if (strcmp(bank, "pnk") == 0) {
		*base = 0xB8;
		*len = 0x8;
		return 0;
	} else if (strcmp(bank, "customer") == 0) {
		*base = 0xC0;
		*len = 0x40;
		return 0;
	}

	return -1;
}

static int get_bits_base(char *const bits, u32 *base, u32 *ofs, u32 *msk)
{
	if (strcmp(bits, "brom.primary") == 0) {
		*base = 0x30;
		*ofs = 0;
		*msk = 0xF << *ofs;
		return 0;
	} else if (strcmp(bits, "brom.secondary") == 0) {
		*base = 0x30;
		*ofs = 4;
		*msk = 0xF << *ofs;
		return 0;
	} else if (strcmp(bits, "brom.skip_sd_phase") == 0) {
		*base = 0x30;
		*ofs = 8;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "brom.checksum_dis") == 0) {
		*base = 0x30;
		*ofs = 14;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "brom.spi_boot_intf") == 0) {
		*base = 0x30;
		*ofs = 15;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "secure.jtag_lock") == 0) {
		*base = 0x38;
		*ofs = 0;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "secure.secure_boot_en") == 0) {
		*base = 0x38;
		*ofs = 16;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "secure.encrypt_boot_en") == 0) {
		*base = 0x38;
		*ofs = 17;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "secure.anti_rollback_en") == 0) {
		*base = 0x38;
		*ofs = 18;
		*msk = 0x1 << *ofs;
		return 0;
	} else if (strcmp(bits, "secure.spi_enc_en") == 0) {
		*base = 0x38;
		*ofs = 19;
		*msk = 0x1 << *ofs;
		return 0;
	}

	return -1;
}

static struct udevice *get_efuse_device(void)
{
	struct udevice *dev = NULL;
	int ret;

	if (efuse_dev)
		return efuse_dev;

	ret = uclass_first_device_err(UCLASS_MISC, &dev);
	if (ret) {
		pr_err("Get UCLASS_MISC device failed.\n");
		return NULL;
	}

	do {
		if (device_is_compatible(dev, "artinchip,aic-sid-v1.0"))
			break;
		ret = uclass_next_device_err(&dev);
	} while (dev);

	efuse_dev = dev;
	return efuse_dev;
}

static int do_efuse_dump(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	struct udevice *dev = NULL;
	int ret = 0, i, j, rdcnt;
	unsigned long size, offset;
	u32 buf[64], base, bank_size, bitofs, bitmsk;
	u8 *p, c;
	char *pe;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = get_bank_base(argv[1], &base, &bank_size);
	if (ret == 0 && argc != 4)
		return CMD_RET_USAGE;
	if (ret)
		ret = get_bits_base(argv[1], &base, &bitofs, &bitmsk);
	if (ret) {
		pr_err("Failed to get efuse info:%s\n", argv[1]);
		return -1;
	}

	dev = get_efuse_device();
	if (!dev) {
		pr_err("Failed to get efuse device.\n");
		return -1;
	}

	if (argc == 4) {
		offset = ustrtoul(argv[2], &pe, 0);
		size = ustrtoul(argv[3], &pe, 0);

		if (size > bank_size) {
			pr_err("Size is large than bank size.\n");
			return -1;
		}
		rdcnt = efuse_read(dev, base + offset, buf, size);
		p = (u8 *)buf;
		printf("%s:\n", argv[1]);
		for (i = 0; i < rdcnt; i += 16) {
			for (j = i; j  < i + 16; j++) {
				if (j < rdcnt)
					printf("%02X ", p[j]);
				else
					printf("   ");
			}
			printf("\t|");
			for (j = i; (j < rdcnt) && (j < i + 16); j++) {
				c = p[j] >= 32 && p[j] < 127 ? p[j] : '.';
				printf("%c", c);
			}
			printf("|\n");
		}

		printf("\n");
	} else {
		rdcnt = efuse_read(dev, base, buf, 4);
		printf("%s = 0x%x\n", argv[1], (buf[0] & bitmsk) >> bitofs);
	}
	return ret;
}

static int do_efuse_read(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	struct udevice *dev = NULL;
	int ret = 0, rdcnt;
	unsigned long addr, size, offset;
	u32 base, bank_size;
	u8 *p;
	char *pe;

	if (argc < 5)
		return CMD_RET_USAGE;
	ret = get_bank_base(argv[1], &base, &bank_size);
	if (ret) {
		pr_err("Failed to get efuse info:%s\n", argv[1]);
		return -1;
	}

	offset = ustrtoul(argv[2], &pe, 0);
	size = ustrtoul(argv[3], &pe, 0);
	addr = ustrtoul(argv[4], &pe, 0);

	if (size > bank_size) {
		pr_err("Size is large than bank size.\n");
		return -1;
	}

	p = (u8 *)addr;
	dev = get_efuse_device();
	if (!dev) {
		pr_err("Failed to get efuse device.\n");
		return -1;
	}

	rdcnt = efuse_read(dev, base + offset, p, size);
	if (rdcnt != size) {
		pr_err("Failed to read eFuse.\n");
		return -1;
	}
	return 0;
}

static int do_efuse_write(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct udevice *dev = NULL;
	int ret = 0, wrcnt;
	unsigned long addr, size, offset;
	u32 base, bank_size;
	u8 *p;
	char *pe;

	if (argc < 5)
		return CMD_RET_USAGE;
	ret = get_bank_base(argv[1], &base, &bank_size);
	if (ret) {
		pr_err("Failed to get efuse info:%s\n", argv[1]);
		return -1;
	}

	offset = ustrtoul(argv[2], &pe, 0);
	size = ustrtoul(argv[3], &pe, 0);
	addr = ustrtoul(argv[4], &pe, 0);

	if (size > bank_size) {
		pr_err("Size is large than bank size.\n");
		return -1;
	}

	p = (u8 *)addr;
	dev = get_efuse_device();
	if (!dev) {
		pr_err("Failed to get efuse device.\n");
		return -1;
	}

	wrcnt = efuse_write(dev, base + offset, p, size);
	if (wrcnt != size) {
		pr_err("Failed to write eFuse.\n");
		return -1;
	}
	return 0;
}

static int do_efuse_writehex(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct udevice *dev = NULL;
	int ret = 0, i, j, wrcnt;
	unsigned long size, offset;
	u32 base, bank_size;
	u8 *p, *data, buf[64];
	u8 byte[3] = {0x00, 0x00, 0x00};
	char *pe;

	if (argc < 4)
		return CMD_RET_USAGE;
	ret = get_bank_base(argv[1], &base, &bank_size);
	if (ret) {
		pr_err("Failed to get efuse info:%s\n", argv[1]);
		return -1;
	}

	offset = ustrtoul(argv[2], &pe, 0);
	data = argv[3];
	size = strlen(data) / 2;

	if (size > bank_size) {
		pr_err("Size is large than bank size.\n");
		return -1;
	}

	/* hex string to hex value */
	for (i = 0, j = 0; i < strlen(data) -1; i += 2, j += 1) {
		byte[0] = data[i];
		byte[1] = data[i + 1];
		buf[j] = simple_strtol(byte, &pe, 16);
	}

	p = (u8 *)buf;
	dev = get_efuse_device();
	if (!dev) {
		pr_err("Failed to get efuse device.\n");
		return -1;
	}

	wrcnt = efuse_write(dev, base + offset, p, size);
	if (wrcnt != size) {
		pr_err("Failed to write eFuse.\n");
		return -1;
	}
	return 0;
}

static int do_efuse_writestr(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct udevice *dev = NULL;
	int ret = 0, wrcnt;
	unsigned long size, offset;
	u32 base, bank_size;
	u8 *p, *data;
	char *pe;

	if (argc < 4)
		return CMD_RET_USAGE;
	ret = get_bank_base(argv[1], &base, &bank_size);
	if (ret) {
		pr_err("Failed to get efuse info:%s\n", argv[1]);
		return -1;
	}

	offset = ustrtoul(argv[2], &pe, 0);
	data = argv[3];
	size = strlen(data);

	if (size > bank_size) {
		pr_err("Size is large than bank size.\n");
		return -1;
	}

	p = (u8 *)data;
	dev = get_efuse_device();
	if (!dev) {
		pr_err("Failed to get efuse device.\n");
		return -1;
	}

	wrcnt = efuse_write(dev, base + offset, p, size);
	if (wrcnt != size) {
		pr_err("Failed to write eFuse.\n");
		return -1;
	}
	return 0;
}

static int do_efuse_setbits(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	struct udevice *dev = NULL;
	int ret = 0, wrcnt;
	unsigned long val;
	u32 base, bitofs, bitmsk;
	char *pe;

	if (argc < 3)
		return CMD_RET_USAGE;
	ret = get_bits_base(argv[1], &base, &bitofs, &bitmsk);
	if (ret) {
		pr_err("Failed to get efuse info:%s\n", argv[1]);
		return -1;
	}

	val = ustrtoul(argv[2], &pe, 0);
	val = (val << bitofs) & bitmsk;

	dev = get_efuse_device();
	if (!dev) {
		pr_err("Failed to get efuse device.\n");
		return -1;
	}

	wrcnt = efuse_write(dev, base, &val, 4);
	if (wrcnt != 4) {
		pr_err("Failed to write eFuse bits.\n");
		return -1;
	}
	return 0;
}

static char efuse_help_text[] =
	"ArtInChip eFuse read/write command\n\n"
	"efuse list                             : List all banks/bits name\n"
	"efuse fake     addr                    : Set RAM address as fake eFuse space for testing\n"
	"                                         Set to 0 to use real eFuse\n"
	"efuse dump     bank offset size        : Dump data in bank\n"
	"efuse read     bank offset size addr   : Read data in bank to RAM address\n"
	"efuse write    bank offset size addr   : Write data to bank from RAM address\n"
	"efuse writehex bank offset data        : Write data to bank from input hex string\n"
	"efuse writestr bank offset data        : Write data to bank from input string\n"
	"efuse dump     bitsname                : Dump data in bitsname\n"
	"efuse set      bitsname value          : Set bitsname to the specific value\n";

U_BOOT_CMD_WITH_SUBCMDS(efuse, "ArtInChip eFuse command", efuse_help_text,
			U_BOOT_SUBCMD_MKENT(list, 1, 0, do_efuse_list),
			U_BOOT_SUBCMD_MKENT(fake, 2, 0, do_efuse_set_fake),
			U_BOOT_SUBCMD_MKENT(dump, 4, 0, do_efuse_dump),
			U_BOOT_SUBCMD_MKENT(read, 5, 0, do_efuse_read),
			U_BOOT_SUBCMD_MKENT(write, 5, 0, do_efuse_write),
			U_BOOT_SUBCMD_MKENT(writehex, 4, 0, do_efuse_writehex),
			U_BOOT_SUBCMD_MKENT(writestr, 4, 0, do_efuse_writestr),
			U_BOOT_SUBCMD_MKENT(set, 3, 0, do_efuse_setbits));
#endif
