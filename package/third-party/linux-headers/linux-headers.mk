################################################################################
#
# linux-headers
#
################################################################################

# This package is used to provide Linux kernel headers for the
# internal toolchain backend.

LINUX_HEADERS_INSTALL_STAGING = YES
LINUX_HEADERS_ENABLE_TARBALL = NO
LINUX_HEADERS_ENABLE_PATCH = NO
LINUX_HEADERS_GEN_PREBUILT_TARBALL = NO
LINUX_HEADERS_ADD_LINUX_HEADERS_DEPENDENCY = NO
LINUX_HEADERS_SUPPORTS_OUT_SOURCE_BUILD = YES

LINUX_HEADERS_SRCDIR = $(LINUX_SRCDIR)/include/uapi/

define LINUX_HEADERS_INSTALL_STAGING_CMDS
	(cd $(@D); \
		$(TARGET_MAKE_ENV) $(MAKE) \
			-C $(LINUX_SRCDIR) \
			O=$(@D) \
			ARCH=$(KERNEL_ARCH) \
			HOSTCC="$(HOSTCC)" \
			HOSTCFLAGS="$(HOSTCFLAGS)" \
			HOSTCXX="$(HOSTCXX)" \
			INSTALL_HDR_PATH=$(STAGING_DIR)/usr \
			headers_install)
endef

$(eval $(generic-package))
