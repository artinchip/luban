################################################################################
#
# udev
#
################################################################################

# Required by default rules for input devices
define UDEV_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/etc/udev/rules.d/

	$(INSTALL) -m 0755 -D package/third-party/udev/11-mount-usb.rules \
		$(TARGET_DIR)/etc/udev/rules.d/
endef
$(eval $(virtual-package))
