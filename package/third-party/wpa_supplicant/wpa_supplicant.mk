################################################################################
#
# wpa_supplicant
#
################################################################################

WPA_SUPPLICANT_VERSION = 2.9
WPA_SUPPLICANT_SITE = http://w1.fi/releases
WPA_SUPPLICANT_PATCH = \
	https://w1.fi/security/2020-2/0001-P2P-Fix-copying-of-secondary-device-types-for-P2P-gr.patch \
	https://w1.fi/security/2021-1/0001-P2P-Fix-a-corner-case-in-peer-addition-based-on-PD-R.patch
WPA_SUPPLICANT_LICENSE = BSD-3-Clause
WPA_SUPPLICANT_LICENSE_FILES = README
WPA_SUPPLICANT_CPE_ID_VENDOR = w1.fi
WPA_SUPPLICANT_CONFIG = $(WPA_SUPPLICANT_DIR)/wpa_supplicant/.config
WPA_SUPPLICANT_SUBDIR = wpa_supplicant
WPA_SUPPLICANT_DBUS_SERVICE = fi.w1.wpa_supplicant1
WPA_SUPPLICANT_CFLAGS = $(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include/libnl3/
WPA_SUPPLICANT_LDFLAGS = $(TARGET_LDFLAGS)
WPA_SUPPLICANT_SELINUX_MODULES = networkmanager

# 0001-AP-Silently-ignore-management-frame-from-unexpected-.patch
WPA_SUPPLICANT_IGNORE_CVES += CVE-2019-16275

# 0001-P2P-Fix-a-corner-case-in-peer-addition-based-on-PD-R.patch
WPA_SUPPLICANT_IGNORE_CVES += CVE-2021-27803

# 0002-ASN.1-Validate-DigestAlgorithmIdentifier-parameters.patch
WPA_SUPPLICANT_IGNORE_CVES += CVE-2021-30004

# install the wpa_client library
WPA_SUPPLICANT_INSTALL_STAGING = YES

WPA_SUPPLICANT_CONFIG_EDITS =

WPA_SUPPLICANT_CONFIG_ENABLE = \
	CONFIG_INTERNAL_LIBTOMMATH \
	CONFIG_MATCH_IFACE

WPA_SUPPLICANT_CONFIG_DISABLE = \
	CONFIG_SMARTCARD

# libnl-3 needs -lm (for rint) and -lpthread if linking statically
# And library order matters hence stick -lnl-3 first since it's appended
# in the wpa_supplicant Makefiles as in LIBS+=-lnl-3 ... thus failing
ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_NL80211),y)
ifeq ($(BR2_STATIC_LIBS),y)
WPA_SUPPLICANT_LIBS += -lnl-3 -lm -lpthread
endif
WPA_SUPPLICANT_DEPENDENCIES += host-pkgconf libnl
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_LIBNL32
else
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_DRIVER_NL80211
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_WEXT),)
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_DRIVER_WEXT
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_IBSS_RSN), )
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_IBSS_RSN
endif

# Trailing underscore on purpose to not enable CONFIG_EAPOL_TEST
ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_EAP),y)
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_EAP_
# uses dlopen()
ifeq ($(BR2_STATIC_LIBS),y)
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_EAP_TNC
endif
else
WPA_SUPPLICANT_CONFIG_DISABLE += \
	CONFIG_EAP \
	CONFIG_IEEE8021X_EAPOL \
	CONFIG_FILS
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_WIRED),)
WPA_SUPPLICANT_CONFIG_DISABLE += \
	CONFIG_DRIVER_WIRED \
	CONFIG_MACSEC \
	CONFIG_DRIVER_MACSEC
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_HOTSPOT),)
WPA_SUPPLICANT_CONFIG_DISABLE += \
	CONFIG_HS20 \
	CONFIG_INTERWORKING
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_AP_SUPPORT),y)
WPA_SUPPLICANT_CONFIG_ENABLE += \
	CONFIG_AP \
	CONFIG_P2P
else
WPA_SUPPLICANT_CONFIG_DISABLE += \
	CONFIG_AP \
	CONFIG_P2P
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_WIFI_DISPLAY),y)
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_WIFI_DISPLAY
else
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_WIFI_DISPLAY
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_MESH_NETWORKING),y)
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_MESH
else
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_MESH
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_AUTOSCAN),y)
WPA_SUPPLICANT_CONFIG_ENABLE += \
	CONFIG_AUTOSCAN_EXPONENTIAL \
	CONFIG_AUTOSCAN_PERIODIC
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_WPS),)
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_WPS
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_WPA3),y)
WPA_SUPPLICANT_CONFIG_ENABLE += \
	CONFIG_DPP \
	CONFIG_SAE \
	CONFIG_OWE
else
WPA_SUPPLICANT_CONFIG_DISABLE += \
	CONFIG_DPP \
	CONFIG_SAE \
	CONFIG_OWE
endif

# Try to use openssl if it's already available
ifeq ($(BR2_PACKAGE_LIBOPENSSL),y)
WPA_SUPPLICANT_DEPENDENCIES += host-pkgconf libopenssl
WPA_SUPPLICANT_LIBS += `$(PKG_CONFIG_HOST_BINARY) --libs openssl`
WPA_SUPPLICANT_CONFIG_EDITS += 's/\#\(CONFIG_TLS=openssl\)/\1/'
else
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_EAP_PWD CONFIG_EAP_TEAP
WPA_SUPPLICANT_CONFIG_EDITS += 's/\#\(CONFIG_TLS=\).*/\1internal/'
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_CTRL_IFACE),)
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_CTRL_IFACE\>
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_DBUS),y)
WPA_SUPPLICANT_DEPENDENCIES += host-pkgconf dbus
WPA_SUPPLICANT_MAKE_ENV = \
	PKG_CONFIG_SYSROOT_DIR="$(STAGING_DIR)" \
	PKG_CONFIG_PATH="$(STAGING_DIR)/usr/lib/pkgconfig"
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_CTRL_IFACE_DBUS_NEW
define WPA_SUPPLICANT_INSTALL_DBUS_NEW
	$(INSTALL) -m 0644 -D \
		$(@D)/wpa_supplicant/dbus/$(WPA_SUPPLICANT_DBUS_SERVICE).service \
		$(TARGET_DIR)/usr/share/dbus-1/system-services/$(WPA_SUPPLICANT_DBUS_SERVICE).service
endef

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_DBUS_INTROSPECTION),y)
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_CTRL_IFACE_DBUS_INTRO
endif

else
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_CTRL_IFACE_DBUS_NEW
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_DEBUG_SYSLOG),)
WPA_SUPPLICANT_CONFIG_DISABLE += CONFIG_DEBUG_SYSLOG
endif

ifeq ($(BR2_PACKAGE_READLINE),y)
WPA_SUPPLICANT_DEPENDENCIES += readline
WPA_SUPPLICANT_CONFIG_ENABLE += CONFIG_READLINE
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_WPA_CLIENT_SO),y)
WPA_SUPPLICANT_CONFIG_SET += CONFIG_BUILD_WPA_CLIENT_SO
define WPA_SUPPLICANT_INSTALL_WPA_CLIENT_SO
	$(INSTALL) -m 0644 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/libwpa_client.so \
		$(TARGET_DIR)/usr/lib/libwpa_client.so
	$(INSTALL) -m 0644 -D $(@D)/src/common/wpa_ctrl.h \
		$(TARGET_DIR)/usr/include/wpa_ctrl.h
endef
define WPA_SUPPLICANT_INSTALL_STAGING_WPA_CLIENT_SO
	$(INSTALL) -m 0644 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/libwpa_client.so \
		$(STAGING_DIR)/usr/lib/libwpa_client.so
	$(INSTALL) -m 0644 -D $(@D)/src/common/wpa_ctrl.h \
		$(STAGING_DIR)/usr/include/wpa_ctrl.h
endef
endif

define WPA_SUPPLICANT_CONFIGURE_CMDS
	cp $(@D)/wpa_supplicant/defconfig $(WPA_SUPPLICANT_CONFIG)
	sed -i $(patsubst %,-e 's/^#\(%\)/\1/',$(WPA_SUPPLICANT_CONFIG_ENABLE)) \
		$(patsubst %,-e 's/^\(%\)/#\1/',$(WPA_SUPPLICANT_CONFIG_DISABLE)) \
		$(patsubst %,-e '1i%=y',$(WPA_SUPPLICANT_CONFIG_SET)) \
		$(patsubst %,-e %,$(WPA_SUPPLICANT_CONFIG_EDITS)) \
		$(WPA_SUPPLICANT_CONFIG)
endef

# LIBS for wpa_supplicant, LIBS_c for wpa_cli, LIBS_p for wpa_passphrase
define WPA_SUPPLICANT_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(WPA_SUPPLICANT_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" BINDIR=/usr/sbin \
		LIBS="$(WPA_SUPPLICANT_LIBS)" LIBS_c="$(WPA_SUPPLICANT_LIBS)" \
		LIBS_p="$(WPA_SUPPLICANT_LIBS)" \
		$(MAKE) CC="$(TARGET_CC)" -C $(@D)/$(WPA_SUPPLICANT_SUBDIR)
	cp $(@D)/.stamp_* $(@D)/$(WPA_SUPPLICANT_SUBDIR)/
endef

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_CLI),y)
define WPA_SUPPLICANT_INSTALL_CLI
	$(INSTALL) -m 0755 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/wpa_cli \
		$(TARGET_DIR)/usr/sbin/wpa_cli
endef
endif

ifeq ($(BR2_PACKAGE_WPA_SUPPLICANT_PASSPHRASE),y)
define WPA_SUPPLICANT_INSTALL_PASSPHRASE
	$(INSTALL) -m 0755 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/wpa_passphrase \
		$(TARGET_DIR)/usr/sbin/wpa_passphrase
endef
endif

ifeq ($(BR2_PACKAGE_DBUS),y)
define WPA_SUPPLICANT_INSTALL_DBUS
	$(INSTALL) -m 0644 -D \
		$(@D)/wpa_supplicant/dbus/dbus-wpa_supplicant.conf \
		$(TARGET_DIR)/etc/dbus-1/system.d/wpa_supplicant.conf
	$(WPA_SUPPLICANT_INSTALL_DBUS_NEW)
endef
endif

define WPA_SUPPLICANT_INSTALL_STAGING_CMDS
	$(WPA_SUPPLICANT_INSTALL_STAGING_WPA_CLIENT_SO)
endef

define WPA_SUPPLICANT_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/wpa_supplicant \
		$(TARGET_DIR)/usr/sbin/wpa_supplicant
	$(INSTALL) -m 644 -D package/third-party/wpa_supplicant/wpa_supplicant.conf \
		$(TARGET_DIR)/etc/wpa_supplicant.conf
	$(WPA_SUPPLICANT_INSTALL_CLI)
	$(WPA_SUPPLICANT_INSTALL_PASSPHRASE)
	$(WPA_SUPPLICANT_INSTALL_DBUS)
	$(WPA_SUPPLICANT_INSTALL_WPA_CLIENT_SO)
endef

define WPA_SUPPLICANT_INSTALL_INIT_SYSTEMD
	$(INSTALL) -m 0644 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/systemd/wpa_supplicant.service \
		$(TARGET_DIR)/usr/lib/systemd/system/wpa_supplicant.service
	$(INSTALL) -m 0644 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/systemd/wpa_supplicant@.service \
		$(TARGET_DIR)/usr/lib/systemd/system/wpa_supplicant@.service
	$(INSTALL) -m 0644 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/systemd/wpa_supplicant-nl80211@.service \
		$(TARGET_DIR)/usr/lib/systemd/system/wpa_supplicant-nl80211@.service
	$(INSTALL) -m 0644 -D $(@D)/$(WPA_SUPPLICANT_SUBDIR)/systemd/wpa_supplicant-wired@.service \
		$(TARGET_DIR)/usr/lib/systemd/system/wpa_supplicant-wired@.service
	$(INSTALL) -D -m 644 $(WPA_SUPPLICANT_PKGDIR)/50-wpa_supplicant.preset \
		$(TARGET_DIR)/usr/lib/systemd/system-preset/50-wpa_supplicant.preset
endef

$(eval $(generic-package))
