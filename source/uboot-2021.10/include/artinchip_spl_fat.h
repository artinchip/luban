// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */
#ifndef __ARTINCHIP_SPL_FAT_H
#define __ARTINCHIP_SPL_FAT_H

#include <linux/ctype.h>
#include <common.h>
#include <image.h>
#include <fat.h>
#include <config_parse.h>
#include <spl.h>

/* Disk Partition Table */
#define DPT_START 0x1be

/* BIOS Parameter Block
 * BPB locate in DBR(DOS Boot Record), 0x0B~0x52
 */
#define BPB_START 0x0b

/* FAT32: Filesystem type string (8-byte) */
#define BS_FilSysType32		82
#define _TO_U16(x, s) ((u16)(x[s+1]<<8|x[s]))
#define _TO_U32(x, s) ((u32)(x[s+3]<<24|x[s+2]<<16|x[s+1]<<8|x[s]))

#define BPB_SECT_SIZE(x) _TO_U16(x, 0x0b)
#define BPB_CLUS_SIZE(x) ((u8)(x[0x0d]))
#define BPB_RESV_SECT_CNT(x) _TO_U16(x, 0x0e)
#define BPB_FAT_NUM(x) ((u8)(x[0x10]))
#define LBA_SIZE 512
/* FAT32: FAT table size(sector) */
#define BPB_FAT32_SIZE(x) _TO_U32(x, 0x24)
//((u32)(x[0x27]<<24|x[0x26]>>16|x[0x25]<<8|x[0x24]))
#define BPB_ROOT_ENTRY_CNT(x) _TO_U16(x, 0x11)
#define BPB_ROOT_START_CLUS32(x) _TO_U32(x, 0x2c)

/*
 * FAT32 will create short name dirent for long file name, to make compabtible
 * for old application
 */
#define SHT_DIR_Name_idx	0
#define SHT_DIR_Name_Suffix_idx 8
#define SHT_DIR_Attr(d)		((u8)d[0xB])
#define SHT_DIR_StartCluster(d)	((u32)(d[0x15]<<24|d[0x14]<<16|d[0x1B]<<8|d[0x1A]))
#define SHT_DIR_Filesize(d)	_TO_U32(d, 0x1C)

#define DIRENT_DEL		0xE5
#define DIRENT_END		0x0

#define DIR_ATTR_RW		0
#define DIR_ATTR_RO		1
#define DIR_ATTR_HIDDEN		2
#define DIR_ATTR_SYSTEM		4
#define DIR_ATTR_VOLUME		8
#define DIR_ATTR_SUBDIR		16
#define DIR_ATTR_ARCHIVE	32

/* 1: Last dirent for LFN */
#define LFN_DIR_Attr_Last(d)	((d[0] & 0x40)>>6)
#define LFN_DIR_Attr_Number(d)	((d[0] & 0x1F))
#define LFN_DIR_Flag(d)		(d[0xB] == 0x0F)
#define LFN_DIR_StartCluster(d)	_TO_U16(d, 0x1A)

#define DIR_ENTRY_SIZE		32
/*
 * One FAT item use 4 bytes
 */
#define MAX_FAT_ITEM_PER_SECT (LBA_SIZE >> 2)
/*
 * Default Cluster size for FAT32:
 * Partition Size | Cluster Size
 * 512MB ~ 8GB    | 4KB
 * 8GB   ~ 16GB   | 8KB
 * 16GB  ~ 32GB   | 16KB
 * > 32GB         | 32KB
 *
 * FSBL image should not large than 256 KB, so here set MAX to 64
 */
#define IMG_MAX_CLUSTER 256
#define IMAGE_HEADER_SIZE 0x40
#define BOOTLOADER_BUF_ADDR (CONFIG_SYS_TEXT_BASE - IMAGE_HEADER_SIZE)

/*IMAGE name fixed config.txt for test */
#define IMAGE_NAME "BOOTCFG "
#define IMAGE_NAME_SIZ 8
#define IMAGE_SUFFIX "TXT"
#define IMAGE_SUFFIX_SIZ 3

#define F_PARSE_NONE 0
#define F_PARSE_LONG 1
#define F_PARSE_SHORT 2
#define TYPE_RAW_VOLUME_FAT32 1
#define TYPE_MBR_PART_FAT32 2
#define TYPE_GPT_PART_FAT32 3



/* Disk Partition Table Entry
 *
 * MBR first 446 bytes(Byte[0:445]) are reserved for boot code.
 * DPT 64 bytes, locate at Byte[446:509], 4 entries, 16 bytes per partition
 * MBR Identification Codes 2 bytes, Byte[510:511], value 0x55, 0xAA
 *
 * Partition Type:
 *     0x00: Empty partition table entry
 *     0x01: DOS FAT12
 *     0x04: DOS FAT16
 *     0x05: DOS3.3+extended partition
 *     0x06: DOS3.31+FAT16
 *     0x07: Maybe exFAT
 *     0x0B: Win95+FAT32
 *     0x0C: Win95+FAT32(using LBA-mode INT 13 extensions)
 *     0x0E: DOS FAT16(over 32MB, using INT 13 extensions)
 *     0x1B: Hidden Win95+FAT32
 *     0x1C: Hidden Win95+FAT32(using LBA-mode INT 13 extensions)
 */
struct mbr_dpt_entry {
	/* 0x80: This partition is activing; 0x00: Partition is not activing. */
	u8	boot_indicator;
	u8	start_head;
	u16	start_sector	:6;
	u16	start_cylinder	:10;
	/* Partition type: somewhere call it Operating system indicator. */
	u8	part_type;
	u8	end_head;
	u16	end_sector	:6;
	u16	end_cylinder	:10;
	/* Partition start LBA */
	u32	lba_start;
	/* Partition length: count of LBA */
	u32	lba_cnt;
};

/* Some key information from FAT32 BPB */
struct fat32_bpb {
	u16	sector_size;
	u16	cluster_size;
	u32	fat_start;
	u32	fat_size;
	u32	data_start;
	u32	root_dirent_cluster;
};

struct gpt_header {
	u8 signature[8];
	u32 revision;
	u32 header_size;
	u32 header_crc32;
	u32 reserved1;
	u64 my_lba;
	u64 alternate_lba;
	u64 first_usable_lba;
	u64 last_usable_lba;
	u8 disk_guid[16];
	u64 partition_entry_lba;
	u32 num_partition_entries;
	u32 sizeof_partition_entry;
	u32 partition_entry_array_crc32;
};

struct gpt_entry {
	u8 partition_type_guid[16];
	u8 unique_partition_guid[16];
	u64 starting_lba;
	u64 ending_lba;
	u8 other[80]; // attr and part name
};

int aic_spl_load_image_fat(struct spl_image_info *spl_image,
				struct blk_desc *block_dev,
				const char *filename);

#endif /* __ARTINCHIP_SPL_FAT_H */

