################################################################################
#
# initscripts
#
################################################################################

INITSCRIPTS_ENABLE_TARBALL = NO
INITSCRIPTS_ENABLE_PATCH = NO
INITSCRIPTS_GEN_PREBUILT_TARBALL = NO

INITSCRIPTS += package/third-party/initscripts/init.d/rcK
INITSCRIPTS += package/third-party/initscripts/init.d/rcS
INITSCRIPTS += package/third-party/initscripts/init.d/S00_show_boot_time

ifeq ($(BR2_PACKAGE_ALSA_UTILS),y)
INITSCRIPTS += package/third-party/initscripts/init.d/S70audiocfg
endif

ifeq ($(BR2_PACKAGE_ANDROID_TOOLS_ADBD),y)
INITSCRIPTS += package/third-party/initscripts/init.d/adbd
endif

define INITSCRIPTS_INSTALL_TARGET_CMDS
	mkdir -p  $(TARGET_DIR)/etc/init.d
	$(foreach f,$(INITSCRIPTS), \
			$(INSTALL) -D -m 0755 $(f) $(TARGET_DIR)/etc/init.d/
	)
	@if test -f $(TARGET_DIR)/etc/init.d/adbd; then \
		cat $(TARGET_DIR)/etc/init.d/adbd |\
		sed 's/0123456789ABCDEF/$(LUBAN_CURRENT_OUT)/'\
		> $(TARGET_DIR)/etc/init.d/S90adbd && \
		chmod +x $(TARGET_DIR)/etc/init.d/S90adbd && \
		rm $(TARGET_DIR)/etc/init.d/adbd; fi
	$(INSTALL) -D -m 0755 package/third-party/initscripts/init $(TARGET_DIR)/
endef

$(eval $(generic-package))
