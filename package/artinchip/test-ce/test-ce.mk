TEST_CE_VERSION =
TEST_CE_ENABLE_TARBALL = NO
TEST_CE_ENABLE_PATCH = NO

TEST_CE_DEPENDENCIES += libkcapi libopenssl test-common

TEST_CE_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local
$(eval $(cmake-package))

