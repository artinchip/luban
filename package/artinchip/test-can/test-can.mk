TEST_CAN_VERSION =
TEST_CAN_ENABLE_TARBALL = NO
TEST_CAN_ENABLE_PATCH = NO

TEST_CAN_DEPENDENCIES += test-common

TEST_CAN_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local
$(eval $(cmake-package))