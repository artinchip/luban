// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#include <common.h>
#include <artinchip/aicupg.h>
#include <dm/uclass.h>
#include <env.h>
#include <mtd.h>
#include <nand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <ubi_uboot.h>
#include "nand_fwc_priv.h"
#include "upg_internal.h"
#include "nand_fwc_spl.h"

#ifdef CONFIG_NAND_BBT_MANAGE
#include <linux/mtd/spinand.h>
#endif

#define MAX_NAND_NAME		32
static bool nand_is_prepared;

struct aicupg_mtd_partition {
	char name[MAX_NAND_NAME];
	u64 size;
	struct aicupg_mtd_partition *next;
};

struct aicupg_nand_dev {
	char name[MAX_NAND_NAME];
	struct aicupg_nand_dev *next;
	struct aicupg_mtd_partition *parts;
};

struct aicupg_ubi_volume {
	char name[MAX_NAND_NAME];
	int vol_type;
	u64 size;
	struct aicupg_ubi_volume *next;
};

struct aicupg_ubi_mtd {
	char name[MAX_NAND_NAME];
	struct aicupg_ubi_mtd *next;
	struct aicupg_ubi_volume *vols;
};

static s32 nand_fwc_ubi_read(struct fwc_info *fwc, u8 *buf, s32 len);
static s32 nand_fwc_mtd_read(struct fwc_info *fwc, u8 *buf, s32 len);

static struct aicupg_mtd_partition *new_partition(char *s)
{
	struct aicupg_mtd_partition *part;
	int cnt = 0;
	char *p;

	part = malloc(sizeof(struct aicupg_mtd_partition));
	if (!part)
		return NULL;

	memset(part, 0, sizeof(struct aicupg_mtd_partition));

	p = s;
	if (*p == '-') {
		/* All remain space */
		part->size = 0;
		p++;
	} else {
		part->size = ustrtoull(p, &p, 0);
	}
	if (*p == '@') {
		p++;
		/* Don't care offset here, just skip it */
		ustrtoull(p, &p, 0);
	}
	if (*p != '(') {
		pr_err("Partition name should be next of size.\n");
		goto err;
	}
	p++;

	cnt = 0;
	while (*p != ')') {
		if (cnt >= MAX_NAND_NAME)
			break;
		part->name[cnt++] = *p++;
	}
	p++;

	/* Skip characters until '\0', ',', ';' */
	while ((*p != '\0') && (*p != ';') && (*p != ','))
		p++;

	if (*p == ',') {
		p++;
		part->next = new_partition(p);
	}

	return part;
err:
	if (part)
		free(part);
	return NULL;
}

static void free_partition(struct aicupg_mtd_partition *part)
{
	if (!part)
		return;

	free_partition(part->next);
	free(part);
}

static struct aicupg_ubi_volume *new_volume(char *s)
{
	struct aicupg_ubi_volume *vol = NULL;
	int cnt = 0;
	char *p;

	vol = malloc(sizeof(struct aicupg_ubi_volume));
	if (!vol)
		return NULL;

	memset(vol, 0, sizeof(struct aicupg_ubi_volume));

	p = s;
	if (*p == '-') {
		/* All remain space */
		vol->size = 0;
		p++;
	} else {
		vol->size = ustrtoull(p, &p, 0);
	}
	if (*p == '@') {
		p++;
		/* Don't care offset here, just skip it */
		ustrtoull(p, &p, 0);
	}
	if (*p != '(') {
		pr_err("Volume name should be next of size.\n");
		goto err;
	}
	p++;

	cnt = 0;
	while (*p != ')') {
		if (cnt >= MAX_NAND_NAME)
			break;
		vol->name[cnt++] = *p++;
	}
	p++;

	vol->vol_type = 1; /* Default is dynamic */
	if (*p == 's')
		vol->vol_type = 0; /* Static volume */

	/* Skip characters until '\0', ',', ';' */
	while ((*p != '\0') && (*p != ';') && (*p != ','))
		p++;

	if (*p == ',') {
		p++;
		vol->next = new_volume(p);
	}

	debug("ubi vol: %s, size %lld\n", vol->name, vol->size);
	return vol;
err:
	if (vol)
		free(vol);
	return NULL;
}

static void free_volume(struct aicupg_ubi_volume *vol)
{
	if (!vol)
		return;

	free_volume(vol->next);
	free(vol);
}

static void free_mtd_list(struct aicupg_nand_dev *dev)
{
	if (!dev)
		return;

	free_mtd_list(dev->next);
	free_partition(dev->parts);
	free(dev);
}

static struct aicupg_nand_dev *build_mtd_list(char *mtdparts)
{
	struct aicupg_nand_dev *head, *cur, *nxt;
	char *p;
	int cnt;

	head = NULL;
	cur = NULL;
	p = mtdparts;

next:
	nxt = malloc(sizeof(struct aicupg_nand_dev));
	if (!nxt)
		goto err;
	memset(nxt, 0, sizeof(struct aicupg_nand_dev));

	if (cur)
		cur->next = nxt;
	cur = nxt;
	if (!head)
		head = cur;

	cnt = 0;
	while (*p != '\0' && *p != ':') {
		if (cnt >= MAX_NAND_NAME)
			break;
		cur->name[cnt++] = *p++;
	}

	if (*p != ':') {
		pr_err("partition table is invalid\n");
		goto err;
	}
	p++;

	cur->parts = new_partition(p);
	if (!cur->parts) {
		pr_err("Build mtd partition list failed.\n");
		goto err;
	}

	/* Check if there is other mtd id */
	while (*p != '\0' && *p != ';')
		p++;

	if (*p == ';') {
		p++;
		goto next;
	}

	return head;
err:
	if (head)
		free_mtd_list(head);
	return NULL;
}

static void free_ubi_list(struct aicupg_ubi_mtd *ubi)
{
	if (!ubi)
		return;

	free_ubi_list(ubi->next);
	free_volume(ubi->vols);
	free(ubi);
}

static struct aicupg_ubi_mtd *build_ubi_list(char *ubivols)
{
	struct aicupg_ubi_mtd *head, *cur, *nxt;
	char *p;
	int cnt;

	head = NULL;
	cur = NULL;
	p = ubivols;

next:
	nxt = malloc(sizeof(struct aicupg_ubi_mtd));
	if (!nxt)
		goto err;
	memset(nxt, 0, sizeof(struct aicupg_ubi_mtd));

	if (cur)
		cur->next = nxt;
	cur = nxt;
	if (!head)
		head = cur;

	cnt = 0;
	while (*p != '\0' && *p != ':') {
		if (cnt >= MAX_NAND_NAME)
			break;
		cur->name[cnt++] = *p++;
	}

	if (*p != ':') {
		pr_err("ubivols_nand is invalid\n");
		goto err;
	}
	p++;

	cur->vols = new_volume(p);
	if (!cur->vols) {
		pr_err("Build ubi volume list failed.\n");
		goto err;
	}

	/* Check if there is other ubi device */
	while (*p != '\0' && *p != ';')
		p++;

	if (*p == ';') {
		p++;
		goto next;
	}

	return head;
err:
	if (head)
		free_ubi_list(head);
	return NULL;
}

static bool ubi_mtd_is_exist(char *mtd_name, struct aicupg_nand_dev *mtd_list)
{
	struct aicupg_mtd_partition *part;
	struct aicupg_nand_dev *nand_dev;

	nand_dev = mtd_list;
	while (nand_dev) {
		part = nand_dev->parts;
		while (part) {
			if (strcmp(mtd_name, part->name) == 0)
				return true;
			part = part->next;
		}

		nand_dev = nand_dev->next;
	}
	pr_err("Partition %s is not exist.....\n", mtd_name);
	return false;
}

/*
 * Check if there is MTD partition for UBI device
 */
static bool ubi_mtd_valid_check(struct aicupg_ubi_mtd *ubi_list,
				struct aicupg_nand_dev *mtd_list)
{
	struct aicupg_ubi_mtd *ubi_dev;

	ubi_dev = ubi_list;
	while (ubi_dev) {
		if (!ubi_mtd_is_exist(ubi_dev->name, mtd_list)) {
			pr_err("Partition %s is not exist!\n", ubi_dev->name);
			return false;
		}

		ubi_dev = ubi_dev->next;
	}

	return true;
}

static s32 ubi_device_is_attached(char *mtdname)
{
	struct ubi_device *ubi;
	int i, attached;

	attached = false;
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		ubi = ubi_get_device(i);
		if (!ubi)
			continue;
		if (strcmp(mtdname, ubi->mtd->name) == 0) {
			attached = true;
			ubi_put_device(ubi);
			break;
		}
		ubi_put_device(ubi);
	}

	return attached;
}

static s32 do_mtd_erase(struct mtd_info *mtd)
{
	struct erase_info erase_op = {};
	s32 ret;

	erase_op.mtd = mtd;
	erase_op.addr = 0;
	erase_op.len = mtd->size;
	erase_op.scrub = false;

	ret = 0;
	while (erase_op.len) {
		ret = mtd_erase(mtd, &erase_op);

		/* Abort if its not a bad block error */
		if (ret != -EIO)
			break;

		pr_info("Skipping bad block at 0x%08llx\n", erase_op.fail_addr);

		ret = 0;
		/* Skip bad block and continue behind it */
		erase_op.len -= erase_op.fail_addr - erase_op.addr;
		erase_op.len -= mtd->erasesize;
		erase_op.addr = erase_op.fail_addr + mtd->erasesize;
	}

	return ret;
}

static s32 ubi_attach_mtd(char *name)
{
	struct mtd_info *mtd;
	s32 ret;

	mtd = get_mtd_device_nm(name);
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("MTD device %s not found, ret %ld\n", name,
		       PTR_ERR(mtd));
		return PTR_ERR(mtd);
	}
	ret = do_mtd_erase(mtd);
	if (ret) {
		pr_err("Do mtd erase failed.\n");
		goto out;
	}
	ret = ubi_attach_mtd_dev(mtd, UBI_DEV_NUM_AUTO, 0, 0);
	if (ret < 0)
		pr_err("Attach mtd %s failed. ret = %d\n", name, ret);
	else
		ret = 0;

out:
	put_mtd_device(mtd);
	return ret;
}

static struct ubi_device *ubi_get_device_by_name(char *name)
{
	struct ubi_device *ubi;
	int i;

	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		ubi = ubi_get_device(i);
		if (!ubi)
			continue;
		if (strcmp(name, ubi->mtd->name) == 0)
			return ubi;
		ubi_put_device(ubi);
	}
	return NULL;
}

static struct ubi_volume *ubi_get_volume_by_name(struct ubi_device *ubi,
						 char *name)
{
	struct ubi_volume *vol = NULL;
	int i;

	for (i = 0; i < ubi->vtbl_slots; i++) {
		vol = ubi->volumes[i];
		if (vol && !strcmp(vol->name, name))
			return vol;
	}

	pr_err("Volume %s not found!\n", name);
	return NULL;
}

static int verify_mkvol_req(const struct ubi_device *ubi,
			    const struct ubi_mkvol_req *req)
{
	int n, err = -EINVAL;

	if (req->bytes < 0 || req->alignment < 0 || req->vol_type < 0 ||
	    req->name_len < 0)
		goto bad;

	if ((req->vol_id < 0 || req->vol_id >= ubi->vtbl_slots) &&
	    req->vol_id != UBI_VOL_NUM_AUTO)
		goto bad;

	if (req->alignment == 0)
		goto bad;

	if (req->bytes == 0) {
		printf("No space left in UBI device!\n");
		err = -ENOMEM;
		goto bad;
	}

	if (req->vol_type != UBI_DYNAMIC_VOLUME &&
	    req->vol_type != UBI_STATIC_VOLUME)
		goto bad;

	if (req->alignment > ubi->leb_size)
		goto bad;

	n = req->alignment % ubi->min_io_size;
	if (req->alignment != 1 && n)
		goto bad;

	if (req->name_len > UBI_VOL_NAME_MAX) {
		printf("Name too long!\n");
		err = -ENAMETOOLONG;
		goto bad;
	}

	return 0;
bad:
	return err;
}

static int ubi_create_vol(struct ubi_device *ubi, char *volume, int64_t size,
			  int dynamic, int vol_id)
{
	struct ubi_mkvol_req req;
	int err;

	pr_info("%s, ubi %s, %s, type %d, %llu\n", __func__, ubi->ubi_name,
	       volume, dynamic, size);
	if (dynamic)
		req.vol_type = UBI_DYNAMIC_VOLUME;
	else
		req.vol_type = UBI_STATIC_VOLUME;

	req.vol_id = vol_id;
	req.alignment = 1;
	req.bytes = size;

	strcpy(req.name, volume);
	req.name_len = strlen(volume);
	req.name[req.name_len] = '\0';
	req.flags = 0;
	/* It's duplicated at drivers/mtd/ubi/cdev.c */
	err = verify_mkvol_req(ubi, &req);
	if (err) {
		pr_err("verify_mkvol_req failed %d\n", err);
		return err;
	}
	pr_info("Creating %s volume %s of size %lld\n",
		dynamic ? "dynamic" : "static", volume, size);
	/* Call real ubi create volume */
	return ubi_create_volume(ubi, &req);
}

static s32 create_ubi_volumes(struct ubi_device *ubidev,
			      struct aicupg_ubi_volume *vols)
{
	struct aicupg_ubi_volume *aicvol;
	s32 ret = 0;

	aicvol = vols;
	while (aicvol) {
		if (!aicvol->size) {
			aicvol->size = ubidev->avail_pebs * ubidev->leb_size;
			printf("No size specified -> Using max size (%lld)\n",
			       aicvol->size);
		}
		ret = ubi_create_vol(ubidev, aicvol->name, aicvol->size,
				     aicvol->vol_type, UBI_VOL_NUM_AUTO);
		if (ret) {
			pr_err("Create volume %s failed.\n", aicvol->name);
			return ret;
		}
		aicvol = aicvol->next;
	}

	return ret;
}

static s32 prepare_ubi_volumes(char *mtdname)
{
	struct aicupg_ubi_mtd *ubi = NULL;
	struct ubi_device *ubidev = NULL;
	struct aicupg_ubi_mtd *ubi_list;
	char *ubivols;
	s32 ret = -1;

	ubivols = env_get("UBI");
	if (!ubivols) {
		pr_err("Get UBI volume table from ENV failed.\n");
		return -ENODEV;
	}
	ubi_list = build_ubi_list(ubivols);
	if (!ubi_list) {
		pr_err("Parse ubivols error.\n");
		return -1;
	}
	ubi = ubi_list;

	while (ubi) {
		if (strcmp(mtdname, ubi->name)) {
			ubi = ubi->next;
			continue;
		}
		if (ubi_device_is_attached(ubi->name)) {
			ubi = ubi->next;
			continue;
		}

		ret = ubi_attach_mtd(ubi->name);
		if (ret) {
			pr_err("UBI attach mtd %s failed.\n", ubi->name);
			return ret;
		}
		ubidev = ubi_get_device_by_name(ubi->name);
		if (!ubidev) {
			pr_err("Get UBI device failed.\n");
			return -1;
		}

		ret = create_ubi_volumes(ubidev, ubi->vols);
		if (ret)
			goto out_put_dev;

		ubi_put_device(ubidev);
		ubidev = NULL;
		ubi = ubi->next;
	}

out_put_dev:
	if (ubidev)
		ubi_put_device(ubidev);
	if (ubi_list)
		free_ubi_list(ubi_list);
	return ret;
}

static int replace_first_str(char *src, char *match_str, char *replace_str)
{
	int str_len;
	char newstring[PARTITION_TABLE_LEN] = {0};
	char *find_pos;

	find_pos = strstr(src, match_str);
	if (!find_pos)
		return -1;

	while (find_pos) {
		str_len = find_pos - src;
		strncpy(newstring, src, str_len);
		strcat(newstring, replace_str);
		strcat(newstring, find_pos + strlen(match_str));
		strcpy(src, newstring);
		return 0;
	}

	return 0;
}

char *mtd_ubi_env_get(void)
{
	struct aicupg_ubi_mtd *ubi_list = NULL;
	struct aicupg_nand_dev *mtd_list = NULL;
	char *ubivols, *mtdparts, *part_table;
	char replace_str[PARTITION_TABLE_LEN] = {0};

	mtdparts = env_get("MTD");
	if (!mtdparts) {
		pr_err("Get MTD partition table from ENV failed.\n");
		return NULL;
	}
	env_set("mtdparts", mtdparts);

	ubivols = env_get("UBI");
	if (!ubivols) {
		pr_err("Get UBI volume table from ENV failed.\n");
		return NULL;
	}

	mtd_list = build_mtd_list(mtdparts);
	if (!mtd_list) {
		pr_err("Parse mtdparts error.\n");
		return NULL;
	}

	ubi_list = build_ubi_list(ubivols);
	if (!ubi_list) {
		pr_err("Parse ubivols error.\n");
		return NULL;
	}

	part_table = (char *)malloc(PARTITION_TABLE_LEN);
	if (!part_table) {
		pr_err("Error: malloc buffer failed.\n");
		return NULL;
	}
	memset(part_table, 0, PARTITION_TABLE_LEN);
	strcpy(part_table, mtdparts);
	while (ubi_list) {
		sprintf(replace_str, "%s:%s", ubi_list->name,
						ubi_list->vols->name);
		if (strstr(mtdparts, ubi_list->name)) {
			replace_first_str(part_table, ubi_list->name,
					replace_str);
		}
		ubi_list = ubi_list->next;
	}
	if (mtd_list)
		free_mtd_list(mtd_list);
	if (ubi_list)
		free_ubi_list(ubi_list);

	return part_table;
}

s32 nand_is_exist(void)
{
	struct aicupg_ubi_mtd *ubi_list = NULL;
	struct aicupg_nand_dev *mtd_list = NULL;
	char *ubivols, *mtdparts;

	mtdparts = env_get("MTD");
	if (!mtdparts) {
		pr_err("Get MTD partition table from ENV failed.\n");
		return false;
	}
	env_set("mtdparts", mtdparts);

	ubivols = env_get("UBI");
	if (!ubivols) {
		pr_err("Get UBI volume table from ENV failed.\n");
		return false;
	}

	mtd_list = build_mtd_list(mtdparts);
	if (!mtd_list) {
		pr_err("Parse mtdparts error.\n");
		return false;
	}

	ubi_list = build_ubi_list(ubivols);
	if (!ubi_list) {
		pr_err("Parse ubivols error.\n");
		return false;
	}

	if (!ubi_mtd_valid_check(ubi_list, mtd_list))
		return false;

	if (mtd_list)
		free_mtd_list(mtd_list);
	if (ubi_list)
		free_ubi_list(ubi_list);
	return true;
}

void set_nand_prepare_status(bool value)
{
	nand_is_prepared = value;
}

bool get_nand_prepare_status(void)
{
	return nand_is_prepared;
}

s32 nand_fwc_prepare(struct fwc_info *fwc, u32 id)
{
	struct aicupg_ubi_mtd *ubi_list = NULL;
	struct aicupg_nand_dev *mtd_list = NULL;
	char *ubivols, *mtdparts;
	s32 ret;

	ret = 0;
	mtdparts = env_get("MTD");
	if (!mtdparts) {
		pr_err("Get MTD partition table from ENV failed.\n");
		return -ENODEV;
	}
	env_set("mtdparts", mtdparts);

	ubivols = env_get("UBI");
	if (!ubivols) {
		pr_err("Get UBI volume table from ENV failed.\n");
		return -ENODEV;
	}

	mtd_list = build_mtd_list(mtdparts);
	if (!mtd_list) {
		pr_err("Parse mtdparts error.\n");
		return -1;
	}

	ubi_list = build_ubi_list(ubivols);
	if (!ubi_list) {
		pr_err("Parse ubivols error.\n");
		return -1;
	}

	if (!ubi_mtd_valid_check(ubi_list, mtd_list)) {
		ret = -1;
		goto out;
	}

#ifdef CONFIG_NAND_BBT_MANAGE
	aic_clear_nand_bbt();
#endif

	ret = mtd_probe_devices();
	if (ret) {
		pr_err("mtd probe partitions failed.\n");
		goto out;
	}

	/* Ensure there is no attach UBI */
	ubi_exit();
	ubi_init();

	set_nand_prepare_status(true);
out:
	if (mtd_list)
		free_mtd_list(mtd_list);
	if (ubi_list)
		free_ubi_list(ubi_list);
	return ret;
}

static s32 nand_fwc_get_mtd_partitions(struct fwc_info *fwc,
				       struct aicupg_nand_priv *priv)
{
	char name[MAX_NAND_NAME], *p;
	int cnt, idx;
	struct mtd_info *mtd;

	p = fwc->meta.partition;

	cnt = 0;
	idx = 0;
	while (*p) {
		if (cnt >= MAX_NAND_NAME) {
			pr_err("Partition name is too long.\n");
			return -1;
		}

		name[cnt] = *p;
		p++;
		cnt++;
		if (*p == ';' || *p == ':' || *p == '\0') {
			name[cnt] = '\0';
			mtd = get_mtd_device_nm(name);
			if (IS_ERR_OR_NULL(mtd)) {
				pr_err("Get mtd %s failed.\n", name);
				return -1;
			}
			priv->mtds[idx] = mtd;
			idx++;
			cnt = 0;
		}
		if (*p == ':') {
			while (*p != ';' && *p != '\0')
				p++;
		}
		if (*p == ';')
			p++;
		if (*p == '\0')
			break;
	}

	return 0;
}

static s32 nand_fwc_get_ubi_volumes(struct fwc_info *fwc,
				    struct aicupg_nand_priv *priv)
{
	char name[MAX_NAND_NAME * 2], *p, *ubiname, *volname;
	struct ubi_device *ubi;
	int cnt, idx;

	p = fwc->meta.partition;

	cnt = 0;
	idx = 0;
	while (*p) {
		if (cnt >= (MAX_NAND_NAME * 2)) {
			pr_err("Name is too long.\n");
			return -1;
		}

		name[cnt] = *p;
		cnt++;
		p++;
		if (*p == ';' || *p == '\0') {
			name[cnt] = '\0';

			ubiname = name;
			volname = name;
			while (*volname != ':' && *volname != '\0')
				volname++;
			if (*volname != ':') {
				pr_err("Volume name not found.\n");
				return -1;
			}
			*volname = '\0';
			volname++;

			ubi = ubi_get_device_by_name(ubiname);
			if (!ubi) {
				pr_err("UBI %s is not found.\n", ubiname);
				return -1;
			}
			priv->vols[idx] = ubi_get_volume_by_name(ubi, volname);
			if (!priv->vols[idx])
				return -1;
			idx++;
			cnt = 0;
		}
		if (*p == ';')
			p++;
		if (*p == '\0')
			break;
	}

	return 0;
}

/*
 * New FirmWare Component start, should prepare to burn FWC to NAND
 *  - Get FWC attributes
 *  - Parse MTD partitions or UBI Volumes the FWC going to upgrade
 *  - Erase MTD partitions(SPL is special, no need to erase here)
 */
void nand_fwc_start(struct fwc_info *fwc)
{
	struct ubi_volume *vol;
	struct mtd_info *mtd;
	struct aicupg_nand_priv *priv;
	int attr, ret, i;

	priv = malloc(sizeof(struct  aicupg_nand_priv));
	if (!priv) {
		pr_err("Out of memory, malloc failed.\n");
		goto err;
	}
	memset(priv, 0, sizeof(struct aicupg_nand_priv));
	fwc->priv = priv;

	/*
	 * If the meta.name contains the "image" string, the program is going to
	 * write data to the partition, otherwise the the program is going to read
	 * data from the partition
	 */
	if (memcmp(fwc->meta.name, "image", 5) == 0)
		pr_debug("NAND FWC %s to %s\n", fwc->meta.name, fwc->meta.partition);
	else
		pr_debug("Read %s partition from NAND FWC\n", fwc->mpart.part.name);

	attr = aicupg_get_fwc_attr(fwc);
	if (attr & FWC_ATTR_DEV_MTD) {
		ret = nand_fwc_get_mtd_partitions(fwc, priv);
		if (ret) {
			pr_err("Get MTD partitions failed.\n");
			goto err;
		}
		mtd = priv->mtds[0];
		if (IS_ERR_OR_NULL(mtd)) {
			pr_err("MTD device is not found.\n");
			goto err;
		}
		fwc->block_size = mtd->writesize;
		if (strstr(fwc->meta.name, "target.spl")) {
			ret = nand_fwc_spl_prepare(fwc);
			if (ret) {
				pr_err("Prepare to write SPL failed.\n");
				goto err;
			}
			priv->spl_flag = true;
		/*
		 * Erase partition data if the meta.name contains the
		 * "image.target" string
		 */
		} else if (strstr(fwc->meta.name, "image.target")) {
			/* Erase MTD partitions */
			for (i = 0; i < MAX_DUPLICATED_PART; i++) {
				if (!priv->mtds[i])
					continue;
				pr_debug("Erase MTD part %s\n",
					 priv->mtds[i]->name);
				do_mtd_erase(priv->mtds[i]);
			}
		}
	} else if (attr & FWC_ATTR_DEV_UBI) {
		ret = nand_fwc_get_mtd_partitions(fwc, priv);
		if (ret) {
			pr_err("Get MTD partitions failed.\n");
			goto err;
		}
		for (i = 0; i < MAX_DUPLICATED_PART; i++) {
			if (!priv->mtds[i])
				continue;
			if (i && (priv->mtds[i] == priv->mtds[0])) {
				/* Point to the same partition, skip it */
				priv->mtds[i] = NULL;
				continue;
			}
			prepare_ubi_volumes(priv->mtds[i]->name);
		}
		ret = nand_fwc_get_ubi_volumes(fwc, priv);
		if (ret) {
			pr_err("Get UBI Volumes failed.\n");
			goto err;
		}
		if (!priv->vols[0]) {
			pr_err("No UBI volume is found.\n");
			goto err;
		}
		fwc->block_size = priv->vols[0]->ubi->leb_size;

		if (strstr(fwc->meta.name, "target")) {
			/* Mark start UBI volume update */
			for (i = 0; i < MAX_DUPLICATED_PART; i++) {
				if (!priv->vols[i])
					continue;
				vol = priv->vols[i];
				/* Setup the total update size */
				ret = ubi_start_update(vol->ubi, vol, fwc->meta.size);
				if (ret) {
					pr_err("Mark volume start update failed.\n");
					goto err;
				}
			}
		}
	} else {
		pr_err("FWC attribute for NAND should be 'mtd' or 'ubi'.\n");
		goto err;
	}

	priv->attr = attr;

	return;
err:
	for (i = 0; i < MAX_DUPLICATED_PART; i++) {
		if (priv->mtds[i]) {
			put_mtd_device(priv->mtds[i]);
			priv->mtds[i] = NULL;
		}
		if (priv->vols[i])
			priv->vols[i] = NULL;
	}
	if (priv)
		free(priv);
	fwc->priv = NULL;
}

static s32 nand_fwc_mtd_writer(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_priv *priv;
	struct mtd_info *mtd;
	size_t retlen;
	loff_t offs;
	s32 ret, i;
	s32 write_end_addr, calc_len;
	u8 *buf_to_write, *buf_to_read, *rdbuf;

	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv)
		return 0;

	rdbuf = malloc(len);
	if (!rdbuf) {
		pr_err("Error: malloc buffer failed.\n");
		return 0;
	}
	for (i = 0; i < MAX_DUPLICATED_PART; i++) {
		mtd = priv->mtds[i];
		if (!mtd)
			continue;
		buf_to_write = buf;
		buf_to_read = rdbuf;
		offs = priv->offs[i];
		write_end_addr = len + priv->offs[i];

		while (offs < write_end_addr) {
			while (mtd_block_isbad(mtd, offs)) {
				offs += mtd->erasesize;
				write_end_addr += mtd->erasesize;
			}

			if (offs >= mtd->size) {
				pr_err("Not enough space to write mtd %s\n", mtd->name);
				return 0;
			}
			ret = mtd_write(mtd, offs, mtd->writesize, &retlen, buf_to_write);
			if (ret) {
				pr_err("Write mtd %s error.\n", mtd->name);
				return 0;
			}
			// Read data to calc crc
			ret = mtd_read(mtd, offs, mtd->writesize, &retlen,
								buf_to_read);
			if (ret) {
				pr_err("Write mtd %s error.\n", mtd->name);
				return 0;
			}
			/* Update for next write */
			buf_to_write += retlen;
			buf_to_read += retlen;
			priv->offs[i] = offs + retlen;
			offs = priv->offs[i];
		}
	}

	if ((fwc->meta.size - fwc->trans_size) < len)
		calc_len = fwc->meta.size - fwc->trans_size;
	else
		calc_len = len;

	fwc->calc_partition_crc = crc32(fwc->calc_partition_crc,
						rdbuf, calc_len);
#ifdef CONFIG_AICUPG_SINGLE_TRANS_BURN_CRC32_VERIFY
	fwc->calc_trans_crc = crc32(fwc->calc_trans_crc, buf, calc_len);
	if (fwc->calc_trans_crc != fwc->calc_partition_crc) {
		pr_err("calc_len:%d\n", calc_len);
		pr_err("crc err at trans len %u\n", fwc->trans_size);
		pr_err("trans crc:0x%x, partition crc:0x%x\n",
				fwc->calc_trans_crc, fwc->calc_partition_crc);
	}
#endif
	debug("%s, data len %d, trans len %d, calc len %d\n", __func__, len,
	      fwc->trans_size, calc_len);

	free(rdbuf);
	return len;
}

static s32 nand_fwc_ubi_writer(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_priv *priv;
	struct ubi_volume *vol;
	s32 ret, i;

	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv)
		return 0;
	for (i = 0; i < MAX_DUPLICATED_PART; i++) {
		if (!priv->vols[i])
			continue;
		vol = priv->vols[i];
		ret = ubi_more_update_data(vol->ubi, vol, buf, len);
		if (ret < 0) {
			pr_err("Couldnt or partially wrote data\n");
			return 0;
		}
	}

	fwc->calc_partition_crc = fwc->meta.crc; /* TODO: need to calc CRC */
	return len;
}

s32 nand_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_priv *priv;

	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv)
		return 0;
	if (priv->attr & FWC_ATTR_DEV_MTD) {
		if (priv->spl_flag)
			len = nand_fwc_spl_writer(fwc, buf, len);
		else
			len = nand_fwc_mtd_writer(fwc, buf, len);
	} else if (priv->attr & FWC_ATTR_DEV_UBI) {
		len = nand_fwc_ubi_writer(fwc, buf, len);
	} else {
		/* Do nothing */
	}

	if (len < 0) {
		len = 0;
		fwc->trans_size += len;
		fwc->calc_partition_crc = 0;
	} else {
		fwc->trans_size += len;
	}
	pr_debug("%s, data len %d, trans len %d\n", __func__, len,
		 fwc->trans_size);

	return len;
}

static s32 nand_fwc_mtd_read(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_priv *priv;
	struct mtd_info *mtd;
	size_t retlen;
	loff_t offs;
	s32 ret;
	s32 read_end_addr;
	u8 *buf_to_read;

	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv)
		return 0;
	mtd = priv->mtds[0];
	if (!mtd)
		return 0;

	buf_to_read = buf;
	offs = priv->offs[0];
	read_end_addr = len + priv->offs[0];

	while (offs < read_end_addr) {
		while (mtd_block_isbad(mtd, offs)) {
			offs += mtd->erasesize;
			read_end_addr += mtd->erasesize;
		}

		if (offs >= mtd->size) {
			pr_err("Not enough space to read mtd %s\n", mtd->name);
			return 0;
		}
		ret = mtd_read(mtd, offs, mtd->writesize, &retlen, buf_to_read);
		if (ret) {
			pr_err("Write mtd %s error.\n", mtd->name);
			return 0;
		}
		/* Update for next write */
		buf_to_read += retlen;
		priv->offs[0] = offs + retlen;
		offs = priv->offs[0];
	}

	return len;
}

static s32 nand_fwc_ubi_read(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_priv *priv;
	struct ubi_volume *vol;
	unsigned long long tmp;
	loff_t offs;
	s32 ret, vol_leb_num, buf_to_read_size;
	size_t size, rdlen;
	u8 *buf_to_read;

	size = len;
	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv->vols[0])
		return 0;

	buf_to_read = buf;
	vol = priv->vols[0];
	tmp = priv->offs[0];
	offs = do_div(tmp, vol->usable_leb_size);
	vol_leb_num = tmp;
	if (priv->offs[0] + size > vol->used_bytes)
		size = vol->used_bytes - priv->offs[0];

	buf_to_read_size = vol->usable_leb_size;
	if (size < buf_to_read_size)
		buf_to_read_size = ALIGN(size, vol->ubi->min_io_size);

	rdlen = size > buf_to_read_size ? buf_to_read_size : size;
	printf("Read %zu bytes from volume %s to %p\n",
			size, vol->name, buf_to_read);

	while (size > 0) {
		if (offs + rdlen >= vol->usable_leb_size)
			rdlen = vol->usable_leb_size - offs;
		ret = ubi_eba_read_leb(vol->ubi, vol, vol_leb_num, buf_to_read,
								offs, rdlen, 1);
		if (ret) {
			pr_err("read err %d\n", ret);
			return 0;
		}
		priv->offs[0] += rdlen;
		if (offs == vol->usable_leb_size) {
			priv->vol_leb_num[0] += 1;
			priv->offs[0] -= vol->usable_leb_size;
		}
		size -= rdlen;
		buf_to_read += rdlen;
		rdlen = size > buf_to_read_size ? buf_to_read_size : size;
	}

	return len;
}

s32 nand_fwc_data_read(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_priv *priv;

	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv)
		return 0;
	if (priv->attr & FWC_ATTR_DEV_MTD) {
		len = nand_fwc_mtd_read(fwc, buf, len);
	} else if (priv->attr & FWC_ATTR_DEV_UBI) {
		len = nand_fwc_ubi_read(fwc, buf, len);
	} else {
		pr_info("no data read\n");
	}

	if (len < 0) {
		len = 0;
		fwc->trans_size += len;
		fwc->calc_partition_crc = 0;
	} else {
		fwc->trans_size += len;
		fwc->calc_partition_crc = fwc->meta.crc;
	}
	pr_debug("%s, data len %d, trans len %d\n", __func__, len,
		 fwc->trans_size);

	return len;
}

void nand_fwc_data_end(struct fwc_info *fwc)
{
	struct aicupg_nand_priv *priv;
	int i;

	priv = (struct aicupg_nand_priv *)fwc->priv;
	if (!priv)
		return;

	for (i = 0; i < MAX_DUPLICATED_PART; i++) {
		if (priv->mtds[i]) {
			put_mtd_device(priv->mtds[i]);
			priv->mtds[i] = NULL;
		}
		if (priv->vols[i]) {
			ubi_put_device(priv->vols[i]->ubi);
			priv->vols[i] = NULL;
		}
	}

	free(priv);
}
