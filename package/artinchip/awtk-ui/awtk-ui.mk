AWTK_UI_VERSION =
AWTK_UI_ENABLE_TARBALL = NO
AWTK_UI_ENABLE_PATCH = NO
AWTK_UI_INSTALL_STAGING = YES

AWTK_UI_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local

define TEST_AWTK_POST_TARGET_INSTALL
    @$(call MESSAGE,"post target install")
    $(INSTALL) -m 0755 -D package/artinchip/awtk-ui/S00test_awtk \
        $(TARGET_DIR)/etc/init.d/S00awtk

endef

AWTK_UI_POST_INSTALL_TARGET_HOOKS += TEST_AWTK_POST_TARGET_INSTALL

$(eval $(cmake-package))

