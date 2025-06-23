// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2025 ArtInChip Technology Co., Ltd.
 * Authors:  Xiong Hao <hao.xiong@artinchip.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <asm/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <linux/kernel.h>
#include <linux/module.h>

int add(int *a, int *b)
{
	int sum;

	sum = *a + *b;

	printf("sum:%d\n", sum);

	return sum;
}

int sub(int *a, int *b)
{
	int diff;

	diff = *a - *b;

	printf("diff:%d\n", diff);

	return diff;
}

int test_add_and_sub(void)
{
	int *a = NULL;
	int *b = NULL;

	printf("sum:%d\n", add(a, b) + sub(a, b));

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;

	test_add_and_sub();

	return ret;
}
