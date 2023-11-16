/*
 * Copyright (C) 2020-2023 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */
#include <unistd.h>
#include <stdint.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/user.h>
#include <time.h>
#include <linux/random.h>
#include <limits.h>
#include <sys/types.h>
#include <zlib.h>
#include "list.h"
#include "internal.h"

static LIST_HEAD(userid_list);
int userid_import(uint8_t *buf);
int userid_export(uint8_t *buf);

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

int userid_init(const char *path)
{
	uint8_t *buf;
	int ret;

	buf = NULL;
	ret = userid_part_load(path, &buf);
	if (ret == 0)
		return 0;
	if (ret < 0) {
		fprintf(stderr, "load userid failed.\n");
		if (buf)
			free(buf);
		return -1;
	}

	ret = userid_import(buf);
	if (ret)
		goto err;

	if (buf)
		free(buf);
	return 0;

err:
	if (buf)
		free(buf);
	return -1;
}

void userid_deinit(void)
{
	free_userid_list(&userid_list);
}

int userid_import(uint8_t *buf)
{
	struct userid_storage_header head;
	struct userid_item_header item;
	struct userid_entity *ent = NULL;
	uint8_t *data, *p, *pe;
	char *name = NULL;
	uint32_t val;
	int ret = 0;

	if (!buf) {
		fprintf(stderr, "invalid parameter.\n");
		return -1;
	}

	memcpy(&head, buf, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC) {
		fprintf(stderr, "UserID data is invalid.\n");
		ret = -1;
		goto err;
	}
	p = buf + 8;
	val = crc32(0, p, head.total_length);
	if (val != head.crc32) {
		fprintf(stderr, "userid data verify failed.\n");
		ret = -1;
		goto err;
	}

	/* Clear old data first */
	free_userid_list(&userid_list);

	p = buf + 32;
	pe = buf + 8 + head.total_length;
	while (p < pe) {
		memcpy(&item, p, sizeof(item));
		p += sizeof(item);
		if (item.magic != USERID_ITEM_MAGIC) {
			fprintf(stderr, "Got unknown item.\n");
			ret = -1;
			goto err;
		}

		ent = malloc(sizeof(struct userid_entity));
		if (!ent) {
			fprintf(stderr, "Error, failed to malloc.\n");
			ret = -1;
			goto err;
		}

		name = malloc(item.name_len + 1);
		if (!name) {
			fprintf(stderr, "Error, failed to malloc name buffer.\n");
			ret = -1;
			goto err;
		}
		memcpy(name, p, item.name_len);
		name[item.name_len] = 0;
		p += item.name_len;

		data = malloc(item.data_len);
		if (!data) {
			fprintf(stderr, "Error, failed to malloc data buffer.\n");
			ret = -1;
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
	return ret;

err:
	if (ent)
		free(ent);
	if (name)
		free(name);
	if (data)
		free(data);
	free_userid_list(&userid_list);
	return ret;
}

int userid_export(uint8_t *buf)
{
	struct userid_storage_header head;
	struct userid_item_header item;
	struct userid_entity *curr;
	struct list_head *pos;
	uint32_t total_len = 0;
	uint8_t *p;

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
		fprintf(stderr, "Invalid input buffer.\n");
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
		fprintf(stderr, "Invalid input name.\n");
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
		printf("%s is not exist.\n", name);
		return -ENOENT;
	}

	return ent->data_len;
}

int userid_read(const char *name, int offset, uint8_t *buf, int len)
{
	struct userid_entity *ent;
	int dolen;

	if ((offset < 0) || (len <= 0)) {
		fprintf(stderr, "%s, Invalid parameters: offset %d, len %d\n",
			__func__, offset, len);
		return -EINVAL;
	}

	ent = userid_find_by_name(name);
	if (!ent) {
		fprintf(stderr, "userid %s is not exist.\n", name);
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

int userid_write(const char *name, int offset, uint8_t *buf, int len)
{
	struct userid_entity *ent;
	int total_len;
	uint8_t *data;

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
		fprintf(stderr, "%s is not exist.\n", name);
		return -ENOENT;
	}

	list_del(&ent->list);
	free(ent->name);
	free(ent->data);
	free(ent);

	return 0;
}

static int get_buffer_length(void)
{
	struct userid_storage_header head;
	struct userid_item_header item;
	struct userid_entity *curr;
	struct list_head *pos;
	uint32_t total_len = 0;

	total_len = sizeof(head);
	list_for_each(pos, &userid_list) {
		curr = list_entry(pos, struct userid_entity, list);
		total_len += sizeof(item);
		total_len += curr->name_len;
		total_len += curr->data_len;
	}

	return (int)(total_len);
}

int userid_save(const char *path)
{
	uint8_t *buf;
	int len, ret;

	len = get_buffer_length();

	buf = malloc(len);
	memset(buf, 0, len);

	ret = userid_export(buf);
	if (ret < 0 || (ret != len)) {
		fprintf(stderr, "export userid failed.\n");
		return -1;
	}

	ret = userid_part_save(path, buf, len);
	if (ret < 0)
		return ret;
	return 0;
}

