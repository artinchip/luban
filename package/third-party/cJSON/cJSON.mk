################################################################################
#
# CJSON
#
################################################################################

CJSON_VERSION = 1.7.16
CJSON_SOURCE = cJSON-$(CJSON_VERSION).tar.gz
CJSON_SITE = https://github.com/DaveGamble/cJSON
CJSON_LICENSE = GPL-2.0+
CJSON_LICENSE_FILES = COPYING
CJSON_CPE_ID_VENDOR = CJSON_project
CJSON_INSTALL_STAGING = YES
CJSON_SUPPORTS_IN_SOURCE_BUILD = NO

ifeq ($(BR2_SHARED_LIBS)$(BR2_SHARED_STATIC_LIBS),y)
CJSON_CONF_OPTS += -DENABLE_SHARED=ON
else
CJSON_CONF_OPTS += -DENABLE_SHARED=OFF
endif

ifeq ($(BR2_STATIC_LIBS)$(BR2_SHARED_STATIC_LIBS),y)
CJSON_CONF_OPTS += -DENABLE_STATIC=ON
else
CJSON_CONF_OPTS += -DENABLE_STATIC=OFF
endif

HOST_CJSON_CONF_OPTS += -DENABLE_SHARED=ON -DENABLE_STATIC=OFF

$(eval $(cmake-package))
