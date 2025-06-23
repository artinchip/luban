################################################################################
#
# libopenssl
#
################################################################################

LIVE555_VERSION = 2024.05.15
LIVE555_SITE = http://www.live555.com/liveMedia/public
LIVE555_SOURCE = live.$(LIVE555_VERSION).tar.gz
LIVE555_LICENSE_FILES = COPYING
LIVE555_INSTALL_STAGING = YES

ifeq ($(BR2_PACKAGE_LIVE555_DEBUG),y)
	LIVE555_CONF_OPTS += -enable-debug
endif
ifeq ($(BR2_PACKAGE_LIVE555_SANITIZE),y)
	LIVE555_CONF_OPTS += -enable-sanitize
endif

ifeq ($(BR2_PACKAGE_LIVE555_OPENSSL),y)
	LIVE555_CONF_OPTS += -enable-ssl
else
	LIVE555_CONF_OPTS += -disable-ssl
endif

ifeq ($(BR2_PACKAGE_LIVE555_TESTPROGS),y)
	LIVE555_CONF_OPTS += -enable-testProgs
else
	LIVE555_CONF_OPTS += -disable-testProgs
endif

ifeq ($(BR2_PACKAGE_LIVE555_MEDIASERVER),y)
	LIVE555_CONF_OPTS += -enable-mediaServer
else
	LIVE555_CONF_OPTS += -disable-mediaServer
endif

ifeq ($(BR2_PACKAGE_LIVE555_PROXYSERVER),y)
	LIVE555_CONF_OPTS += -enable-proxyServer
else
	LIVE555_CONF_OPTS += -disable-proxyServer
endif

ifeq ($(BR2_PACKAGE_LIVE555_HLSPROXY),y)
	LIVE555_CONF_OPTS += -enable-hlsProxy
else
	LIVE555_CONF_OPTS += -disable-hlsProxy
endif


ifeq ($(BR2_PACKAGE_LIVE555_STATIC),y)
	LIVE555_CONF_OPTS += -static
else
	LIVE555_CONF_OPTS += -shared
endif


define LIVE555_CONFIGURE_CMDS
	(cd $(@D) && chmod +x $(@D)/preGenMakefiles.sh &&\
	$(TARGET_CONFIGURE_OPTS) \
	$(TARGET_CONFIGURE_ARGS) \
	$(@D)/preGenMakefiles.sh $(LIVE555_CONF_OPTS) \
	)
endef

define LIVE555_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)
endef

define LIVE555_INSTALL_STAGING_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
endef

define LIVE555_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) DESTDIR=$(TARGET_DIR) install
endef

$(eval $(generic-package))
$(eval $(host-generic-package))
