/*
 * Copyright (C) 2022-2024 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef CPU_MEM_H
#define CPU_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

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

int mem_occupy_get(struct memory_occupy *mem);

float mem_occupy_cal_ratio(struct memory_occupy *mem);

float mem_occupy_cal_size(struct memory_occupy *mem);

int cpu_occupy_get(struct cpu_occupy *t);

float cpu_occupy_cal(struct cpu_occupy *o, struct cpu_occupy *n);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //CPU_MEM_H
