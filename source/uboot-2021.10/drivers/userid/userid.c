// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <userid.h>
#include <userid_internal.h>
#include <log.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/bug.h>
#include <u-boot/crc.h>
#include <malloc.h>
#include <hexdump.h>

static LIST_HEAD(userid_list);

static struct userid_driver *userid_driver_lookup(void)
{
	const int n_ents = ll_entry_count(struct userid_driver, userid_driver);
	struct userid_driver *drv;

	if (n_ents == 0) {
		pr_err("Error, No userid driver is found.\n");
		return NULL;
	}
	if (n_ents > 1) {
		pr_err("Error, userid is more than one location.\n");
		return NULL;
	}

	drv = ll_entry_start(struct userid_driver, userid_driver);

	return drv;
}

int userid_init(void)
{
	struct userid_driver *drv;
	const int n_ents = ll_entry_count(struct userid_driver, userid_driver);

	if (n_ents == 0) {
		pr_err("Error, No userid driver is found.\n");
		return -ENODEV;
	}
	if (n_ents > 1) {
		pr_err("Error, userid is more than one location.\n");
		return -ENODEV;
	}

	INIT_LIST_HEAD(&userid_list);

	drv = userid_driver_lookup();
	if (!drv) {
		pr_err("userid driver is not found.\n");
		return -ENODEV;
	}

	return drv->load();
}

static void free_userid_list(struct list_head *h)
{
	struct userid_entity *ent;

	while (!list_empty(h)) {
		ent = list_entry(h->next, struct userid_entity, list);
		list_del(h->next);
		free(ent->name);
		free(ent->data);
		free(ent);
	}
}

void userid_deinit(void)
{
	free_userid_list(&userid_list);
}

int userid_import(u8 *buf)
{
	struct userid_storage_header head;
	struct userid_item_header item;
	struct userid_entity *ent;
	u8 *data, *p, *pe;
	char *name;
	u32 val;

	if (!buf) {
		pr_err("Invalid userid import parameter.\n");
		return -1;
	}

	memcpy(&head, buf, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC) {
		pr_info("UserID data is invalid.\n");
		return -1;
	}
	p = buf + 8;
	val = crc32(0, p, head.total_length);
	if (val != head.crc32) {
		pr_err("userid data verify failed.\n");
		return -1;
	}

	/* Clear old data first */
	free_userid_list(&userid_list);

	p = buf + 32;
	pe = buf + 8 + head.total_length;
	while (p < pe) {
		memcpy(&item, p, sizeof(item));
		p += sizeof(item);
		if (item.magic != USERID_ITEM_MAGIC) {
			pr_warn("Got unknown item.\n");
			goto err;
		}

		ent = malloc(sizeof(struct userid_entity));
		if (!ent) {
			pr_err("Error, failed to malloc.\n");
			goto err;
		}

		name = malloc(item.name_len + 1);
		if (!name) {
			pr_err("Error, failed to malloc name buffer.\n");
			goto err;
		}
		memcpy(name, p, item.name_len);
		name[item.name_len] = 0;
		p += item.name_len;

		data = malloc(item.data_len);
		if (!data) {
			pr_err("Error, failed to malloc data buffer.\n");
			goto err;
		}
		memcpy(data, p, item.data_len);
		p += item.data_len;

		ent->name_len = item.name_len;
		ent->data_len = item.data_len;
		ent->name = name;
		ent->data = data;
		list_add_tail(&ent->list, &userid_list);
		ent = NULL;
		name = NULL;
		data = NULL;
	}
	return 0;

err:
	if (ent)
		free(ent);
	if (name)
		free(name);
	if (data)
		free(data);
	free_userid_list(&userid_list);
	return -1;
}

int userid_export(u8 *buf)
{
	struct userid_storage_header head;
	struct userid_item_header item;
	struct userid_entity *curr;
	struct list_head *pos;
	u32 total_len = 0;
	u8 *p;

	memset(&head, 0, sizeof(head));
	head.magic = USERID_HEADER_MAGIC;
	p = buf + sizeof(head);
	total_len = sizeof(head) - 8;
	list_for_each(pos, &userid_list) {
		curr = list_entry(pos, struct userid_entity, list);
		item.magic = USERID_ITEM_MAGIC;
		item.name_len = curr->name_len;
		item.data_len = curr->data_len;
		memcpy(p, &item, sizeof(item));
		p += sizeof(item);
		total_len += sizeof(item);
		memcpy(p, curr->name, curr->name_len);
		p += curr->name_len;
		total_len += curr->name_len;
		memcpy(p, curr->data, curr->data_len);
		p += curr->data_len;
		total_len += curr->data_len;
	}
	head.total_length = total_len;
	memcpy(buf, &head, sizeof(head));
	head.crc32 = crc32(0, buf + 8, total_len);
	memcpy(buf, &head, sizeof(head));

	return (int)(total_len + 8);
}

int userid_get_count(void)
{
	struct list_head *pos;
	int cnt = 0;

	list_for_each(pos, &userid_list) {
		cnt++;
	}
	return cnt;
}

int userid_get_name(int idx, char *buf, int len)
{
	struct userid_entity *curr;
	struct list_head *pos;
	int cnt = 0, ret = 0;

	if (!buf) {
		pr_err("Invalid input buffer.\n");
		return -EINVAL;
	}
	list_for_each(pos, &userid_list) {
		curr = list_entry(pos, struct userid_entity, list);
		if (cnt == idx) {
			strncpy(buf, curr->name, len);
			ret = curr->name_len;
			if (ret > len)
				ret = len;
			break;
		}
		cnt++;
	}

	return ret;
}

static struct userid_entity *userid_find_by_name(const char *name)
{
	struct userid_entity *curr;
	struct list_head *pos;

	if (!name) {
		pr_err("Invalid input name.\n");
		return NULL;
	}
	list_for_each(pos, &userid_list) {
		curr = list_entry(pos, struct userid_entity, list);
		if (!strcmp(name, curr->name))
			return curr;
	}

	return NULL;
}

int userid_get_data_length(const char *name)
{
	struct userid_entity *ent;

	ent = userid_find_by_name(name);
	if (!ent) {
		pr_info("%s is not exist.\n", name);
		return -ENOENT;
	}

	return ent->data_len;
}

int userid_read(const char *name, int offset, u8 *buf, int len)
{
	struct userid_entity *ent;
	int total_len, dolen;

	if ((offset < 0) || (len <= 0)) {
		pr_err("%s, Invalid parameters: offset %d, len %d\n", __func__,
		       offset, len);
		return -EINVAL;
	}

	total_len = offset + len;
	ent = userid_find_by_name(name);
	if (!ent) {
		pr_err("userid %s is not exist.\n", name);
		return -ENOENT;
	}
	if (ent->data_len < offset)
		return 0;
	dolen = len;
	if (dolen > (ent->data_len - offset))
		dolen = (ent->data_len - offset);
	memcpy(buf, ent->data + offset, dolen);

	return dolen;
}

int userid_write(const char *name, int offset, u8 *buf, int len)
{
	struct userid_entity *ent;
	int total_len;
	u8 *data;

	if ((offset < 0) || (len <= 0))
		return -1;

	total_len = offset + len;
	ent = userid_find_by_name(name);
	if (ent) {
		if (total_len > ent->data_len) {
			data = malloc(total_len);
			memcpy(data, ent->data, ent->data_len);
			memcpy(data + offset, buf, len);
			free(ent->data);
			ent->data = data;
		} else {
			memcpy(ent->data + offset, buf, len);
		}
	} else {
		ent = malloc(sizeof(*ent));
		ent->name = malloc(strlen(name) + 1);
		memset(ent->name, 0, strlen(name) + 1);
		memcpy(ent->name, name, strlen(name));
		ent->data = malloc(total_len);
		memset(ent->data, 0, total_len);
		memcpy(ent->data + offset, buf, len);
		list_add_tail(&ent->list, &userid_list);
	}
	ent->name_len = strlen(name);
	ent->data_len = total_len;
	return len;
}

int userid_remove(const char *name)
{
	struct userid_entity *ent;

	ent = userid_find_by_name(name);
	if (!ent) {
		pr_info("%s is not exist.\n", name);
		return -ENOENT;
	}

	list_del(&ent->list);
	free(ent->name);
	free(ent->data);
	free(ent);
	return 0;
}

int userid_save(void)
{
	struct userid_driver *drv;

	drv = userid_driver_lookup();
	if (!drv) {
		pr_err("Error, userid driver is not found.\n");
		return -ENODEV;
	}
	return drv->save();
}
