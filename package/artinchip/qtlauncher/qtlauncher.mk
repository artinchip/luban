################################################################################
#
# qt-launcher
#
################################################################################
QTLAUNCHER_ENABLE_TARBALL = NO
QTLAUNCHER_ENABLE_PATCH = NO

QTLAUNCHER_DEPENDENCIES += qt directfb

QTLAUNCHER_CONF_OPTS = $(QTLAUNCHER_SRCDIR)/QtLauncher.pro

ifeq ($(BR2_QTLAUNCHER_GE_SUPPORT),y)
export QTLAUNCHER_GE_SUPPORT = YES
endif
ifeq ($(BR2_QTLAUNCHER_SMALL_MEMORY),y)
export QTLAUNCHER_SMALL_MEMORY = YES
endif

define QTLAUNCHER_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/local/launcher/
	cp -a $(@D)/qtlauncher $(TARGET_DIR)/usr/local/launcher/

	$(INSTALL) -m 0755 -D package/artinchip/qtlauncher/S99qtlauncher \
		$(TARGET_DIR)/etc/init.d/S99qtlauncher
endef

$(eval $(qmake-package))

