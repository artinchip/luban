# SPDX-License-Identifier:	GPL-2.0

obj-y += upg_main.o
obj-y += basic_cmd.o
obj-y += fwc_cmd.o
obj-y += ram_fwc.o
obj-y += spi_enc_spl.o
obj-y += upg_sdcard.o upg_fat.o
obj-$(CONFIG_AICUPG_NAND_ARTINCHIP) += nand_fwc.o
obj-$(CONFIG_AICUPG_NOR_ARTINCHIP) += nor_fwc.o
obj-$(CONFIG_AICUPG_MMC_ARTINCHIP) += mmc_fwc.o

ifdef CONFIG_TARGET_D211
obj-$(CONFIG_AICUPG_NAND_ARTINCHIP) += nand_fwc_spl.o
endif

