/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021, ArtInChip Technology Co., Ltd
 */

#ifndef __AIC_UDC_H
#define __AIC_UDC_H

#include <linux/errno.h>
#include <linux/sizes.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <dm/ofnode.h>

#define PHY0_SLEEP              (1 << 5)
#define AIC_MAX_HW_ENDPOINTS	16
#define DIS_EP_TIMOUT		100
#define EP0_RW_WAIT_COUNT	100000

struct aic_plat_udc_data {
	void		*priv;
	ofnode		phy_of_node;
	int		(*phy_control)(int on);
	uintptr_t	regs_phy;
	uintptr_t	regs_udc;
	unsigned int    usb_phy_ctrl;
	unsigned int    usb_flags;
	unsigned int	usb_gusbcfg;
	unsigned int	rx_fifo_sz;
	unsigned int	np_tx_fifo_sz;
	unsigned int	tx_fifo_sz;
	unsigned int	tx_fifo_sz_array[AIC_MAX_HW_ENDPOINTS];
	unsigned char   tx_fifo_sz_nb;
	bool		force_b_session_valid;
	bool		force_vbus_detection;
	bool		activate_stm_id_vb_detection;
};

int aic_udc_probe(struct aic_plat_udc_data *pdata);

/*-------------------------------------------------------------------------*/
/* DMA bounce buffer size, 16K is enough even for mass storage */
#define DMA_BUFFER_SIZE	(16 * SZ_1K)

#define EP0_FIFO_SIZE		64
#define EP_FIFO_SIZE		512
#define EP_FIFO_SIZE2		1024
/* ep0-control, ep1in-bulk, ep2out-bulk, ep3in-int */
#ifdef CONFIG_AIC_USB_UDC_V10
#define AIC_MAX_ENDPOINTS	4
#else
#define AIC_MAX_ENDPOINTS	16
#endif
#define WAIT_FOR_SETUP          0
#define DATA_STATE_XMIT         1
#define DATA_STATE_NEED_ZLP     2
#define WAIT_FOR_OUT_STATUS     3
#define DATA_STATE_RECV         4
#define WAIT_FOR_COMPLETE	5
#define WAIT_FOR_OUT_COMPLETE	6
#define WAIT_FOR_IN_COMPLETE	7
#define WAIT_FOR_NULL_COMPLETE	8

#define TEST_J_SEL		0x1
#define TEST_K_SEL		0x2
#define TEST_SE0_NAK_SEL	0x3
#define TEST_PACKET_SEL		0x4
#define TEST_FORCE_ENABLE_SEL	0x5

/* ************************************************************************* */
/* IO
 */

enum ep_type {
	ep_control, ep_bulk_in, ep_bulk_out, ep_interrupt
};

struct aic_ep {
	struct usb_ep ep;
	struct aic_udc *dev;

	const struct usb_endpoint_descriptor *desc;
	struct list_head queue;
	unsigned long pio_irqs;
	int len;
	void *dma_buf;

	u8 stopped;
	u8 bEndpointAddress;
	u8 bmAttributes;

	enum ep_type ep_type;
	int fifo_num;
};

struct aic_request {
	struct usb_request req;
	struct list_head queue;
	void *saved_req_buf;
};

struct aic_udc {
	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;

	struct aic_plat_udc_data *pdata;

	int ep0state;
	struct aic_ep ep[AIC_MAX_ENDPOINTS];

	unsigned char usb_address;

	unsigned req_pending:1, req_std:1;

	u32 tx_fifo_map;
};

#define ep_is_in(EP) (((EP)->bEndpointAddress &  USB_DIR_IN) == USB_DIR_IN)
#define ep_index(EP) ((EP)->bEndpointAddress & 0xF)
#define ep_maxpacket(EP) ((EP)->ep.maxpacket)

void udc_phy_init(struct aic_udc *dev);
void udc_phy_off(struct aic_udc *dev);

struct ep_fifo {
	u32 fifo;
	u8  res[4092];
};

struct aic_udc_reg_v10 {
	u32 ahbbasic;		/* 0x0000: AHBBASIC */
	u32 usbdevinit;		/* 0x0004: USBDEVINIT */
	u32 usbphyif;		/* 0x0008: USBPHYIF */
	u32 usbulpiphy;		/* 0x000C: USBULPIPHY */
	u32 usbintsts;		/* 0x0010: USBINTSTS */
	u32 usbintmsk;		/* 0x0014: USBINTMSK */
	u32 rxfifosiz;		/* 0x0018: RXFIFOSIZ */
	u32 rxfifosts;		/* 0x001C: RXFIFOSTS */
	u32 nptxfifosiz;	/* 0x0020: NPTXFIFOSIZ */
	u32 nptxfifosts;	/* 0x0024: NPTXFIFOSTS */
	u32 txfifosiz[2];	/* 0x0028 - 0x002C: TXFIFOSIZ() */
	u32 rxfifosts_dbg;	/* 0x0030: RXFIFOSTS_DBG */
	u8  res0[0x1cc];
	u32 usbdevconf;		/* 0x0200: USBDEVCONF */
	u32 usbdevfunc;		/* 0x0204: USBDEVFUNC */
	u32 usblinests;		/* 0x0208: USBLINESTS */
	u32 inepintmsk;		/* 0x020C: INEPINTMSK */
	u32 outepintmsk;	/* 0x0210: OUTEPINTMSK */
	u32 usbepint;		/* 0x0214: USBEPINT */
	u32 usbepintmsk;	/* 0x0218: USBEPINTMSK */
	u8  res1[4];
	u32 inepcfg[5];		/* 0x0220 - 0x0230: INEPCFG() */
	u8  res2[0xc];
	u32 outepcfg[5];	/* 0x0240 - 0x0250: OUTEPCFG() */
	u8  res3[0xc];
	u32 inepint[5];		/* 0x0260 - 0x0270: INEPINT() */
	u8  res4[0xc];
	u32 outepint[5];	/* 0x0280 - 0x0290: OUTEPINT() */
	u8  res5[0xc];
	u32 ineptsfsiz[5];	/* 0x02A0 - 0x02B0: INEPTSFSIZ() */
	u8  res6[0xc];
	u32 outeptsfsiz[5];	/* 0x02C0 - 0x02D0: OUTEPTSFSIZ() */
	u8  res7[0x2c];
	u32 inepdmaaddr[5];	/* 0x0300 - 0x0310: INEPDMAADDR() */
	u8  res8[0xc];
	u32 outepdmaaddr[5];	/* 0x0320 - 0x0330: OUTEPDMAADDR() */
	u8  res9[0xc];
	u32 ineptxsts[5];	/* 0x0340 - 0x0350: INEPTXSTS() */
	u8  res10[0xc];
	u32 dtknqr1;		/* 0x0360: DTKNQR1 */
	u32 dtknqr2;		/* 0x0364: DTKNQR2 */
	u32 dtknqr3;		/* 0x0368: DTKNQR3 */
	u32 dtknqr4;		/* 0x036C: DTKNQR4 */
};

struct aic_udc_reg_v20 {
	u32 ahbbasic;			/* 0x0000: AHBBASIC */
	u32 usbdevinit;			/* 0x0004: USBDEVINIT */
	u32 usbphyif;			/* 0x0008: USBPHYIF */
	u32 usbulpiphy;			/* 0x000C: USBULPIPHY */
	u32 usbintsts;			/* 0x0010: USBINTSTS */
	u32 usbintmsk;			/* 0x0014: USBINTMSK */
	u32 rxfifosiz;			/* 0x0018: RXFIFOSIZ */
	u32 rxfifosts;			/* 0x001C: RXFIFOSTS */
	u32 ietxfifosiz;		/* 0x0020: IETXFIFO0_SIZ */
	u32 thr_ctl;			/* 0x0024: THR_CTL */
	u8  res0[0x8];
	u32 rxfifosts_dbg;		/* 0x0030: RXFIFOSTS_DBG */
	u8  res1[0xc];
	u32 phyclkctl;			/* 0x0040: RXFIFOSTS_DBG */
	u8  res2[0x1c];
	u32 txfifosiz[AIC_MAX_ENDPOINTS - 1];	/* 0x0060 - 0x0098: TXFIFOSIZ() */
	u8  res3[0x164];
	u32 usbdevconf;			/* 0x0200: USBDEVCONF */
	u32 usbdevfunc;			/* 0x0204: USBDEVFUNC */
	u32 usblinests;			/* 0x0208: USBLINESTS */
	u32 inepintmsk;			/* 0x020C: INEPINTMSK */
	u32 outepintmsk;		/* 0x0210: OUTEPINTMSK */
	u32 usbepint;			/* 0x0214: USBEPINT */
	u32 usbepintmsk;		/* 0x0218: USBEPINTMSK */
	u8  res4[0x4];
	u32 inepcfg[AIC_MAX_ENDPOINTS];		/* 0x0220 - 0x0260: INEPCFG() */
	u32 outepcfg[AIC_MAX_ENDPOINTS];	/* 0x0260 - 0x02A0: OUTEPCFG() */
	u32 inepint[AIC_MAX_ENDPOINTS];		/* 0x02A0 - 0x02E0: INEPINT() */
	u32 outepint[AIC_MAX_ENDPOINTS];	/* 0x02E0 - 0x0320: OUTEPINT() */
	u32 ineptsfsiz[AIC_MAX_ENDPOINTS];	/* 0x0320 - 0x0360: INEPTSFSIZ() */
	u32 outeptsfsiz[AIC_MAX_ENDPOINTS];	/* 0x0360 - 0x03A0: OUTEPTSFSIZ() */
	u32 inepdmaaddr[AIC_MAX_ENDPOINTS];	/* 0x03A0 - 0x03E0: INEPDMAADDR() */
	u32 outepdmaaddr[AIC_MAX_ENDPOINTS];	/* 0x03E0 - 0x0420: OUTEPDMAADDR() */
	u32 ineptxsts[AIC_MAX_ENDPOINTS];	/* 0x0420 - 0x0460: INEPTXSTS() */
	u32 dtknqr1;			/* 0x0360: DTKNQR1 */
	u32 dtknqr2;			/* 0x0364: DTKNQR2 */
	u32 dtknqr3;			/* 0x0368: DTKNQR3 */
	u32 dtknqr4;			/* 0x036C: DTKNQR4 */
};

/*===================================================================== */
/*definitions related to CSR setting */

/* AHBBASIC */
#define AHBBASIC_NOTI_ALL_DMA_WRIT	BIT(8)
#define AHBBASIC_REM_MEM_SUPP		BIT(7)
#define AHBBASIC_INV_DESC_ENDIANNESS	BIT(6)
#define AHBBASIC_AHB_SINGLE		BIT(5)
#define AHBBASIC_TXENDDELAY		BIT(3)
#define AHBBASIC_AHBIDLE		BIT(2)
#define AHBBASIC_DMAREQ			BIT(1)

/* USBDEVINIT */
#define USBDEVINIT_HBSTLEN_MASK		(0xf << 12)
#define USBDEVINIT_HBSTLEN_SHIFT	12
#define USBDEVINIT_HBSTLEN_SINGLE	0
#define USBDEVINIT_HBSTLEN_INCR		1
#define USBDEVINIT_HBSTLEN_INCR4	3
#define USBDEVINIT_HBSTLEN_INCR8	5
#define USBDEVINIT_HBSTLEN_INCR16	7
#define USBDEVINIT_DMA_EN		BIT(11)
#define USBDEVINIT_NP_TXF_EMP_LVL	BIT(10)
#define USBDEVINIT_GLBL_INTR_EN		BIT(9)
#define USBDEVINIT_CTRL_MASK		(USBDEVINIT_NP_TXF_EMP_LVL | \
					 USBDEVINIT_DMA_EN | \
					 USBDEVINIT_GLBL_INTR_EN)
#define USBDEVINIT_IN_TKNQ_FLSH		BIT(8)
#define USBDEVINIT_TXFNUM_MASK		(0x1f << 3)
#define USBDEVINIT_TXFNUM_SHIFT		3
#define USBDEVINIT_TXFNUM_LIMIT		0x1f
#define USBDEVINIT_TXFNUM(_x)		((_x) << 3)
#define USBDEVINIT_TXFFLSH		BIT(2)
#define USBDEVINIT_RXFFLSH		BIT(1)
#define USBDEVINIT_CSFTRST		BIT(0)

/* USBPHYIF */
#define USBPHYIF_ULPI_CLK_SUSP_M	BIT(19)
#define USBPHYIF_ULPI_AUTO_RES		BIT(18)
#define USBPHYIF_PHY_LP_CLK_SEL		BIT(15)
#define USBPHYIF_USBTRDTIM_MASK		(0xf << 10)
#define USBPHYIF_USBTRDTIM_SHIFT	10
#define USBPHYIF_DDRSEL			BIT(7)
#define USBPHYIF_ULPI_UTMI_SEL		BIT(4)
#define USBPHYIF_PHYIF16		BIT(3)
#define USBPHYIF_PHYIF8			(0 << 3)
#define USBPHYIF_TOUTCAL_MASK		(0x7 << 0)
#define USBPHYIF_TOUTCAL_SHIFT		0
#define USBPHYIF_TOUTCAL_LIMIT		0x7
#define USBPHYIF_TOUTCAL(_x)		((_x) << 0)

/* USBINTSTS/USBINTMSK interrupt register */
#define INT_RESUME			(1u << 31)
#define INT_OUT_EP			(0x1 << 19)
#define INT_IN_EP			(0x1 << 18)
#define INT_ENUMDONE			(0x1 << 13)
#define INT_RESET			(0x1 << 12)
#define INT_SUSPEND			(0x1 << 11)
#define INT_EARLY_SUSPEND		(0x1 << 10)
#define INT_NP_TX_FIFO_EMPTY		(0x1 << 5)
#define INT_RX_FIFO_NOT_EMPTY		(0x1 << 4)
#define INT_SOF				(0x1 << 3)
#define INT_GOUTNAKEFF			(0x01 << 7)
#define INT_GINNAKEFF			(0x01 << 6)

#define FULL_SPEED_CONTROL_PKT_SIZE	8
#define FULL_SPEED_BULK_PKT_SIZE	64

#define HIGH_SPEED_CONTROL_PKT_SIZE	64
#define HIGH_SPEED_BULK_PKT_SIZE	512

#define RX_FIFO_SIZE			(1024)
#define NPTX_FIFO_SIZE			(1024)
#define PTX_FIFO_SIZE			(384)

/* fifo size configure */
#define EPS_NUM				5
#ifdef CONFIG_AIC_USB_UDC_V10
#define TX_FIFO_NUM			3 /* Non-periodic:1 Periodic:2 */
#else
#define TX_FIFO_NUM			16
#endif
#define TOTAL_FIFO_SIZE			0x3f6
#define AIC_RX_FIFO_SIZE		0x119
#define AIC_NP_TX_FIFO_SIZE		0x100
#define AIC_PERIOD_TX_FIFO1_SIZE	0x100
#define AIC_PERIOD_TX_FIFO2_SIZE	0xDD

#define DEPCTL_TXFNUM_0			(0x0 << 22)
#define DEPCTL_TXFNUM_1			(0x1 << 22)
#define DEPCTL_TXFNUM_2			(0x2 << 22)
#define DEPCTL_TXFNUM_3			(0x3 << 22)
#define DEPCTL_TXFNUM_4			(0x4 << 22)

/* Enumeration speed */
#define USB_HIGH_30_60MHZ		(0x0 << 1)
#define USB_FULL_30_60MHZ		(0x1 << 1)
#define USB_LOW_6MHZ			(0x2 << 1)
#define USB_FULL_48MHZ			(0x3 << 1)

/* RXFIFOSTS */
#define OUT_PKT_RECEIVED		(0x2 << 17)
#define OUT_TRANSFER_COMPLELTED		(0x3 << 17)
#define SETUP_TRANSACTION_COMPLETED	(0x4 << 17)
#define SETUP_PKT_RECEIVED		(0x6 << 17)
#define GLOBAL_OUT_NAK			(0x1 << 17)

/* USBDEVFUNC */
#define NORMAL_OPERATION		(0x1 << 0)
#define SOFT_DISCONNECT			(0x1 << 1)
#define USBDEVFUNC_SERVICE_INTERVAL_SUPPORTED BIT(19)
#define USBDEVFUNC_PWRONPRGDONE		BIT(11)
#define USBDEVFUNC_CGOUTNAK		BIT(10)
#define USBDEVFUNC_SGOUTNAK		BIT(9)
#define USBDEVFUNC_CGNPINNAK		BIT(8)
#define USBDEVFUNC_SGNPINNAK		BIT(7)
#define USBDEVFUNC_TSTCTL_MASK		(0x7 << 4)
#define USBDEVFUNC_TSTCTL_SHIFT		4
#define USBDEVFUNC_GOUTNAKSTS		BIT(3)
#define USBDEVFUNC_GNPINNAKSTS		BIT(2)
#define USBDEVFUNC_SFTDISCON		BIT(1)
#define USBDEVFUNC_RMTWKUPSIG		BIT(0)

/* USBEPINT endpoint interrupt register */
#define DAINT_OUT_BIT			(16)
#define DAINT_MASK			(0xFFFF)

/* INEPCFG()/OUTEPCFG()
 * devicecontrol IN/OUT endpoint 0 control register
 */
#define DEPCTL_EPENA			(1u << 31)
#define DEPCTL_EPDIS			(0x1 << 30)
#define DEPCTL_SETD1PID			(0x1 << 29)
#define DEPCTL_SETD0PID			(0x1 << 28)
#define DEPCTL_SNAK			(0x1 << 27)
#define DEPCTL_CNAK			(0x1 << 26)
#define DEPCTL_STALL			(0x1 << 21)
#define DEPCTL_TYPE_BIT			(18)
#define DEPCTL_TYPE_MASK		(0x3 << 18)
#define DEPCTL_CTRL_TYPE		(0x0 << 18)
#define DEPCTL_ISO_TYPE			(0x1 << 18)
#define DEPCTL_BULK_TYPE		(0x2 << 18)
#define DEPCTL_INTR_TYPE		(0x3 << 18)
#define DEPCTL_USBACTEP			(0x1 << 15)
#define DEPCTL_NEXT_EP_BIT		(11)
#define DEPCTL_MPS_BIT			(0)
#define DEPCTL_MPS_MASK			(0x7FF)

#define DEPCTL0_MPS_64			(0x0 << 0)
#define DEPCTL0_MPS_32			(0x1 << 0)
#define DEPCTL0_MPS_16			(0x2 << 0)
#define DEPCTL0_MPS_8			(0x3 << 0)
#define DEPCTL_MPS_BULK_512		(512 << 0)
#define DEPCTL_MPS_INT_MPS_16		(16 << 0)

#define DIEPCTL0_NEXT_EP_BIT		(11)

/* INEPINT/OUTEPINT device IN/OUT endpoint interrupt register */
#define BACK2BACK_SETUP_RECEIVED	(0x1 << 6)
#define INTKNEPMIS			(0x1 << 5)
#define INTKN_TXFEMP			(0x1 << 4)
#define NON_ISO_IN_EP_TIMEOUT		(0x1 << 3)
#define CTRL_OUT_EP_SETUP_PHASE_DONE	(0x1 << 3)
#define AHB_ERROR			(0x1 << 2)
#define EPDISBLD			(0x1 << 1)
#define TRANSFER_DONE			(0x1 << 0)

/* Device Configuration Register DCFG */
#define DEV_SPEED_HIGH_SPEED_20         (0x0 << 0)
#define DEV_SPEED_FULL_SPEED_20         (0x1 << 0)
#define DEV_SPEED_LOW_SPEED_11          (0x2 << 0)
#define DEV_SPEED_FULL_SPEED_11         (0x3 << 0)
#define EP_MISS_CNT(x)                  (x << 18)
#define DEVICE_ADDRESS(x)               (x << 4)

/* Masks definitions */
#define GINTMSK_INIT	(INT_OUT_EP | INT_IN_EP | INT_RESUME | INT_ENUMDONE\
			| INT_RESET | INT_SUSPEND)
#define DOEPMSK_INIT	(CTRL_OUT_EP_SETUP_PHASE_DONE |\
			 AHB_ERROR | TRANSFER_DONE)
#define DIEPMSK_INIT	(NON_ISO_IN_EP_TIMEOUT | AHB_ERROR | TRANSFER_DONE \
			| INTKNEPMIS)
#define GAHBCFG_INIT	(USBDEVINIT_DMA_EN | USBDEVINIT_GLBL_INTR_EN\
			 | (USBDEVINIT_HBSTLEN_INCR4 <<\
			 USBDEVINIT_HBSTLEN_SHIFT))

/* Device Endpoint X Transfer Size Register INEPTSFSIZ() */
#define DIEPT_SIZ_PKT_CNT(x)                      (x << 19)
#define DIEPT_SIZ_XFER_SIZE(x)                    (x << 0)

/* Device OUT Endpoint X Transfer Size Register OUTEPTSFSIZ() */
#define DOEPT_SIZ_PKT_CNT(x)                      (x << 19)
#define DOEPT_SIZ_XFER_SIZE(x)                    (x << 0)
#define DOEPT_SIZ_XFER_SIZE_MAX_EP0               (0x7F << 0)
#define DOEPT_SIZ_XFER_SIZE_MAX_EP                (0x7FFF << 0)

/* Device Endpoint-N Control Register INEPCFG()/OUTEPCFG() */
#define DIEPCTL_TX_FIFO_NUM(x)                    (x << 22)
#define DIEPCTL_TX_FIFO_NUM_MASK                  (~DIEPCTL_TX_FIFO_NUM(0xF))

/* Device ALL Endpoints Interrupt Register (USBEPINT) */
#define DAINT_IN_EP_INT(x)                        (x << 0)
#define DAINT_OUT_EP_INT(x)                       (x << 16)

#endif
