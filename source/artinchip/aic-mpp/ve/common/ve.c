/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: qi.xu@artinchip.com
*  Desc: ve module
*/

#define LOG_TAG "ve"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <video/artinchip_ve.h>
#include "ve.h"
#include "mpp_log.h"

#define VE_DEV		"/dev/aic_ve"

// 2s
#define VE_TIMEOUT	(2000)

struct ve_device_info {
	unsigned long reg_base;
	int ve_fd;
	int reg_size;
};

pthread_mutex_t 	g_ve_mutex = PTHREAD_MUTEX_INITIALIZER;
int 			g_ve_ref = 0;
struct ve_device_info 	g_ve_info;

int ve_open_device(void)
{
	pthread_mutex_lock(&g_ve_mutex);

	if(g_ve_ref == 0) {
		// 1. open /dev/aic_ve
		g_ve_info.ve_fd = open(VE_DEV, O_RDWR);
		if(g_ve_info.ve_fd < 0) {
			loge("open %s failed!", VE_DEV);
			pthread_mutex_unlock(&g_ve_mutex);
			return -1;
		}

		// 2. get register space size
		struct ve_info info = {0};
		ioctl(g_ve_info.ve_fd, IOC_VE_GET_INFO, &info);
		g_ve_info.reg_size = info.reg_size;

		// 3. map register space to virtual space
		g_ve_info.reg_base = (unsigned long)mmap(NULL,
						info.reg_size,
						PROT_READ | PROT_WRITE, MAP_SHARED,
						g_ve_info.ve_fd,
						0);
	}

	g_ve_ref ++;
	pthread_mutex_unlock(&g_ve_mutex);

	logi("ve_ref: %d, ve_fd: %d", g_ve_ref, g_ve_info.ve_fd);

	return g_ve_info.ve_fd;
}

void ve_close_device()
{
	pthread_mutex_lock(&g_ve_mutex);
	if (g_ve_ref == 0) {
		logd("ve has been closed\n");
		pthread_mutex_unlock(&g_ve_mutex);
		return;
	}
	g_ve_ref --;

	if(g_ve_ref == 0) {
		if(g_ve_info.ve_fd != -1) {
			munmap((void*)g_ve_info.reg_base, g_ve_info.reg_size);
			close(g_ve_info.ve_fd);
			g_ve_info.ve_fd = -1;
		}
	}

	logi("close ve, ref: %d, fd: %d", g_ve_ref, g_ve_info.ve_fd);

	pthread_mutex_unlock(&g_ve_mutex);
}

unsigned long ve_get_reg_base()
{
	return g_ve_info.reg_base;
}

int ve_reset()
{
	int ret;

	pthread_mutex_lock(&g_ve_mutex);
	if (g_ve_info.ve_fd < 0) {
		pthread_mutex_unlock(&g_ve_mutex);
		return -1;
	}

	ret = ioctl(g_ve_info.ve_fd, IOC_VE_RESET);

	pthread_mutex_unlock(&g_ve_mutex);
	return ret;
}

int ve_wait(unsigned int *reg_status)
{
	int ret;

	pthread_mutex_lock(&g_ve_mutex);
	if (g_ve_info.ve_fd < 0) {
		pthread_mutex_unlock(&g_ve_mutex);
		return -1;
	}

	struct wait_info info;
	info.wait_time = VE_TIMEOUT;

	ret = ioctl(g_ve_info.ve_fd, IOC_VE_WAIT, &info);
	*reg_status = info.reg_status;

	pthread_mutex_unlock(&g_ve_mutex);
	return ret;
}

int ve_get_client()
{
	int ret;

	if (g_ve_info.ve_fd < 0) {
		return -1;
	}

	ret = ioctl(g_ve_info.ve_fd, IOC_VE_GET_CLIENT);
	if (ret < 0) {
		loge("get client failed");
	}

	return ret;
}

int ve_put_client()
{
	int ret;

	if (g_ve_info.ve_fd < 0) {
		return -1;
	}

	ret = ioctl(g_ve_info.ve_fd, IOC_VE_PUT_CLIENT);

	return ret;
}

int ve_add_dma_buf(int dma_buf_fd, unsigned int *phy_addr)
{
	int ret;

	pthread_mutex_lock(&g_ve_mutex);
	if (g_ve_info.ve_fd < 0) {
		pthread_mutex_unlock(&g_ve_mutex);
		return -1;
	}

	struct dma_buf_info info = {0};
	info.fd = dma_buf_fd;
	ret = ioctl(g_ve_info.ve_fd, IOC_VE_ADD_DMA_BUF, &info);
	if (ret < 0) {
		loge("IOC_VE_ADD_DMA_BUF failed");
		*phy_addr = 0;
		pthread_mutex_unlock(&g_ve_mutex);
		return ret;
	}

	*phy_addr = info.phy_addr;

	pthread_mutex_unlock(&g_ve_mutex);
	return ret;
}

int ve_rm_dma_buf(int dma_buf_fd, unsigned int phy_addr)
{
	int ret;

	pthread_mutex_lock(&g_ve_mutex);
	if (g_ve_info.ve_fd < 0) {
		pthread_mutex_unlock(&g_ve_mutex);
		return -1;
	}

	struct dma_buf_info info = {0};
	info.fd = dma_buf_fd;
	info.phy_addr = phy_addr;
	ret = ioctl(g_ve_info.ve_fd, IOC_VE_RM_DMA_BUF, &info);
	if (ret < 0) {
		loge("IOC_VE_RM_DMA_BUF failed");
		pthread_mutex_unlock(&g_ve_mutex);
		return ret;
	}

	pthread_mutex_unlock(&g_ve_mutex);
	return ret;
}
