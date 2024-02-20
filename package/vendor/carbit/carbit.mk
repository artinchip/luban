################################################################################
#
# carbit wifi screen solution
#
################################################################################
CARBIT_ENABLE_TARBALL = NO
CARBIT_ENABLE_PATCH = NO

CARBIT_DEPENDENCIES += aic-mpp

define CARBIT_INSTALL_TARGET_LIBS
	$(INSTALL) -D -m 644 package/vendor/carbit/lib/* $(STAGING_DIR)/usr/lib/
	$(INSTALL) -D -m 644 package/vendor/carbit/lib/* $(TARGET_DIR)/usr/lib/
endef

define CARBIT_INSTALL_TARGET_INCLUDES
	$(INSTALL) -D -m 644 package/vendor/carbit/include/* $(STAGING_DIR)/usr/include/
endef

define CARBIT_INSTALL_TARGET_CMDS
	$(CARBIT_INSTALL_TARGET_LIBS)
	$(CARBIT_INSTALL_TARGET_INCLUDES)
endef

$(eval $(generic-package))
