OPENSSL_HWENGINE_VERSION =
OPENSSL_HWENGINE_ENABLE_TARBALL = NO
OPENSSL_HWENGINE_ENABLE_PATCH = NO

OPENSSL_HWENGINE_DEPENDENCIES += libkcapi libopenssl

define ADD_HWENGINE_TO_OPENSSL_ENGINES
	ln -sf /usr/lib/libengine_aic.so $(TARGET_DIR)/usr/lib/engines-1.1/aic.so
	ln -sf /usr/lib/libengine_huk.so $(TARGET_DIR)/usr/lib/engines-1.1/huk.so
endef

OPENSSL_HWENGINE_POST_INSTALL_TARGET_HOOKS += ADD_HWENGINE_TO_OPENSSL_ENGINES

$(eval $(cmake-package))
