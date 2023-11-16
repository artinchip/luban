################################################################################
#
# makedevs
#
################################################################################

MAKEDEVS_LICENSE = GPL-2.0

HOST_MAKEDEVS_CFLAGS = $(HOST_CFLAGS)
HOST_MAKEDEVS_LDFLAGS = $(HOST_LDFLAGS)

# makedevs supports to configure alternate build directory
HOST_MAKEDEVS_SUPPORTS_OUT_SOURCE_BUILD = YES

ifeq ($(BR2_ROOTFS_DEVICE_TABLE_SUPPORTS_EXTENDED_ATTRIBUTES),y)
HOST_MAKEDEVS_DEPENDENCIES += host-libcap
HOST_MAKEDEVS_CFLAGS += -DEXTENDED_ATTRIBUTES
HOST_MAKEDEVS_LDFLAGS += -lcap
endif

define HOST_MAKEDEVS_EXTRACT_CMDS
	cp $(HOST_MAKEDEVS_PKGDIR)/makedevs.c $(HOST_MAKEDEVS_SRCDIR)
endef

define HOST_MAKEDEVS_BUILD_CMDS
	cp $(HOST_MAKEDEVS_PKGDIR)/makedevs.c $(HOST_MAKEDEVS_SRCDIR)
	$(HOSTCC) $(HOST_MAKEDEVS_CFLAGS) $(HOST_MAKEDEVS_SRCDIR)/makedevs.c \
		-o $(@D)/makedevs $(HOST_MAKEDEVS_LDFLAGS)
endef

define HOST_MAKEDEVS_INSTALL_CMDS
	$(INSTALL) -D -m 755 $(@D)/makedevs $(HOST_DIR)/bin/makedevs
endef

$(eval $(host-generic-package))
