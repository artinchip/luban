################################################################################
#
# lvgl for linux
#
################################################################################

LVGL_VERSION = 8.3.2
LVGL_SITE = https://github.com/lvgl/lvgl
LVGL_LICENSE = MIT
LVGL_LICENSE_FILES = COPYING

LVGL_INSTALL_STAGING = YES
LVGL_DEPENDENCIES = host-pkgconf

LVGL_APPLICATION = source/artinchip/test-lvgl

define LVGL_PRE_BUILD
	@$(call MESSAGE,"pre build")
	$(INSTALL) -D -m 0644 $(LVGL_APPLICATION)/lv_conf.h $(STAGING_DIR)/usr/include/ 
endef

define LVGL_POST_TARGET_INSTALL
	@$(call MESSAGE,"post target install")
	$(RM) -r $(TARGET_DIR)/usr/local/include/lvgl
endef

LVGL_PRE_BUILD_HOOKS += LVGL_PRE_BUILD
LVGL_POST_INSTALL_TARGET_HOOKS += LVGL_POST_TARGET_INSTALL

LVGL_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local

$(eval $(cmake-package))
