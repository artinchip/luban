/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */

#ifndef __AIC_UDC_H__
#define __AIC_UDC_H__

/* --------- AIC usb request ------------ */

struct aic_usb_req {
	struct usb_request		req;
	struct list_head		queue;
	void				*saved_req_buf;
};

/* --------- AIC usb endpoint ------------ */

struct aic_usb_ep {
	struct usb_ep			ep;
	struct aic_usb_gadget		*parent;
	struct list_head		queue;
	struct aic_usb_req		*req;
	struct dentry			*debugfs;

	unsigned char			dir_in;
	unsigned char			index;
	unsigned char			mc;
	u16				interval;
	unsigned short			fifo_size;
	unsigned short			fifo_index;
	unsigned long			total_data;
	unsigned int			size_loaded;
	unsigned int			last_load;

	unsigned int			halted:1;
	unsigned int			periodic:1;
	unsigned int			isochronous:1;
	unsigned int			send_zlp:1;
	unsigned int			target_frame;
#define TARGET_FRAME_INITIAL		0xFFFFFFFF
	bool				frame_overrun;

	char				name[10];
};

/* --------- Global Define ------------ */

/* Size of control and EP0 buffers */
#define CTRL_BUFF_SIZE			8
#define EP0_MPS_LIMIT			64
#define IRQ_RETRY_MASK			(USBINTSTS_NPTXFEMP | \
					USBINTSTS_RXFLVL)
#define DIS_EP_TIMOUT			100

#define aic_gadget_driver_cb(_hs, _entry) \
do { \
	if ((_hs)->gadget.speed != USB_SPEED_UNKNOWN && \
		(_hs)->driver && (_hs)->driver->_entry) { \
		spin_unlock(&_hs->lock); \
		(_hs)->driver->_entry(&(_hs)->gadget); \
		spin_lock(&_hs->lock); \
	} \
} while (0)

/* Gadget ep0 states */
enum aic_ep0_state {
	AIC_EP0_SETUP,
	AIC_EP0_DATA_IN,
	AIC_EP0_DATA_OUT,
	AIC_EP0_STATUS_IN,
	AIC_EP0_STATUS_OUT,
};

/* --------- AIC UDC Parameters ------------ */

#define MAX_EPS_NUM			16
#define EPS_NUM				5
#define PERIOD_IN_EP_NUM		2
#define TOTAL_FIFO_SIZE			0x3f6
#if 1
/* configure 1 */
#define RX_FIFO_SIZE			0x119
#define NP_TX_FIFO_SIZE			0x100
#define PERIOD_TX_FIFO1_SIZE		0x100
#define PERIOD_TX_FIFO2_SIZE		0xDD
#else
/* configure 2 */
#define RX_FIFO_SIZE			0x119
#define NP_TX_FIFO_SIZE			0x200
#define PERIOD_TX_FIFO1_SIZE		0x6E
#define PERIOD_TX_FIFO2_SIZE		0x6E
/* configure 0 */
#define RX_FIFO_SIZE			0x17c
#define NP_TX_FIFO_SIZE			0x258
#define PERIOD_TX_FIFO1_SIZE		0x8
#define PERIOD_TX_FIFO2_SIZE		0x8
#endif
#define EP_DIRS				0x0
/* phy type */
#define AIC_PHY_TYPE_PARAM_FS		0
#define AIC_PHY_TYPE_PARAM_UTMI		1
#define AIC_PHY_TYPE_PARAM_ULPI		2
/* speed */
#define AIC_SPEED_PARAM_HIGH		0
#define AIC_SPEED_PARAM_FULL		1
#define AIC_SPEED_PARAM_LOW		2

#define SYSCFG_USB_RES_CAL_EN_SHIFT		8
#define SYSCFG_USB_RES_CAL_EN_MASK		BIT(8)
#define SYSCFG_USB_RES_CAL_VAL_SHIFT		0
#define SYSCFG_USB_RES_CAL_VAL_MASK		GENMASK(7, 0)
#define SYSCFG_USB_RES_CAL_VAL_DEF		0x40

struct aic_usb_res_cfg {
	void __iomem *addr;
	u32 resis;
};

struct aic_gadget_params {
	struct aic_usb_res_cfg		usb_res_cfg;
	unsigned int			num_ep;
	unsigned int			num_perio_in_ep;
	unsigned int			total_fifo_size;
	unsigned int			rx_fifo_size;
	unsigned int			np_tx_fifo_size;
	unsigned int			p_tx_fifo_size[MAX_EPS_NUM];
	u32				ep_dirs;
	unsigned int			speed;
	/* phy */
	unsigned int			phy_type;
	unsigned int			phy_ulpi_ddr;
	unsigned int			phy_utmi_width;
};

/* --------- AIC gadget ------------ */
#define USB_MAX_CLKS_RSTS 2

struct aic_usb_gadget {
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	struct device			*dev;
	void __iomem			*regs;
	int				irq;
	struct clk			*clks[USB_MAX_CLKS_RSTS];
	struct reset_control		*reset;
	struct reset_control		*reset_ecc;
	struct phy			*phy;
	struct usb_phy			*uphy;
	struct aic_gadget_params	params;
	spinlock_t			lock;

	struct aic_usb_ep		*eps_in[MAX_EPS_NUM];
	struct aic_usb_ep		*eps_out[MAX_EPS_NUM];

	/* ep0 */
	enum aic_ep0_state		ep0_state;
	struct usb_request		*ep0_reply;
	struct usb_request		*ctrl_req;
	void				*ep0_buff;
	void				*ctrl_buff;

	u32				fifo_map;
	u32				tx_fifo_map;
	unsigned int			enabled:1;
	unsigned int			connected:1;
	unsigned int			remote_wakeup_allowed:1;
	unsigned int			delayed_status : 1;

	u8				test_mode;
	u16				frame_number;

	struct dentry			*debug_root;
	struct debugfs_regset32		*regset;
};

/* --------- UDC register define ------------ */

#define UDC_REG(x)	(x)

#define AHBBASIC			UDC_REG(0x000)
#define AHBBASIC_NOTI_ALL_DMA_WRIT	BIT(8)
#define AHBBASIC_REM_MEM_SUPP		BIT(7)
#define AHBBASIC_INV_DESC_ENDIANNESS	BIT(6)
#define AHBBASIC_AHB_SINGLE		BIT(5)
#define AHBBASIC_TXENDDELAY		BIT(3)
#define AHBBASIC_AHBIDLE		BIT(2)
#define AHBBASIC_DMAREQ			BIT(1)

#define USBDEVINIT			UDC_REG(0x004)
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

#define USBPHYIF			UDC_REG(0x008)
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

#define USBULPIPHY			UDC_REG(0x000C)

#define USBINTSTS			UDC_REG(0x010)
#define USBINTMSK			UDC_REG(0x014)
#define USBINTSTS_WKUPINT		BIT(31)
#define USBINTSTS_FET_SUSP		BIT(22)
#define USBINTSTS_INCOMPL_IP		BIT(21)
#define USBINTSTS_INCOMPL_SOOUT		BIT(21)
#define USBINTSTS_INCOMPL_SOIN		BIT(20)
#define USBINTSTS_OEPINT		BIT(19)
#define USBINTSTS_IEPINT		BIT(18)
#define USBINTSTS_EPMIS			BIT(17)
#define USBINTSTS_EOPF			BIT(15)
#define USBINTSTS_ISOUTDROP		BIT(14)
#define USBINTSTS_ENUMDONE		BIT(13)
#define USBINTSTS_USBRST		BIT(12)
#define USBINTSTS_USBSUSP		BIT(11)
#define USBINTSTS_ERLYSUSP		BIT(10)
#define USBINTSTS_ULPI_CK_INT		BIT(8)
#define USBINTSTS_GOUTNAKEFF		BIT(7)
#define USBINTSTS_GINNAKEFF		BIT(6)
#define USBINTSTS_NPTXFEMP		BIT(5)
#define USBINTSTS_RXFLVL		BIT(4)
#define USBINTSTS_SOF			BIT(3)

#define RXFIFOSIZ			UDC_REG(0x018)
#define RXFIFOSIZ_DEPTH_MASK		(0xffff << 0)
#define RXFIFOSIZ_DEPTH_SHIFT		0

#define RXFIFOSTS			UDC_REG(0x01C)
#define RXFIFOSTS_FN_MASK		(0x7f << 25)
#define RXFIFOSTS_FN_SHIFT		25
#define RXFIFOSTS_PKTSTS_MASK		(0xf << 17)
#define RXFIFOSTS_PKTSTS_SHIFT		17
#define RXFIFOSTS_PKTSTS_GLOBALOUTNAK	1
#define RXFIFOSTS_PKTSTS_OUTRX		2
#define RXFIFOSTS_PKTSTS_HCHIN		2
#define RXFIFOSTS_PKTSTS_OUTDONE	3
#define RXFIFOSTS_PKTSTS_HCHIN_XFER_COMP 3
#define RXFIFOSTS_PKTSTS_SETUPDONE	4
#define RXFIFOSTS_PKTSTS_DATATOGGLEERR	5
#define RXFIFOSTS_PKTSTS_SETUPRX	6
#define RXFIFOSTS_PKTSTS_HCHHALTED	7
#define RXFIFOSTS_HCHNUM_MASK		(0xf << 0)
#define RXFIFOSTS_HCHNUM_SHIFT		0
#define RXFIFOSTS_DPID_MASK		(0x3 << 15)
#define RXFIFOSTS_DPID_SHIFT		15
#define RXFIFOSTS_BYTECNT_MASK		(0x7ff << 4)
#define RXFIFOSTS_BYTECNT_SHIFT		4
#define RXFIFOSTS_EPNUM_MASK		(0xf << 0)
#define RXFIFOSTS_EPNUM_SHIFT		0

#define NPTXFIFOSIZ			UDC_REG(0x020)

#define NPTXFIFOSTS			UDC_REG(0x024)

#define NPTXFIFOSTS_NP_TXQ_TOP_MASK		(0x7f << 24)
#define NPTXFIFOSTS_NP_TXQ_TOP_SHIFT		24
#define NPTXFIFOSTS_NP_TXQ_SPC_AVAIL_MASK	(0xff << 16)
#define NPTXFIFOSTS_NP_TXQ_SPC_AVAIL_SHIFT	16
#define NPTXFIFOSTS_NP_TXQ_SPC_AVAIL_GET(_v)	(((_v) >> 16) & 0xff)
#define NPTXFIFOSTS_NP_TXF_SPC_AVAIL_MASK	(0xffff << 0)
#define NPTXFIFOSTS_NP_TXF_SPC_AVAIL_SHIFT	0
#define NPTXFIFOSTS_NP_TXF_SPC_AVAIL_GET(_v)	(((_v) >> 0) & 0xffff)

#define TXFIFOSIZ(_a)			UDC_REG(0x28 + (((_a) - 1) * 4))

#define FIFOSIZE_DEPTH_MASK		(0xffff << 16)
#define FIFOSIZE_DEPTH_SHIFT		16
#define FIFOSIZE_STARTADDR_MASK		(0xffff << 0)
#define FIFOSIZE_STARTADDR_SHIFT	0
#define FIFOSIZE_DEPTH_GET(_x)		(((_x) >> 16) & 0xffff)

#define RXFIFOSTS_DBG			UDC_REG(0x030)

/* Device mode registers */

#define USBDEVCONF			UDC_REG(0x200)
#define USBDEVCONF_DESCDMA_EN		BIT(23)
#define USBDEVCONF_EPMISCNT_MASK	(0x1f << 18)
#define USBDEVCONF_EPMISCNT_SHIFT	18
#define USBDEVCONF_EPMISCNT_LIMIT	0x1f
#define USBDEVCONF_EPMISCNT(_x)		((_x) << 18)
#define USBDEVCONF_IPG_ISOC_SUPPORDED	BIT(17)
#define USBDEVCONF_PERFRINT_MASK	(0x3 << 11)
#define USBDEVCONF_PERFRINT_SHIFT	11
#define USBDEVCONF_PERFRINT_LIMIT	0x3
#define USBDEVCONF_PERFRINT(_x)		((_x) << 11)
#define USBDEVCONF_DEVADDR_MASK		(0x7f << 4)
#define USBDEVCONF_DEVADDR_SHIFT	4
#define USBDEVCONF_DEVADDR_LIMIT	0x7f
#define USBDEVCONF_DEVADDR(_x)		((_x) << 4)
#define USBDEVCONF_NZ_STS_OUT_HSHK	BIT(2)
#define USBDEVCONF_DEVSPD_MASK		(0x3 << 0)
#define USBDEVCONF_DEVSPD_SHIFT		0
#define USBDEVCONF_DEVSPD_HS		0
#define USBDEVCONF_DEVSPD_FS		1
#define USBDEVCONF_DEVSPD_LS		2
#define USBDEVCONF_DEVSPD_FS48		3

#define USBDEVFUNC			UDC_REG(0x204)
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

#define USBLINESTS			UDC_REG(0x208)
#define USBLINESTS_SOFFN_MASK		(0x3fff << 8)
#define USBLINESTS_SOFFN_SHIFT		8
#define USBLINESTS_SOFFN_LIMIT		0x3fff
#define USBLINESTS_SOFFN(_x)		((_x) << 8)
#define USBLINESTS_ERRATICERR		BIT(3)
#define USBLINESTS_ENUMSPD_MASK		(0x3 << 1)
#define USBLINESTS_ENUMSPD_SHIFT	1
#define USBLINESTS_ENUMSPD_HS		0
#define USBLINESTS_ENUMSPD_FS		1
#define USBLINESTS_ENUMSPD_LS		2
#define USBLINESTS_ENUMSPD_FS48		3
#define USBLINESTS_SUSPSTS		BIT(0)

#define INEPINTMSK			UDC_REG(0x20C)
#define INEPINTMSK_NAKMSK		BIT(13)
#define INEPINTMSK_TXFIFOEMPTY		BIT(7)
#define INEPINTMSK_INEPNAKEFFMSK	BIT(6)
#define INEPINTMSK_INTKNEPMISMSK	BIT(5)
#define INEPINTMSK_INTKNTXFEMPMSK	BIT(4)
#define INEPINTMSK_TIMEOUTMSK		BIT(3)
#define INEPINTMSK_AHBERRMSK		BIT(2)
#define INEPINTMSK_EPDISBLDMSK		BIT(1)
#define INEPINTMSK_XFERCOMPLMSK		BIT(0)

#define OUTEPINTMSK			UDC_REG(0x210)
#define OUTEPINTMSK_BACK2BACKSETUP	BIT(6)
#define OUTEPINTMSK_STSPHSERCVDMSK	BIT(5)
#define OUTEPINTMSK_OUTTKNEPDISMSK	BIT(4)
#define OUTEPINTMSK_SETUPMSK		BIT(3)
#define OUTEPINTMSK_AHBERRMSK		BIT(2)
#define OUTEPINTMSK_EPDISBLDMSK		BIT(1)
#define OUTEPINTMSK_XFERCOMPLMSK	BIT(0)

#define USBEPINT			UDC_REG(0x214)
#define USBEPINTMSK			UDC_REG(0x218)
#define USBEPINT_OUTEP_SHIFT		16
#define USBEPINT_OUTEP(_x)		(1 << ((_x) + 16))
#define USBEPINT_INEP(_x)		(1 << (_x))

#define INEPCFG0			UDC_REG(0x220)
#define INEPCFG(_a)			UDC_REG(0x220 + ((_a) * 0x4))

#define OUTEPCFG0			UDC_REG(0x240)
#define OUTEPCFG(_a)			UDC_REG(0x240 + ((_a) * 0x4))
#define EP0CTL_MPS_MASK			(0x3 << 0)
#define EP0CTL_MPS_SHIFT		0
#define EP0CTL_MPS_64			0
#define EP0CTL_MPS_32			1
#define EP0CTL_MPS_16			2
#define EP0CTL_MPS_8			3

#define EPCTL_EPENA			BIT(31)
#define EPCTL_EPDIS			BIT(30)
#define EPCTL_SETD1PID			BIT(29)
#define EPCTL_SETODDFR			BIT(29)
#define EPCTL_SETD0PID			BIT(28)
#define EPCTL_SETEVENFR			BIT(28)
#define EPCTL_SNAK			BIT(27)
#define EPCTL_CNAK			BIT(26)
#define EPCTL_TXFNUM_MASK		(0xf << 22)
#define EPCTL_TXFNUM_SHIFT		22
#define EPCTL_TXFNUM_LIMIT		0xf
#define EPCTL_TXFNUM(_x)		((_x) << 22)
#define EPCTL_STALL			BIT(21)
#define EPCTL_SNP			BIT(20)
#define EPCTL_EPTYPE_MASK		(0x3 << 18)
#define EPCTL_EPTYPE_CONTROL		(0x0 << 18)
#define EPCTL_EPTYPE_ISO		(0x1 << 18)
#define EPCTL_EPTYPE_BULK		(0x2 << 18)
#define EPCTL_EPTYPE_INTERRUPT		(0x3 << 18)
#define EPCTL_NAKSTS			BIT(17)
#define EPCTL_DPID			BIT(16)
#define EPCTL_EOFRNUM			BIT(16)
#define EPCTL_USBACTEP			BIT(15)
#define EPCTL_NEXTEP_MASK		(0xf << 11)
#define EPCTL_NEXTEP_SHIFT		11
#define EPCTL_NEXTEP_LIMIT		0xf
#define EPCTL_NEXTEP(_x)		((_x) << 11)
#define EPCTL_MPS_MASK			(0x7ff << 0)
#define EPCTL_MPS_SHIFT			0
#define EPCTL_MPS_LIMIT			0x7ff
#define EPCTL_MPS(_x)			((_x) << 0)

#define INEPINT(_a)			UDC_REG(0x260 + ((_a) * 0x4))
#define OUTEPINT(_a)			UDC_REG(0x280 + ((_a) * 0x4))
#define EPINT_SETUP_RCVD		BIT(15)
#define EPINT_NYETINTRPT		BIT(14)
#define EPINT_NAKINTRPT			BIT(13)
#define EPINT_BBLEERRINTRPT		BIT(12)
#define EPINT_PKTDRPSTS			BIT(11)
#define EPINT_TXFEMP			BIT(7)
#define EPINT_INEPNAKEFF		BIT(6)
#define EPINT_BACK2BACKSETUP		BIT(6)
#define EPINT_INTKNEPMIS		BIT(5)
#define EPINT_STSPHSERCVD		BIT(5)
#define EPINT_INTKNTXFEMP		BIT(4)
#define EPINT_OUTTKNEPDIS		BIT(4)
#define EPINT_TIMEOUT			BIT(3)
#define EPINT_SETUP			BIT(3)
#define EPINT_AHBERR			BIT(2)
#define EPINT_EPDISBLD			BIT(1)
#define EPINT_XFERCOMPL			BIT(0)

#define INEPTSFSIZ0			UDC_REG(0x2A0)
#define INEPTSFSIZ0_PKTCNT_MASK		(0x3 << 19)
#define INEPTSFSIZ0_PKTCNT_SHIFT	19
#define INEPTSFSIZ0_PKTCNT_LIMIT	0x3
#define INEPTSFSIZ0_PKTCNT(_x)		((_x) << 19)
#define INEPTSFSIZ0_XFERSIZE_MASK	(0x7f << 0)
#define INEPTSFSIZ0_XFERSIZE_SHIFT	0
#define INEPTSFSIZ0_XFERSIZE_LIMIT	0x7f
#define INEPTSFSIZ0_XFERSIZE(_x)	((_x) << 0)

#define OUTEPTSFSIZ0			UDC_REG(0x2C0)
#define OUTEPTSFSIZ0_SUPCNT_MASK	(0x3 << 29)
#define OUTEPTSFSIZ0_SUPCNT_SHIFT	29
#define OUTEPTSFSIZ0_SUPCNT_LIMIT	0x3
#define OUTEPTSFSIZ0_SUPCNT(_x)		((_x) << 29)
#define OUTEPTSFSIZ0_PKTCNT		BIT(19)
#define OUTEPTSFSIZ0_XFERSIZE_MASK	(0x7f << 0)
#define OUTEPTSFSIZ0_XFERSIZE_SHIFT	0

#define INEPTSFSIZ(_a)			UDC_REG(0x2A0 + ((_a) * 0x4))
#define OUTEPTSFSIZ(_a)			UDC_REG(0x2C0 + ((_a) * 0x4))

#define EPTSIZ_MC_MASK			(0x3 << 29)
#define EPTSIZ_MC_SHIFT			29
#define EPTSIZ_MC_LIMIT			0x3
#define EPTSIZ_MC(_x)			((_x) << 29)
#define EPTSIZ_PKTCNT_MASK		(0x3ff << 19)
#define EPTSIZ_PKTCNT_SHIFT		19
#define EPTSIZ_PKTCNT_LIMIT		0x3ff
#define EPTSIZ_PKTCNT_GET(_v)		(((_v) >> 19) & 0x3ff)
#define EPTSIZ_PKTCNT(_x)		((_x) << 19)
#define EPTSIZ_XFERSIZE_MASK		(0x7ffff << 0)
#define EPTSIZ_XFERSIZE_SHIFT		0
#define EPTSIZ_XFERSIZE_LIMIT		0x7ffff
#define EPTSIZ_XFERSIZE_GET(_v)		(((_v) >> 0) & 0x7ffff)
#define EPTSIZ_XFERSIZE(_x)		((_x) << 0)

#define INEPDMAADDR(_a)			UDC_REG(0x300 + ((_a) * 0x4))
#define OUTEPDMAADDR(_a)		UDC_REG(0x320 + ((_a) * 0x4))
#define INEPTXSTS(_a)			UDC_REG(0x340 + ((_a) * 0x4))

#define DTKNQR1				UDC_REG(0x360)
#define DTKNQR2				UDC_REG(0x364)
#define DTKNQR3				UDC_REG(0x368)
#define DTKNQR4				UDC_REG(0x36C)

#define PCGCTL				UDC_REG(0x0040)
#define PCGCTL_STOPPCLK			BIT(0)

#define UDCVERSION			UDC_REG(0x0FFC)

#define EP_FIFO(_a)			UDC_REG(0x1000 + ((_a) * 0x1000))

#endif /* __AIC_UDC_REG_H__ */
