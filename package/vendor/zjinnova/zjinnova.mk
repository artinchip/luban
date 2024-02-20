################################################################################
#
# zjinnova carplay solution
#
################################################################################
ZJINNOVA_ENABLE_TARBALL = NO
ZJINNOVA_ENABLE_PATCH = NO

ZJINNOVA_DEPENDENCIES += aic-mpp

define ZJINNOVA_INSTALL_TARGET_LIBS
	mkdir -p $(TARGET_DIR)/usr/lib/zjinnova/
	$(INSTALL) -D -m 644 package/vendor/zjinnova/lib/* $(STAGING_DIR)/usr/lib/
	$(INSTALL) -D -m 644 package/vendor/zjinnova/lib/* $(TARGET_DIR)/usr/lib/zjinnova/
endef

define ZJINNOVA_INSTALL_TARGET_INCLUDES
endef

define ZJINNOVA_INSTALL_TARGET_BINS
	$(INSTALL) -D -m 755 package/vendor/zjinnova/bin/* $(TARGET_DIR)/usr/bin/
	$(INSTALL) -D -m 755 package/vendor/zjinnova/zlink.sh $(TARGET_DIR)/usr/bin
endef

define ZJINNOVA_INSTALL_TARGET_CMDS
	$(ZJINNOVA_INSTALL_TARGET_LIBS)
	$(ZJINNOVA_INSTALL_TARGET_INCLUDES)
	$(ZJINNOVA_INSTALL_TARGET_BINS)
endef

$(eval $(generic-package))

