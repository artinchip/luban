/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "cpu_mem.h"

int mem_occupy_get(struct memory_occupy *mem)
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

    // MemTotal
    fgets(mem_total, sizeof(mem_total), fd);
    mem->total = atoi(mem_total + 15);

    // MemFree
    fgets (mem_free, sizeof(mem_free), fd);
    mem->free = atoi(mem_free + 15);

    // MemAvailable
    fgets (mem_available, sizeof(mem_available), fd);
    mem->available = atoi(mem_available + 15);

    // Buffers
    fgets(mem_buffers, sizeof(mem_buffers), fd); //Buffers
    mem->buffers = atoi(mem_buffers + 15);

    // Cached
    fgets(mem_cached, sizeof(mem_cached), fd);
    mem->cached = atoi(mem_cached + 15);

    fclose(fd);

    return 0;
}

float mem_occupy_cal_ratio(struct memory_occupy *mem)
{
    return (float)100.0 * (mem->total - mem->free - mem->buffers - mem->cached) / mem->total;
}

float mem_occupy_cal_size(struct memory_occupy *mem)
{
    return (float) ((mem->total - mem->free - mem->buffers - mem->cached) / 1024.0);
}

int cpu_occupy_get(struct cpu_occupy *t)
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

float cpu_occupy_cal(struct cpu_occupy *o, struct cpu_occupy *n)
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
