AIC8800_FW_VERSION = 1.0
AIC8800_FW_SOURCE = aic8800_fw-$(AIC8800_FW_VERSION).tar.gz
AIC8800_FW_SITE = aic8800_fw
AIC8800_FW_ENABLE_TARBALL = YES
AIC8800_FW_ENABLE_PATCH = NO
AIC8800_FW_INSTALL_STAGING = YES
AIC8800_FW_DEPENDENCIES = linux

AIC8800_FW_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/etc/firmware

$(eval $(cmake-package))

