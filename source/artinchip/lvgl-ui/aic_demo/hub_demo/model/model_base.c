/*
 * Copyright (C) 2022-2023 Artinchip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "model_base.h"
#include "lv_port_disp.h"

struct cpu_occupy
{
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
};

struct memory_occupy
{
    unsigned long total;
    unsigned long free;
    unsigned long available;
    unsigned long buffers;
    unsigned long cached;
};

static int mem_occupy_get(struct memory_occupy *mem)
{
    FILE *fd;
    char mem_cached[256];
    char mem_total[256];
    char mem_free[256];
    char mem_available[256];
    char mem_buffers[256];

    fd = fopen ("/proc/meminfo", "r");

    if (!fd) {
        return -1;
    }

    /* MemTotal */
    fgets(mem_total, sizeof(mem_total), fd);
    mem->total = atoi(mem_total + 15);

    /* MemFree */
    fgets (mem_free, sizeof(mem_free), fd);
    mem->free = atoi(mem_free + 15);

    /* MemAvailable */
    fgets (mem_available, sizeof(mem_available), fd);
    mem->available = atoi(mem_available + 15);

    /* Buffers */
    fgets(mem_buffers, sizeof(mem_buffers), fd);
    mem->buffers = atoi(mem_buffers + 15);

    /* Cached */
    fgets(mem_cached, sizeof(mem_cached), fd);
    mem->cached = atoi(mem_cached + 15);

    fclose(fd);

    return 0;
}

static float mem_occupy_cal_size(struct memory_occupy *mem)
{
    return (float) ((mem->total - mem->free - mem->buffers - mem->cached) / 1024.0);
}

static int cpu_occupy_get(struct cpu_occupy *t)
{
    FILE *fd;
    char buff[1024] = { 0 };
    char name[64]={ 0 };

    fd = fopen("/proc/stat","r");

    if (!fd) {
        return -1;
    }

    fgets(buff,sizeof(buff), fd);
    sscanf(buff,"%s %lu %lu %lu %lu", name, &t->user, &t->nice, &t->system, &t->idle);
    fclose(fd);

    return 0;
}

static float cpu_occupy_cal(struct cpu_occupy *o, struct cpu_occupy *n)
{
    unsigned long od, nd;
    unsigned long id, sd;
    float cpu_use = 0;

    od = (unsigned long)(o->user + o->nice + o->system + o->idle);
    nd = (unsigned long)(n->user + n->nice + n->system + n->idle);
    id = (unsigned long)(n->user - o->user);
    sd = (unsigned long)(n->system - o->system);

    if ((nd - od) != 0)
        cpu_use = (float)((sd + id)*100) / (float)(nd - od);

    return cpu_use;
}

int get_fb_draw_fps(void)
{
    return fbdev_draw_fps();
}

float get_cpu_usage(void)
{
    static int first_cal_cpu = 0;
    static int cpu_cal_id = 0;
    static struct cpu_occupy cpu_stat[2];
    int last_id = 0;
    float cpu_usage = 0;

    /* cpu usage */
    last_id = (cpu_cal_id == 1) ? 0 : 1;
    if (first_cal_cpu)
        cpu_occupy_get((struct cpu_occupy *)&cpu_stat[last_id]);

    cpu_occupy_get((struct cpu_occupy *)&cpu_stat[cpu_cal_id]);
    cpu_usage = cpu_occupy_cal((struct cpu_occupy *)&cpu_stat[last_id],
        (struct cpu_occupy *)&cpu_stat[cpu_cal_id]);

    if (cpu_usage >= 30.0) {
        cpu_usage -= 10.0;
    }

    cpu_cal_id = last_id;

    return cpu_usage;
}

float get_mem_usage(void)
{
    static struct memory_occupy mem_stat = {0};
    float mem_usage = 0;

    mem_occupy_get(&mem_stat);
    mem_usage =  mem_occupy_cal_size(&mem_stat);

    return mem_usage;
}
