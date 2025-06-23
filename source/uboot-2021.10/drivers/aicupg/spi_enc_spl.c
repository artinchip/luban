#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <reset.h>
#include <misc.h>
#include <artinchip/aic_spienc.h>
#include "spi_enc_spl.h"

#ifdef CONFIG_ARTINCHIP_SPIENC
/*
 * Default SPIENC use user provide tweak value,
 * but for SPL Read/Write, should select hardware tweak value.
 */
void spi_enc_tweak_select(long sel)
{
	struct udevice *dev = NULL;

	uclass_first_device_err(UCLASS_MISC, &dev);
	do {
		if (device_is_compatible(dev, "artinchip,aic-spienc-v1.0"))
			break;
		else
			dev = NULL;
		uclass_next_device_err(&dev);
	} while (dev);
	if (dev)
		misc_ioctl(dev, AIC_SPIENC_IOCTL_TWEAK_SELECT, (void *)sel);
}

void spi_enc_set_bypass(long status)
{
	struct udevice *dev = NULL;

	uclass_first_device_err(UCLASS_MISC, &dev);
	do {
		if (device_is_compatible(dev, "artinchip,aic-spienc-v1.0"))
			break;
		else
			dev = NULL;
		uclass_next_device_err(&dev);
	} while (dev);
	if (dev)
		misc_ioctl(dev, AIC_SPIENC_IOCTL_BYPASS, (void *)status);
}
#endif

