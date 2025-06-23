################################################################################
#
# uboot
#
################################################################################

UBOOT_VERSION = $(call qstrip,$(BR2_TARGET_UBOOT_VERSION))
UBOOT_BOARD_NAME = $(call qstrip,$(BR2_TARGET_UBOOT_BOARDNAME))

# Handle stable official U-Boot versions
UBOOT_SITE = https://ftp.denx.de/pub/u-boot
UBOOT_SOURCE = u-boot-$(UBOOT_VERSION).tar.bz2

UBOOT_LICENSE = GPL-2.0+
ifeq ($(BR2_TARGET_UBOOT_LATEST_VERSION),y)
UBOOT_LICENSE_FILES = Licenses/gpl-2.0.txt
endif
UBOOT_CPE_ID_VENDOR = denx
UBOOT_CPE_ID_PRODUCT = u-boot

UBOOT_INSTALL_IMAGES = YES
UBOOT_ENABLE_TARBALL = NO
UBOOT_ENABLE_PATCH = NO
UBOOT_ADD_LINUX_HEADERS_DEPENDENCY = NO
UBOOT_GEN_PREBUILT_TARBALL = NO
UBOOT_USE_PREBUILT_TARBALL = NO

# u-boot 2020.01+ needs make 4.0+
UBOOT_DEPENDENCIES = host-pkgconf $(BR2_MAKE_HOST_DEPENDENCY)
UBOOT_MAKE = $(BR2_MAKE)

ifeq ($(BR2_TARGET_UBOOT_FORMAT_BIN),y)
UBOOT_BINS += u-boot.bin
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_DTB),y)
UBOOT_BINS += u-boot.dtb
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_ELF),y)
UBOOT_BINS += u-boot
endif

# Call 'make all' unconditionally
UBOOT_MAKE_TARGET += all

ifeq ($(BR2_TARGET_UBOOT_FORMAT_DTB_IMG),y)
UBOOT_BINS += u-boot-dtb.img
UBOOT_MAKE_TARGET += u-boot-dtb.img
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_DTB_BIN),y)
UBOOT_BINS += u-boot-dtb.bin
UBOOT_MAKE_TARGET += u-boot-dtb.bin
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_NODTB_BIN),y)
UBOOT_BINS += u-boot-nodtb.bin
UBOOT_MAKE_TARGET += u-boot-nodtb.bin
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_IMG),y)
UBOOT_BINS += u-boot.img
UBOOT_MAKE_TARGET += u-boot.img
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_ITB),y)
UBOOT_BINS += u-boot.itb
UBOOT_MAKE_TARGET += u-boot.itb
endif

ifeq ($(BR2_TARGET_UBOOT_FORMAT_CUSTOM),y)
UBOOT_BINS += $(call qstrip,$(BR2_TARGET_UBOOT_FORMAT_CUSTOM_NAME))
endif

# The kernel calls AArch64 'arm64', but U-Boot calls it just 'arm', so
# we have to special case it. Similar for i386/x86_64 -> x86
ifeq ($(KERNEL_ARCH),arm64)
UBOOT_ARCH = arm
else ifneq ($(filter $(KERNEL_ARCH),i386 x86_64),)
UBOOT_ARCH = x86
else
UBOOT_ARCH = $(KERNEL_ARCH)
endif

UBOOT_MAKE_OPTS += \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	ARCH=$(UBOOT_ARCH) \
	HOSTCC="$(HOSTCC) $(subst -I/,-isystem /,$(subst -I /,-isystem /,$(HOST_CFLAGS)))" \
	HOSTLDFLAGS="$(HOST_LDFLAGS)" \
	$(call qstrip,$(BR2_TARGET_UBOOT_CUSTOM_MAKEOPTS))

ifeq ($(BR2_TARGET_UBOOT_NEEDS_OPENSBI),y)
UBOOT_DEPENDENCIES += opensbi
UBOOT_MAKE_OPTS += OPENSBI=$(BINARIES_DIR)/fw_dynamic.bin
endif

ifeq ($(BR2_TARGET_UBOOT_NEEDS_DTC),y)
UBOOT_DEPENDENCIES += host-dtc
endif

ifeq ($(BR2_TARGET_UBOOT_NEEDS_PYTHON2),y)
UBOOT_DEPENDENCIES += host-python host-python-setuptools
else ifeq ($(BR2_TARGET_UBOOT_NEEDS_PYTHON3),y)
UBOOT_DEPENDENCIES += host-python3 host-python3-setuptools
endif

ifeq ($(BR2_TARGET_UBOOT_NEEDS_PYLIBFDT),y)
UBOOT_DEPENDENCIES += host-swig
endif

ifeq ($(BR2_TARGET_UBOOT_NEEDS_PYELFTOOLS),y)
ifeq ($(BR2_TARGET_UBOOT_NEEDS_PYTHON2),y)
UBOOT_DEPENDENCIES += host-python-pyelftools
else ifeq ($(BR2_TARGET_UBOOT_NEEDS_PYTHON3),y)
UBOOT_DEPENDENCIES += host-python3-pyelftools
endif
endif

ifeq ($(BR2_TARGET_UBOOT_NEEDS_OPENSSL),y)
UBOOT_DEPENDENCIES += host-openssl
endif

ifeq ($(BR2_TARGET_UBOOT_NEEDS_LZOP),y)
UBOOT_DEPENDENCIES += host-lzop
endif


# Analogous code exists in linux/linux.mk. Basically, the generic
# package infrastructure handles downloading and applying remote
# patches. Local patches are handled depending on whether they are
# directories or files.
UBOOT_PATCHES = $(call qstrip,$(BR2_TARGET_UBOOT_PATCH))
UBOOT_PATCH = $(filter ftp://% http://% https://%,$(UBOOT_PATCHES))

define UBOOT_APPLY_LOCAL_PATCHES
	$(Q)for p in $(filter-out ftp://% http://% https://%,$(UBOOT_PATCHES)) ; do \
		if test -d $$p ; then \
			$(APPLY_PATCHES) $(@D) $$p \*.patch || exit 1 ; \
		else \
			$(APPLY_PATCHES) $(@D) `dirname $$p` `basename $$p` || exit 1; \
		fi \
	done
endef
UBOOT_POST_PATCH_HOOKS += UBOOT_APPLY_LOCAL_PATCHES

# Fixup inclusion of libfdt headers, which can fail in older u-boot versions
# when libfdt-devel is installed system-wide.
# The core change is equivalent to upstream commit
# e0d20dc1521e74b82dbd69be53a048847798a90a (first in v2018.03). However, the fixup
# is complicated by the fact that the underlying u-boot code changed multiple
# times in history:
# - The directory scripts/dtc/libfdt only exists since upstream commit
#   c0e032e0090d6541549b19cc47e06ccd1f302893 (first in v2017.11). For earlier
#   versions, create a dummy scripts/dtc/libfdt directory with symlinks for the
#   fdt-related files. This allows to use the same -I<path> option for both
#   cases.
# - The variable 'srctree' used to be called 'SRCTREE' before upstream commit
#   01286329b27b27eaeda045b469d41b1d9fce545a (first in v2014.04).
# - The original location for libfdt, 'lib/libfdt/', used to be simply
#   'libfdt' before upstream commit 0de71d507157c4bd4fddcd3a419140d2b986eed2
#   (first in v2010.06). Make the 'lib' part optional in the substitution to
#   handle this.
define UBOOT_FIXUP_LIBFDT_INCLUDE
	$(Q)if [ ! -d $(@D)/scripts/dtc/libfdt ]; then \
		mkdir -p $(@D)/scripts/dtc/libfdt; \
		cd $(@D)/scripts/dtc/libfdt; \
		ln -s ../../../include/fdt.h .; \
		ln -s ../../../include/libfdt*.h .; \
		ln -s ../../../lib/libfdt/libfdt_internal.h .; \
	fi
	$(Q)$(SED) \
		's%-I\ *\$$(srctree)/lib/libfdt%-I$$(srctree)/scripts/dtc/libfdt%; \
		s%-I\ *\$$(SRCTREE)\(/lib\)\?/libfdt%-I$$(SRCTREE)/scripts/dtc/libfdt%' \
		$(@D)/tools/Makefile
endef
UBOOT_POST_PATCH_HOOKS += UBOOT_FIXUP_LIBFDT_INCLUDE

ifeq ($(BR2_TARGET_UBOOT_USE_DEFCONFIG),y)
UBOOT_KCONFIG_DEFCONFIG = $(call qstrip,$(BR2_TARGET_UBOOT_BOARD_DEFCONFIG))_defconfig
else ifeq ($(BR2_TARGET_UBOOT_USE_CUSTOM_CONFIG),y)
UBOOT_KCONFIG_FILE = $(call qstrip,$(BR2_TARGET_UBOOT_CUSTOM_CONFIG_FILE))
endif # BR2_TARGET_UBOOT_USE_DEFCONFIG

UBOOT_KCONFIG_EDITORS = menuconfig

# UBOOT_MAKE_OPTS overrides HOSTCC / HOSTLDFLAGS to allow the build to
# find our host-openssl. However, this triggers a bug in the kconfig
# build script that causes it to build with /usr/include/ncurses.h
# (which is typically wchar) but link with
# $(HOST_DIR)/lib/libncurses.so (which is not).  We don't actually
# need any host-package for kconfig, so remove the HOSTCC/HOSTLDFLAGS
# override again. In addition, host-ccache is not ready at kconfig
# time, so use HOSTCC_NOCCACHE.
UBOOT_KCONFIG_OPTS = $(UBOOT_MAKE_OPTS) HOSTCC="$(HOSTCC_NOCCACHE)" HOSTLDFLAGS=""

UBOOT_CUSTOM_DTS_PATH = $(call qstrip,$(BR2_TARGET_UBOOT_CUSTOM_DTS_PATH))

define UBOOT_PREPARE_DTS
	$(Q)ln -sf $(TARGET_CHIP_DIR)/common/*.dtsi $(UBOOT_SRCDIR)/arch/$(KERNEL_ARCH)/dts/
	$(Q)ln -sf $(TARGET_BOARD_DIR)/board.dts $(UBOOT_SRCDIR)/arch/$(KERNEL_ARCH)/dts/artinchip-board.dts
	$(Q)ln -sf $(TARGET_BOARD_DIR)/image_cfg.json $(UBOOT_SRCDIR)/include/configs/image_cfg.json
	$(Q)ln -sf $(LINUX_SRCDIR)/include/dt-bindings/clock/artinchip*.h $(UBOOT_SRCDIR)/include/dt-bindings/clock/
	$(Q)ln -sf $(LINUX_SRCDIR)/include/dt-bindings/reset/artinchip*.h $(UBOOT_SRCDIR)/include/dt-bindings/reset/
	$(Q)ln -sf $(LINUX_SRCDIR)/include/dt-bindings/display/artinchip*.h $(UBOOT_SRCDIR)/include/dt-bindings/display/
	$(Q)if [ -f $(TARGET_BOARD_DIR)/board-u-boot.dtsi ]; then \
		ln -sf $(TARGET_BOARD_DIR)/board-u-boot.dtsi $(UBOOT_SRCDIR)/arch/$(KERNEL_ARCH)/dts/artinchip-board-u-boot.dtsi; \
	else \
		rm -rf $(UBOOT_SRCDIR)/arch/$(KERNEL_ARCH)/dts/artinchip-board-u-boot.dtsi; \
	fi
	$(Q)if [ -f $(TARGET_BOARD_DIR)/u-boot.its.dtsi ]; then \
		ln -sf $(TARGET_BOARD_DIR)/u-boot.its.dtsi $(UBOOT_SRCDIR)/arch/$(KERNEL_ARCH)/dts/u-boot.its.dtsi; \
	fi
	$(Q)if [ -f $(TARGET_BOARD_DIR)/kernel.its.dtsi ]; then \
		ln -sf $(TARGET_BOARD_DIR)/kernel.its.dtsi $(UBOOT_SRCDIR)/arch/$(KERNEL_ARCH)/dts/kernel.its.dtsi; \
	fi
endef

define UBOOT_BUILD_CMDS
	$(if $(UBOOT_CUSTOM_DTS_PATH),
		cp -f $(UBOOT_CUSTOM_DTS_PATH) $(@D)/arch/$(UBOOT_ARCH)/dts/
	)
	$(UBOOT_PREPARE_DTS)
	$(Q)$(TARGET_CONFIGURE_OPTS) \
		PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)" \
		PKG_CONFIG_SYSROOT_DIR="/" \
		PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 \
		PKG_CONFIG_ALLOW_SYSTEM_LIBS=1 \
		PKG_CONFIG_LIBDIR="$(HOST_DIR)/lib/pkgconfig:$(HOST_DIR)/share/pkgconfig" \
		$(UBOOT_MAKE) -C $(@D) $(UBOOT_MAKE_OPTS) \
		$(UBOOT_MAKE_TARGET)
endef

ifeq ($(BR2_TARGET_INSTALL_ETC_CONFIG),y)
define UBOOT_INSTALL_DOT_CONFIG_TO_TARGET
	$(Q)if [ -f $(TARGET_CHIP_DIR)/common/module.csv ]; then \
		$(INSTALL) -m 0644 -D $(TARGET_CHIP_DIR)/common/module.csv $(TARGET_DIR)/etc/config/.module.csv; \
	fi
	$(Q)$(INSTALL) -m 0644 -D $(@D)/.config $(TARGET_DIR)/etc/config/uboot.config
endef
endif

define UBOOT_INSTALL_CMDS
	$(INSTALL) -m 0755 -D $(@D)/tools/mkimage $(HOST_DIR)/bin/mkimage
	$(INSTALL) -m 0755 -D $(@D)/tools/mkeficapsule $(HOST_DIR)/bin/mkeficapsule
	$(INSTALL) -m 0755 -D $(@D)/tools/mkenvimage $(HOST_DIR)/bin/mkenvimage
	$(INSTALL) -m 0755 -D $(@D)/tools/dumpimage $(HOST_DIR)/bin/dumpimage
endef

define UBOOT_INSTALL_IMAGES_CMDS
	$(UBOOT_INSTALL_DOT_CONFIG_TO_TARGET)
	$(foreach f,$(UBOOT_BINS), \
			$(Q)cp -dpf $(@D)/$(f) $(BINARIES_DIR)/
	)
	$(if $(BR2_TARGET_UBOOT_SPL),
		$(foreach f,$(call qstrip,$(BR2_TARGET_UBOOT_SPL_NAME)), \
			$(Q)cp -dpf $(@D)/$(f) $(BINARIES_DIR)/
		)
	)
	$(Q)if [ -f $(@D)/u-boot.its ]; then \
		cp -dpf $(@D)/u-boot.its $(BINARIES_DIR)/; \
	fi
	$(Q)if [ -f $(@D)/kernel.its ]; then \
		cp -dpf $(@D)/kernel.its $(BINARIES_DIR)/; \
	fi
	$(Q)cd $(BINARIES_DIR) && echo In $(BINARIES_DIR) && \
		ls -og --time-style=iso u-boot*
endef

ifeq ($(BR2_TARGET_UBOOT)$(BR_BUILDING),yy)
#
# Check U-Boot board name (for legacy) or the defconfig/custom config
# file options (for kconfig)
#
ifeq ($(BR2_TARGET_UBOOT_USE_DEFCONFIG),y)
ifeq ($(call qstrip,$(BR2_TARGET_UBOOT_BOARD_DEFCONFIG)),)
$(error No board defconfig name specified, check your BR2_TARGET_UBOOT_BOARD_DEFCONFIG setting)
endif # qstrip BR2_TARGET_UBOOT_BOARD_DEFCONFIG
endif # BR2_TARGET_UBOOT_USE_DEFCONFIG
ifeq ($(BR2_TARGET_UBOOT_USE_CUSTOM_CONFIG),y)
ifeq ($(call qstrip,$(BR2_TARGET_UBOOT_CUSTOM_CONFIG_FILE)),)
$(error No board configuration file specified, check your BR2_TARGET_UBOOT_CUSTOM_CONFIG_FILE setting)
endif # qstrip BR2_TARGET_UBOOT_CUSTOM_CONFIG_FILE
endif # BR2_TARGET_UBOOT_USE_CUSTOM_CONFIG

endif # BR2_TARGET_UBOOT && BR_BUILDING

UBOOT_MAKE_ENV = $(TARGET_MAKE_ENV)
# Starting with 2021.10, the kconfig in uboot calls the cross-compiler
# to check its capabilities. So we need the toolchain before we can
# call the configurators.
UBOOT_KCONFIG_DEPENDENCIES += \
	toolchain \
	$(BR2_MAKE_HOST_DEPENDENCY) \
	$(BR2_BISON_HOST_DEPENDENCY) \
	$(BR2_FLEX_HOST_DEPENDENCY)
$(eval $(kconfig-package))
