################################################################################
#
# uboot-tools
#
################################################################################

UBOOT_TOOLS_ENABLE_TARBALL = NO
UBOOT_TOOLS_ENABLE_PATCH = NO
UBOOT_TOOLS_INSTALL_STAGING = YES

UBOOT_TOOLS_ARCH = $(KERNEL_ARCH)
UBOOT_TOOLS_MAKE_OPTS = CROSS_COMPILE="$(TARGET_CROSS)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	ARCH=$(UBOOT_TOOLS_ARCH) \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	HOSTCFLAGS="$(HOST_CFLAGS)" \
	STRIP=$(TARGET_STRIP)

UBOOT_TOOLS_KCONFIG_DEFCONFIG = $(call qstrip,$(BR2_TARGET_UBOOT_BOARD_DEFCONFIG))_defconfig
UBOOT_TOOLS_KCONFIG_EDITORS = menuconfig
UBOOT_TOOLS_KCONFIG_OPTS = $(UBOOT_TOOLS_MAKE_OPTS) HOSTCC="$(HOSTCC_NOCCACHE)" HOSTLDFLAGS=""

define UBOOT_TOOLS_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(BR2_MAKE) -C $(@D) $(UBOOT_TOOLS_MAKE_OPTS) \
		CROSS_BUILD_TOOLS=y tools-only
	$(TARGET_MAKE_ENV) $(BR2_MAKE) -C $(@D) $(UBOOT_TOOLS_MAKE_OPTS) \
		envtools no-dot-config-targets=envtools
endef

define UBOOT_TOOLS_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0755 $(@D)/tools/env/lib.a $(STAGING_DIR)/usr/lib/libubootenv.a
	$(INSTALL) -D -m 0644 $(UBOOT_TOOLS_SRCDIR)/tools/env/fw_env.h $(STAGING_DIR)/usr/include/fw_env.h
endef

define UBOOT_TOOLS_INSTALL_FWPRINTENV
	$(INSTALL) -m 0755 -D $(@D)/tools/env/fw_printenv $(TARGET_DIR)/usr/sbin/fw_printenv
	ln -sf fw_printenv $(TARGET_DIR)/usr/sbin/fw_setenv
endef
define UBOOT_TOOLS_INSTALL_TARGET_CMDS
	$(UBOOT_TOOLS_INSTALL_FWPRINTENV)
endef

################################################################################

HOST_UBOOT_TOOLS_ENABLE_TARBALL = NO
HOST_UBOOT_TOOLS_ENABLE_PATCH = NO

HOST_UBOOT_TOOLS_DEPENDENCIES += host-openssl

HOST_UBOOT_TOOLS_ARCH = $(KERNEL_ARCH)

HOST_UBOOT_TOOLS_MAKE = $(BR2_MAKE)
HOST_UBOOT_TOOLS_MAKE_OPTS += \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	ARCH=$(HOST_UBOOT_TOOLS_ARCH) \
	HOSTCC="$(HOSTCC) $(subst -I/,-isystem /,$(subst -I /,-isystem /,$(HOST_CFLAGS)))" \
	HOSTLDFLAGS="$(HOST_LDFLAGS)"

HOST_UBOOT_TOOLS_KCONFIG_DEFCONFIG = $(call qstrip,$(BR2_TARGET_UBOOT_BOARD_DEFCONFIG))_defconfig
HOST_UBOOT_TOOLS_KCONFIG_EDITORS = menuconfig
HOST_UBOOT_TOOLS_KCONFIG_OPTS = $(HOST_UBOOT_TOOLS_MAKE_OPTS) HOSTCC="$(HOSTCC_NOCCACHE)" HOSTLDFLAGS=""

define HOST_UBOOT_TOOLS_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) \
		PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)" \
		PKG_CONFIG_SYSROOT_DIR="/" \
		PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 \
		PKG_CONFIG_ALLOW_SYSTEM_LIBS=1 \
		PKG_CONFIG_LIBDIR="$(HOST_DIR)/lib/pkgconfig:$(HOST_DIR)/share/pkgconfig" \
		$(HOST_UBOOT_TOOLS_MAKE) -C $(@D) $(HOST_UBOOT_TOOLS_MAKE_OPTS) tools-all
endef

define HOST_UBOOT_TOOLS_INSTALL_CMDS
	$(INSTALL) -m 0755 -D $(@D)/tools/mkimage $(HOST_DIR)/bin/mkimage
	$(INSTALL) -m 0755 -D $(@D)/tools/mkenvimage $(HOST_DIR)/bin/mkenvimage
	$(INSTALL) -m 0755 -D $(@D)/tools/dumpimage $(HOST_DIR)/bin/dumpimage
endef

HOST_UBOOT_TOOLS_MAKE_ENV = $(TARGET_MAKE_ENV)
# Starting with 2021.10, the kconfig in uboot calls the cross-compiler
# to check its capabilities. So we need the toolchain before we can
# call the configurators.
HOST_UBOOT_TOOLS_KCONFIG_DEPENDENCIES += \
	toolchain \
	$(BR2_MAKE_HOST_DEPENDENCY) \
	$(BR2_BISON_HOST_DEPENDENCY) \
	$(BR2_FLEX_HOST_DEPENDENCY)
$(eval $(kconfig-package))
$(eval $(host-kconfig-package))
