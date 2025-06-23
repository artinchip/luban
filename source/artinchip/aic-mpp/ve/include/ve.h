/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: artinchip
 *  Desc: ve module interface
 *        ve is a singleton pattern. there ara three functions in this module:
 *           1) open ve device
 *           2) get the ve device resource
 *           3) map the register space
 *           4) get physic address of a dma-buf
 */

#ifndef VE_H
#define VE_H

/**
 * ve_open_device - open ve device, return ve fd
 */
int ve_open_device(void);

/**
 * ve_close_device - close ve device
 */
void ve_close_device(void);

/**
 * ve_get_reg_base - get the register virtual address, it must call after ve_open_device
 */
unsigned long ve_get_reg_base(void);

/**
 * ve_reset - ve hardware reset. used if ve work error (decode fail or timeout)
 */
int ve_reset(void);

/**
 * ve_get_client- get ve device resource
 */
int ve_get_client(void);

/**
 * ve_put_client- release ve device resource
 */
int ve_put_client(void);

/**
 * ve_wait - ve wait irq
 * @reg_status: return value of ve status register
 * return: wait interrupt timeout if < 0
 */
int ve_wait(unsigned int *reg_status);

/**
 * ve_add_dma_buf - add a dma-buf to ve driver and get the physic address
 * @dma_buf_fd: fd of dma-buf
 * @phy_addr: physic address of this dma-buf
 */
int ve_add_dma_buf(int dma_buf_fd, unsigned int *phy_addr);

/**
 * ve_rm_dma_buf - remove the dma-buf from ve driver
 * @dma_buf_fd: fd of dma-buf
 * @phy_addr: physic address of this dma-buf
 */
int ve_rm_dma_buf(int dma_buf_fd, unsigned int phy_addr);

#endif /* VE_H */
