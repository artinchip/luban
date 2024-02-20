################################################################################
#
# msn carplay solution
#
################################################################################
MSNLINK_ENABLE_TARBALL = NO
MSNLINK_ENABLE_PATCH = NO

MSNLINK_DEPENDENCIES += aic-mpp

define MSNLINK_INSTALL_TARGET_LIBS
	$(INSTALL) -D -m 644 package/vendor/msnlink/lib/* $(STAGING_DIR)/usr/lib/
	$(INSTALL) -D -m 644 package/vendor/msnlink/lib/* $(TARGET_DIR)/usr/lib/
endef

define MSNLINK_INSTALL_TARGET_INCLUDES
	$(INSTALL) -D -m 644 package/vendor/msnlink/include/* $(STAGING_DIR)/usr/include/
endef

define MSNLINK_INSTALL_TARGET_BINS
	$(INSTALL) -D -m 755 package/vendor/msnlink/bin/* $(TARGET_DIR)/usr/bin/
	$(INSTALL) -D -m 755 package/vendor/msnlink/linksdkenv $(TARGET_DIR)/etc/
	$(INSTALL) -D -m 755 package/vendor/msnlink/S90mdnsd $(TARGET_DIR)/etc/init.d/
endef

define MSNLINK_INSTALL_TARGET_CMDS
	$(MSNLINK_INSTALL_TARGET_LIBS)
	$(MSNLINK_INSTALL_TARGET_INCLUDES)
	$(MSNLINK_INSTALL_TARGET_BINS)
endef

$(eval $(generic-package))

