// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#include <common.h>
#include <fat.h>
#include <spl.h>
#include <log.h>
#include <blk.h>
#include <asm/unaligned.h>
#include <config_parse.h>
#include <artinchip_spl_fat.h>

static struct blk_desc *cur_dev;
static struct fat32_bpb *cur_bpb;
static struct spl_image_info *cur_image;

static int check_identifier(u8 *header)
{
	struct mbr_dpt_entry part;

	/* Last two bytes, MBR identifier */
	if ((header[510] != 0x55) || (header[511] != 0xAA)) {
		debug("Not MBR");
		return -1;
	}

	/* Check FAT32 string, 8 characters */
	if (strncmp((char *)&header[BS_FilSysType32], "FAT32   ", 8) == 0)
		return TYPE_RAW_VOLUME_FAT32;

	/* Check partition type flag */
	memcpy(&part, header + DPT_START, sizeof(struct mbr_dpt_entry));

	if (part.lba_start == 0 || part.lba_cnt == 0) {
		debug("partition info not correct.\n");
		return -1;
	}

	switch (part.part_type) {
	case 0x0B: /* Win95+FAT32 */
	case 0x0C: /* Win95+FAT32(using LBA-mode INT 13 extensions) */
	case 0x1B: /* Hidden Win95+FAT32 */
	case 0x1C: /* Hidden Win95+FAT32(using LBA-mode INT 13 extensions) */
		return TYPE_MBR_PART_FAT32;
	case 0xEE: /* GPT */
		return TYPE_GPT_PART_FAT32;
	}

	debug("Unknown header.\n");
	return -1;
}

static int search_boot_cfg_file(u32 *cluster, ulong *filesiz, u8 *buf)
{
	u32 blkcnt, dirent_cnt, dirent_start, clus_siz;
	u32 sect_idx = 0, i = 0, status = 0;
	u8 *pdirent;
	char shname[12];

	dirent_start = cur_bpb->data_start;
	clus_siz = cur_bpb->cluster_size;

	if (cur_bpb->root_dirent_cluster != 2) {
		debug("Root directory entry should use cluster 2.\n");
		return -1;
	}

	/* Step1: Search File in dirent sectors  */
	for (sect_idx = 0; sect_idx < clus_siz; sect_idx++) {
		pdirent = (u8 *)buf; /* Read and search */
		blkcnt = blk_dread(cur_dev, dirent_start + sect_idx, 1,
				   pdirent);
		if (blkcnt != 1) {
			debug("Read dirent sector failed.\n");
			return -1;
		}

		dirent_cnt = (LBA_SIZE >> 5);
		shname[11] = 0;
		/* Search in Short DIRENTs only */
		for (i = 0; i < dirent_cnt; i++, pdirent += DIR_ENTRY_SIZE) {
			if (pdirent[0] == DIRENT_END) { /* End of dirent list */
				debug("No dirent\n");
				return -1;
			}
			if (pdirent[0] == DIRENT_DEL) /* Deleted item */
				continue;
			if (LFN_DIR_Flag(pdirent)) /* Long DIRENT */
				continue;
			memcpy(shname, pdirent, 11);

			if (SHT_DIR_Filesize(pdirent) != 0) {
				status = strncmp(IMAGE_NAME,
						 (const char *)pdirent,
						 IMAGE_NAME_SIZ);
				status += strncmp(IMAGE_SUFFIX,
						  (const char *)&pdirent[8],
						  IMAGE_SUFFIX_SIZ);
				if (status != 0)
					continue;
				*cluster = SHT_DIR_StartCluster(pdirent);
				debug("Start cluster: %d\n", *cluster);
				*filesiz = SHT_DIR_Filesize(pdirent);
				debug("bootcfg file size: %lu\n", *filesiz);

				return 0;
			}
		}
	}

	debug("search bootcfg file failed.\n");
	return -1;
}

static int read_file_data(u32 *clusters, ulong filesiz, ulong sect_off,
			  u32 byte_off, u8 *filebuf)
{
	u32 idx = 0, sect = 0, sect_cnt = 0, cnt = 0, cur = 0;
	u32 to_read = 0, read_len = 0, data_sector = 0, byte_len = 0;
	u8 *p;
	u8 temp[LBA_SIZE];

	p = (u8 *)filebuf;
	for (idx = 0; idx < IMG_MAX_CLUSTER; idx++) {
		cur = clusters[idx];
		debug("Read cluster id %d\n", cur);
		if (cur == 0)
			break;
		data_sector = cur_bpb->data_start + (cur - 2)
					* cur_bpb->cluster_size;
		sect_cnt = cur_bpb->cluster_size;
		/*
		 * sect_off: The first data's sector offset in the first
		 * cluster
		 */
		if (sect_off) {
			/* Only set offset for the first data */
			data_sector += sect_off;
			sect_cnt -= sect_off;
			sect_off = 0;
		}

		/*
		 * byte_off: The first data's byte offset in the first sector
		 */
		if (byte_off) {
			cnt = blk_dread(cur_dev, data_sector, 1, temp);
			if (cnt != 1) {
				debug("Read file failed.\n");
				return -1;
			}
			byte_len = LBA_SIZE - byte_off;
			memcpy(p, temp+byte_off, byte_len);
			p += byte_len;
			read_len += byte_len;
			data_sector += 1;
			sect_cnt -= 1;
			byte_off = 0;
			if (read_len >= filesiz)
				return 0;
		}
		to_read = sect_cnt * LBA_SIZE;

		if ((read_len + to_read) <= filesiz) {
			cnt = blk_dread(cur_dev, data_sector, sect_cnt, p);
			if (cnt != sect_cnt) {
				debug("Read file failed.\n");
				return -1;
			}
			p += to_read;
			read_len += to_read;
			if (read_len >= filesiz)
				return 0;
		} else {
			for (sect = 0; sect < sect_cnt; sect++) {
				cnt = blk_dread(cur_dev, data_sector + sect,
						1, p);
				if (cnt != 1) {
					debug("Read file failed.\n");
					return -1;
				}
				p += LBA_SIZE;
				read_len += LBA_SIZE;
				if (read_len >= filesiz)
					return 0;
			}
		}
	}

	debug("Read file data failed.\n");
	return -1;
}

static int search_boot_image_file(const char *filename, u32 *cluster,
				  ulong *actread, u8 *buf)
{
	u32 i = 0, j = 0, blkcnt, chidx = 0, do_flag = 0, sect_idx = 0;
	u32 dirent_cnt, dirent_start, clus_siz;
	u8 *pdirent;
	char lngname[IMG_NAME_MAX_SIZ];

	dirent_start = cur_bpb->data_start;
	clus_siz = cur_bpb->cluster_size;

	if (cur_bpb->root_dirent_cluster != 2) {
		debug("Root directory entry should use cluster 2.\n");
		return -1;
	}
	/* Step1: Search File in dirent sectors  */
	for (sect_idx = 0; sect_idx < clus_siz; sect_idx++) {
		pdirent = (u8 *)buf; /* Read and search */
		blkcnt = blk_dread(cur_dev, dirent_start + sect_idx, 1,
				   pdirent);
		if (blkcnt != 1) {
			debug("Read dirent sector failed.\n");
			return -1;
		}

		dirent_cnt = (LBA_SIZE >> 5);
		/* Search in Long DIRENT only
		 *
		 * Step:
		 * 1. Get parse long DIRENT to get long name first
		 * 2. Compare long name
		 * 3. If long name is match, then parse Short DIRENT
		 *    to get start cluster and file size
		 *
		 * Note:
		 * Long DIRENTs is in Reverse Order.
		 */

		/* Long DIRENT and the last one. OK begin parse it */
		for (i = 0; i < dirent_cnt; i++, pdirent += DIR_ENTRY_SIZE) {
			if (pdirent[0] == 0x0) { /* End of dirent list */
				debug("No dirent\n");
				return -1;
			}
			if (pdirent[0] == 0xe5) /* Deleted item */
				continue;

			/* Long DIRENT and the last one. OK begin parse it */
			if (LFN_DIR_Flag(pdirent) &&
			    LFN_DIR_Attr_Last(pdirent)) {
				do_flag = F_PARSE_LONG;
				memset(lngname, 0, IMG_NAME_MAX_SIZ);
			}

			if ((do_flag == F_PARSE_LONG) &&
			    LFN_DIR_Flag(pdirent)) {
				/* Get file name. Only support ASCII */
				chidx = (LFN_DIR_Attr_Number(pdirent) - 1) * 13;

				/* Part 1 */
				for (j = 0x1; j < 0xB; j += 2) {
					if (pdirent[j] != 0xFF)
						lngname[chidx++] = pdirent[j];
				}
				/* Part 2 */
				for (j = 0xE; j < 0x1A; j += 2) {
					if (pdirent[j] != 0xFF)
						lngname[chidx++] = pdirent[j];
				}
				/* Part 3 */
				for (j = 0x1C; j < 0x20; j += 2) {
					if (pdirent[j] != 0xFF)
						lngname[chidx++] = pdirent[j];
				}
			}

			if ((do_flag == F_PARSE_LONG) &&
			    LFN_DIR_Flag(pdirent) &&
			    (LFN_DIR_Attr_Number(pdirent) == 1)) {
				debug("Find name:%s\n", filename);
				debug("Got  name:%s\n", lngname);
				/* The last Long DIRENT is parsed, need to
				 * compare file name
				 */
				if (!strncmp(filename, lngname,
							strlen(filename)))
					do_flag = F_PARSE_SHORT;
				else
					do_flag = F_PARSE_NONE;
			}

			if ((do_flag == F_PARSE_SHORT) &&
			    (LFN_DIR_Flag(pdirent) == 0) &&
			    (SHT_DIR_Filesize(pdirent) != 0)) {
				/* Usually Short DIRENT following after Long
				 * DIRENT for the same file, so here don't
				 * verify it.
				 */
				*cluster = SHT_DIR_StartCluster(pdirent);
				debug("Start cluster: %d\n", *cluster);

				return 0;
			}
		}
	}

	debug("Search image file failed..\n");
	return -1;
}

static u32 get_fat_sector_offset(u32 cluster)
{
	u32 curmax = 0, offset = 0;

	while (1) {
		curmax += MAX_FAT_ITEM_PER_SECT;
		if (curmax > cluster)
			break;
		offset++;
	}

	return offset;
}

static int update_start_cluster(u32 start_idx, u32 *cstart, void *buf)
{
	u32 cnt = 0, idx = 1, offset = 0, cur, next = 0;
	u32 *pfat;

	/* Reuse BOOTLOADER_BUF_ADDR to read image */
	pfat = (u32 *)buf;

	cur = *cstart;
	while (1) {
		offset = get_fat_sector_offset(cur);
		/* Read FAT sector which cur cluster id located, to find out
		 * next cluster id.
		 */
		cnt = blk_dread(cur_dev, cur_bpb->fat_start + offset, 1,
				(u8 *)pfat);
		if (cnt != 1) {
			debug("Read FAT sector failed.\n");
			return -1;
		}
		next = *(pfat + (cur - (offset * MAX_FAT_ITEM_PER_SECT)));

		/* File data end in this cluster. */
		if (next == 0x0FFFFFFF)
			break;
		if (idx == start_idx) {
			*cstart = next;
			break;
		}
		idx++;
		cur = next;
	}

	return 0;
}

/*
 * Get all cluster ids which image is using.
 *
 * Cluster value:
 *     0x0FFFFFF8, FAT start, cluster 0 use it
 *     0xFFFFFFFF, cluster 1 use it, mark unused
 *     0x0FFFFFFF, File data end in this cluster.
 */
static int get_file_cluster_list(u32 cstart, u32 *clusters, void *buf)
{
	u32 cnt = 0, idx = 1, offset = 0, cur, next = 0;
	u32 *pfat;

	/* Reuse BOOTLOADER_BUF_ADDR to read image */
	pfat = (u32 *)buf;

	cur = cstart;
	clusters[0] = cur;
	while (1) {
		offset = get_fat_sector_offset(cur);
		/* Read FAT sector which cur cluster id located, to find out
		 * next cluster id.
		 */
		cnt = blk_dread(cur_dev, cur_bpb->fat_start + offset, 1,
				(u8 *)pfat);
		if (cnt != 1) {
			debug("Read FAT sector failed.\n");
			return -1;
		}
		next = *(pfat + (cur - (offset * MAX_FAT_ITEM_PER_SECT)));
		if (next == 0x0FFFFFFF)
			break;
		clusters[idx++] = next;
		cur = next;
		if (idx >= IMG_MAX_CLUSTER)
			break;
	}

	return idx;
}

static int aic_fat_read_file(const char *filename, void *buf, ulong offset,
			     ulong maxsize, ulong *actread)
{
	int err = 0;
	u32 sect_off, byte_off = 0;
	u32 cluster_start, clusters[IMG_MAX_CLUSTER];

	if (memcmp("bootcfg.txt", filename, 11) == 0) {
		/* Search bootcfg.txt */
		err = search_boot_cfg_file(&cluster_start, actread, buf);
		if (err != 0) {
			debug("Search bootcfg.txt failed.\n");
			return err;
		}

		/* Get all clusters for bootcfg.txt file. */
		memset(clusters, 0, IMG_MAX_CLUSTER << 2);
		err = get_file_cluster_list(cluster_start, clusters, buf);
		if (err <= 0) {
			debug("Get cluster list failed.\n");
			return err;
		}

		/* Read bootcfg.txt to buffer: buf */
		err = read_file_data(clusters, *actread, 0, 0, buf);
		if (err != 0) {
			debug("Read bootcfg.txt failed.\n");
			return err;
		}
	} else {
		/* SPL start offset should alignment with sector size */
		byte_off = offset % cur_bpb->sector_size;

		/* Change unit from byte to sector, not sect_off is
		 * the SPL start sector offset in image file
		 */
		sect_off = offset / cur_bpb->sector_size;

		/* Search image file */
		err = search_boot_image_file(filename, &cluster_start,
					     actread, buf);
		if (err != 0) {
			debug("Search image file failed.\n");
			return err;
		}

		/* If SPL is not located at the first cluster, need to
		 * update the start cluster.
		 */
		if (sect_off > cur_bpb->cluster_size) {
			err = update_start_cluster(
					sect_off / cur_bpb->cluster_size,
					&cluster_start, buf);
			if (err != 0) {
				debug("Update cluster failed.\n");
				return err;
			}
			/* Update start sector offset (now is in cluster) */
			sect_off = (sect_off % cur_bpb->cluster_size);
		}

		/* Get all clusters for SPL image. */
		memset(clusters, 0, IMG_MAX_CLUSTER << 2);
		err = get_file_cluster_list(cluster_start, clusters, buf);
		if (err <= 0) {
			debug("Get cluster list failed.\n");
			return err;
		}
		err = read_file_data(clusters, maxsize, sect_off,
				     byte_off, buf);
		if (err != 0) {
			debug("Read image data failed.\n");
			return err;
		}
	}

	return err;
}

static ulong spl_fit_read(struct spl_load_info *load, ulong file_offset,
			  ulong size, void *buf)
{
	int ret;
	char *filename = (char *)load->filename;

#ifdef CONFIG_UPDATE_ARTINCHIP_BOOTCFG
	file_offset += (ulong)load->priv;
#endif
	ret = aic_fat_read_file(filename, buf, file_offset, size, NULL);
	if (ret)
		return ret;
	return size;
}

static int load_image_file(void *buf, const char *cfgtxt)
{
	int err = 0;
	struct image_header *header;
	ulong offset;
	ulong actread, maxsize = 0;
	char imgname[IMG_NAME_MAX_SIZ];

	err = aic_fat_read_file(cfgtxt, (u8 *)buf, 0, 1024, &actread);
	if (err != 0) {
		debug("read bootcfg.txt failed!\n");
		return err;
	}

	/* Get boot image file name and SPL start offset (unit is byte) */
	err = boot_cfg_parse_file((u8 *)buf, actread, "boot1", imgname,
				  IMG_NAME_MAX_SIZ, &offset, &maxsize);
	if (err <= 0) {
		debug("Parse boot cfg file failed.\n");
		return err;
	}

	header = buf;
	err = aic_fat_read_file(imgname, header, offset,
				sizeof(struct image_header), &actread);
	if (err != 0) {
		debug("read image header failed!\n");
		return err;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
		image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.read = spl_fit_read;
		load.bl_len = 1;
		load.filename = (void *)imgname;
		load.priv = NULL;
#ifdef CONFIG_UPDATE_ARTINCHIP_BOOTCFG
		load.priv = (void *)offset;
#endif

		return spl_load_simple_fit(cur_image, &load, 0, header);
	} else {
		err = spl_parse_image_header(cur_image, header);
		if (err != 0) {
			debug("Image header parse failed.\n");
			return err;
		}

		err = aic_fat_read_file(imgname,
					(u8 *)(uintptr_t)cur_image->load_addr,
					offset, maxsize, &actread);
		if (err != 0) {
			debug("Read image file failed.\n");
			return err;
		}
	}

	return err;
}

static int load_image_from_raw_volume_fat32(u32 start_lba, u8 *header,
					    const char *filename)
{
	int ret;
	struct fat32_bpb bpb;

	cur_bpb = &bpb;
	cur_bpb->sector_size = BPB_SECT_SIZE(header);
	cur_bpb->cluster_size = BPB_CLUS_SIZE(header);
	cur_bpb->fat_start = start_lba + BPB_RESV_SECT_CNT(header);
	cur_bpb->fat_size = BPB_FAT32_SIZE(header);
	cur_bpb->data_start = cur_bpb->fat_start + cur_bpb->fat_size
						* BPB_FAT_NUM(header);
	cur_bpb->root_dirent_cluster = BPB_ROOT_START_CLUS32(header);

	debug("FAT32 volume start from LBA 0x%x\n", start_lba);
	if (cur_bpb->sector_size != LBA_SIZE) {
		debug("Unsupportted LBA size: 0x%X\n", cur_bpb->sector_size);
		return -1;
	}

	ret = load_image_file(header, filename);
	if (ret) {
		debug("Load image file from FAT32 failed.\n");
		return -1;
	}

	return ret;
}

static int load_image_from_mbr_part_fat32(u8 *header, const char *filename)
{
	struct mbr_dpt_entry part;
	u32 blkcnt = 0;
	int type;

	/* Try the first partition only */
	memcpy(&part, header + DPT_START, sizeof(struct mbr_dpt_entry));

	blkcnt = blk_dread(cur_dev, part.lba_start, 1, header);
	if (blkcnt != 1) {
		debug("Read partition first sector failed.\n");
		return -1;
	}

	type = check_identifier(header);
	if (type != TYPE_RAW_VOLUME_FAT32) {
		debug("Partition is not FAT32.\n");
		return -1;
	}
	return load_image_from_raw_volume_fat32(part.lba_start,	header,
						filename);
}

static int load_image_from_gpt_part_fat32(u8 *header, const char *filename)
{
	struct gpt_header gpthead;
	struct gpt_entry gptent;
	u32 blkcnt = 0;
	int type;

	/* Read LBA1 for GPT header */
	blkcnt = blk_dread(cur_dev, 1, 1, header);
	if (blkcnt != 1) {
		debug("Read LBA1 failed.\n");
		return -1;
	}

	memcpy(&gpthead, header, sizeof(struct gpt_header));
	if (strncmp((char *)gpthead.signature, "EFI PART", 8)) {
		debug("LBA1 is not GPT header.\n");
		return -1;
	}

	/* Read the first partition entry, only check the first partition */
	blkcnt = blk_dread(cur_dev, gpthead.partition_entry_lba, 1, header);
	if (blkcnt != 1) {
		debug("Read the first partition entry failed.\n");
		return -1;
	}
	memcpy(&gptent, header, sizeof(struct gpt_entry));

	/* Read first LBA of the first GPT partition */
	blkcnt = blk_dread(cur_dev, gptent.starting_lba, 1, header);
	if (blkcnt != 1) {
		debug("Read the first partition entry failed.\n");
		return -1;
	}

	type = check_identifier(header);
	if (type != TYPE_RAW_VOLUME_FAT32) {
		debug("Partition is not FAT32.\n");
		return -1;
	}
	return load_image_from_raw_volume_fat32(gptent.starting_lba, header,
						filename);
}

int aic_spl_load_image_fat(struct spl_image_info *spl_image,
			   struct blk_desc *block_dev, const char *filename)
{
	int ret = 0, type;
	u32 blkcnt = 0;
	u8 *header;

	cur_image = spl_image;
	cur_dev = block_dev;

	debug("%s:%d\n", __func__, __LINE__);
	header = (u8 *)BOOTLOADER_BUF_ADDR;
	blkcnt = blk_dread(cur_dev, 0, 1, header);
	if (blkcnt != 1) {
		debug("IO error, cann't read data from SD.\n");
		return -1;
	}

	type = check_identifier(header);
	switch (type) {
	case TYPE_RAW_VOLUME_FAT32:
		debug("TYPE_RAW_VOLUME_FAT32\n");
		/* There is no partition table in SD card, just FAT32 FS */
		ret = load_image_from_raw_volume_fat32(0, header, filename);
		break;
	case TYPE_MBR_PART_FAT32:
		debug("TYPE_MBR_PART_FAT32\n");
		/* MBR partition exist, try to boot from the first partition */
		ret = load_image_from_mbr_part_fat32(header, filename);
		break;
	case TYPE_GPT_PART_FAT32:
		debug("TYPE_GPT_PART_FAT32\n");
		/* GPT partition exist, try to boot from the first partition */
		ret = load_image_from_gpt_part_fat32(header, filename);
		break;
	default:
		debug("No FAT32.\n");
		return -1;
	}

	return ret;
}
