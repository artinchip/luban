// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/io.h>
#include <linux/delay.h>

#include "aicmac_hwtstamp.h"
#include "aicmac_1588.h"

void aicmac_hwtstamp_config_hw_tstamping(void __iomem *ioaddr, u32 data)
{
	writel(data, ioaddr + PTP_TMSTMP_CTL);
}

void aicmac_hwtstamp_config_sub_second_increment(void __iomem *ioaddr,
						 u32 ptp_clock, u32 *ssinc)
{
	u32 value = readl(ioaddr + PTP_TMSTMP_CTL);
	unsigned long data;
	u32 reg_value;

	/* For GMAC3.x, 4.x versions, in "fine adjustement mode" set sub-second
	 * increment to twice the number of nanoseconds of a clock cycle.
	 * The calculation of the default_addend value by the caller will set it
	 * to mid-range = 2^31 when the remainder of this division is zero,
	 * which will make the accumulator overflow once every 2 ptp_clock
	 * cycles, adding twice the number of nanoseconds of a clock cycle :
	 * 2000000000ULL / ptp_clock.
	 */
	if (value & PTP_TCR_TSCFUPDT)
		data = (2000000000ULL / ptp_clock);
	else
		data = (1000000000ULL / ptp_clock);

	/* 0.465ns accuracy */
	if (!(value & PTP_TCR_TSCTRLSSR))
		data = (data * 1000) / 465;

	data &= PTP_SSIR_SSINC_MASK;

	reg_value = data;

	writel(reg_value, ioaddr + PTP_SUB_SEC_INCR);

	if (ssinc)
		*ssinc = data;
}

int aicmac_hwtstamp_init_systime(void __iomem *ioaddr, u32 sec, u32 nsec)
{
	int limit;
	u32 value;

	writel(sec, ioaddr + PTP_UPDT_TIME_SEC);
	writel(nsec, ioaddr + PTP_UPDT_TIME_NANO_SEC);
	/* issue command to initialize the system time value */
	value = readl(ioaddr + PTP_TMSTMP_CTL);
	value |= PTP_TCR_TSINIT;
	writel(value, ioaddr + PTP_TMSTMP_CTL);

	/* wait for present system time initialize to complete */
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + PTP_TMSTMP_CTL) & PTP_TCR_TSINIT))
			break;
		mdelay(10);
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

int aicmac_hwtstamp_config_addend(void __iomem *ioaddr, u32 addend)
{
	u32 value;
	int limit;

	writel(addend, ioaddr + PTP_TMSMP_ADDEND);
	/* issue command to update the addend value */
	value = readl(ioaddr + PTP_TMSTMP_CTL);
	value |= PTP_TCR_TSADDREG;
	writel(value, ioaddr + PTP_TMSTMP_CTL);

	/* wait for present addend update to complete */
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + PTP_TMSTMP_CTL) & PTP_TCR_TSADDREG))
			break;
		mdelay(10);
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

int aicmac_hwtstamp_adjust_systime(void __iomem *ioaddr, u32 sec, u32 nsec,
				   int add_sub)
{
	u32 value;
	int limit;

	if (add_sub) {
		/* If the new sec value needs to be subtracted with
		 * the system time, then MAC_STSUR reg should be
		 * programmed with (2^32 – <new_sec_value>)
		 */
		value = readl(ioaddr + PTP_TMSTMP_CTL);
		if (value & PTP_TCR_TSCTRLSSR)
			nsec = (PTP_DIGITAL_ROLLOVER_MODE - nsec);
		else
			nsec = (PTP_BINARY_ROLLOVER_MODE - nsec);
	}

	writel(sec, ioaddr + PTP_UPDT_TIME_SEC);
	value = (add_sub << PTP_STNSUR_ADDSUB_SHIFT) | nsec;
	writel(value, ioaddr + PTP_UPDT_TIME_NANO_SEC);

	/* issue command to initialize the system time value */
	value = readl(ioaddr + PTP_TMSTMP_CTL);
	value |= PTP_TCR_TSUPDT;
	writel(value, ioaddr + PTP_TMSTMP_CTL);

	/* wait for present system time adjust/update to complete */
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + PTP_TMSTMP_CTL) & PTP_TCR_TSUPDT))
			break;
		mdelay(10);
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

void aicmac_hwtstamp_get_systime(void __iomem *ioaddr, u64 *systime)
{
	u64 ns;

	/* Get the TSSS value */
	ns = readl(ioaddr + PTP_SYS_TIME_NANO_SEC);
	/* Get the TSS and convert sec time value to nanosecond */
	ns += readl(ioaddr + PTP_SYS_TIME_SEC) * 1000000000ULL;

	if (systime)
		*systime = ns;
}

