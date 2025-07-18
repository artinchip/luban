# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

ifndef CONFIG_SPL_BUILD

obj-$(CONFIG_EFI) += efi/
obj-$(CONFIG_EFI_LOADER) += efi_driver/
obj-$(CONFIG_EFI_LOADER) += efi_loader/
obj-$(CONFIG_CMD_BOOTEFI_SELFTEST) += efi_selftest/
obj-$(CONFIG_XZ) += decompress_unxz.o
obj-$(CONFIG_XZ) += xz/
obj-$(CONFIG_LZMA) += lzma/
obj-$(CONFIG_BZIP2) += bzip2/
obj-$(CONFIG_TIZEN) += tizen/
obj-$(CONFIG_FIT) += libfdt/
obj-$(CONFIG_OF_LIVE) += of_live.o
obj-$(CONFIG_CMD_DHRYSTONE) += dhry/
obj-$(CONFIG_ARCH_AT91) += at91/
obj-$(CONFIG_OPTEE) += optee/
obj-$(CONFIG_ASN1_DECODER) += asn1_decoder.o
obj-y += crypto/

obj-$(CONFIG_$(SPL_TPL_)BINMAN_FDT) += binman.o

ifndef API_BUILD
ifneq ($(CONFIG_CHARSET),)
obj-y += charset.o
endif
endif
obj-$(CONFIG_USB_TTY) += circbuf.o
obj-y += crc8.o
obj-y += crc16.o
obj-$(CONFIG_ERRNO_STR) += errno_str.o
obj-$(CONFIG_FIT) += fdtdec_common.o
obj-$(CONFIG_TEST_FDTDEC) += fdtdec_test.o
obj-$(CONFIG_GZIP_COMPRESSED) += gzip.o
obj-$(CONFIG_GENERATE_SMBIOS_TABLE) += smbios.o
obj-$(CONFIG_SMBIOS_PARSER) += smbios-parser.o
obj-$(CONFIG_IMAGE_SPARSE) += image-sparse.o
obj-y += ldiv.o
obj-$(CONFIG_XXHASH) += xxhash.o
obj-y += net_utils.o
obj-$(CONFIG_PHYSMEM) += physmem.o
obj-y += rc4.o
obj-$(CONFIG_SUPPORT_EMMC_RPMB) += sha256.o
obj-$(CONFIG_RBTREE)	+= rbtree.o
obj-$(CONFIG_BITREVERSE) += bitrev.o
obj-y += list_sort.o
obj-$(CONFIG_SHA1) += sha1.o
obj-$(CONFIG_SHA256) += sha256.o
obj-$(CONFIG_SHA512) += sha512.o
endif

obj-$(CONFIG_$(SPL_TPL_)TPM) += tpm-common.o
ifeq ($(CONFIG_$(SPL_TPL_)TPM),y)
obj-y += crc8.o
obj-$(CONFIG_TPM) += tpm_api.o
obj-$(CONFIG_TPM_V1) += tpm-v1.o
obj-$(CONFIG_TPM_V2) += tpm-v2.o
endif

obj-$(CONFIG_AES) += aes.o
obj-$(CONFIG_AES) += aes/

obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi/
obj-$(CONFIG_$(SPL_)MD5) += md5.o
obj-$(CONFIG_ECDSA) += ecdsa/
obj-$(CONFIG_$(SPL_)RSA) += rsa/
obj-$(CONFIG_HASH) += hash-checksum.o
obj-$(CONFIG_CRYPT_PW) += crypt/

obj-$(CONFIG_$(SPL_)ZLIB) += zlib/
obj-$(CONFIG_$(SPL_)ZSTD) += zstd/
obj-$(CONFIG_$(SPL_)GZIP) += gunzip.o
obj-$(CONFIG_$(SPL_)LZO) += lzo/
obj-$(CONFIG_$(SPL_)XZ) += decompress_unxz.o
obj-$(CONFIG_$(SPL_)XZ) += xz/
obj-$(CONFIG_$(SPL_)LZMA) += lzma/
obj-$(CONFIG_$(SPL_)LZ4) += lz4_wrapper.o

obj-$(CONFIG_$(SPL_)LIB_RATIONAL) += rational.o

obj-$(CONFIG_LIBAVB) += libavb/

obj-$(CONFIG_$(SPL_TPL_)OF_LIBFDT) += libfdt/
ifneq ($(CONFIG_$(SPL_TPL_)BUILD)$(CONFIG_$(SPL_TPL_)OF_PLATDATA),yy)
obj-$(CONFIG_$(SPL_TPL_)OF_CONTROL) += fdtdec_common.o
obj-$(CONFIG_$(SPL_TPL_)OF_CONTROL) += fdtdec.o
endif

ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_SPL_YMODEM_SUPPORT) += crc16.o
obj-$(CONFIG_$(SPL_TPL_)HASH) += crc16.o
obj-y += net_utils.o
endif
obj-$(CONFIG_ADDR_MAP) += addr_map.o
obj-y += qsort.o
obj-y += hashtable.o
obj-y += errno.o
obj-y += display_options.o
CFLAGS_display_options.o := $(if $(BUILD_TAG),-DBUILD_TAG='"$(BUILD_TAG)"')
obj-$(CONFIG_BCH) += bch.o
obj-$(CONFIG_MMC_SPI) += crc7.o
#ifndef CONFIG_TPL_BUILD
obj-y += crc32.o
#endif
obj-$(CONFIG_CRC32C) += crc32c.o
obj-y += ctype.o
obj-y += div64.o
obj-$(CONFIG_$(SPL_TPL_)OF_LIBFDT) += fdtdec.o fdtdec_common.o
obj-y += hang.o
obj-y += linux_compat.o
obj-y += linux_string.o
obj-$(CONFIG_LMB) += lmb.o
obj-y += membuff.o
obj-$(CONFIG_REGEX) += slre.o
obj-y += string.o
obj-y += tables_csum.o
obj-y += time.o
obj-y += hexdump.o
obj-$(CONFIG_GETOPT) += getopt.o
obj-$(CONFIG_TRACE) += trace.o
obj-$(CONFIG_LIB_UUID) += uuid.o
obj-$(CONFIG_LIB_RAND) += rand.o
obj-y += panic.o
obj-y += config_parse.o

ifeq ($(CONFIG_$(SPL_TPL_)BUILD),y)
# SPL U-Boot may use full-printf, tiny-printf or none at all
ifdef CONFIG_$(SPL_TPL_)USE_TINY_PRINTF
obj-$(CONFIG_$(SPL_TPL_)SPRINTF) += tiny-printf.o
else
obj-$(CONFIG_$(SPL_TPL_)SPRINTF) += vsprintf.o
endif
obj-$(CONFIG_$(SPL_TPL_)STRTO) += strto.o
else
# Main U-Boot always uses the full printf support
obj-y += vsprintf.o strto.o
obj-$(CONFIG_OID_REGISTRY) += oid_registry.o
obj-$(CONFIG_SSCANF) += sscanf.o
endif

obj-y += date.o
obj-y += rtc-lib.o
obj-$(CONFIG_LIB_ELF) += elf.o

#
# Build a fast OID lookup registry from include/linux/oid_registry.h
#
$(obj)/oid_registry.o: $(obj)/oid_registry_data.c

$(obj)/oid_registry_data.c: $(srctree)/include/linux/oid_registry.h \
			    $(srctree)/scripts/build_OID_registry
	$(call cmd,build_OID_registry)

quiet_cmd_build_OID_registry = GEN     $@
      cmd_build_OID_registry = perl $(srctree)/scripts/build_OID_registry $< $@

clean-files     += oid_registry_data.c

subdir-ccflags-$(CONFIG_CC_OPTIMIZE_LIBS_FOR_SPEED) += -O2
