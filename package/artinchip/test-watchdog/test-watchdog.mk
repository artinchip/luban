TEST_WATCHDOG_VERSION =
TEST_WATCHDOG_ENABLE_TARBALL = NO
TEST_WATCHDOG_ENABLE_PATCH = NO

TEST_WATCHDOG_DEPENDENCIES += test-common

TEST_WATCHDOG_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local
$(eval $(cmake-package))
