################################################################################
#
# fscrypt
#
################################################################################

FSCRYPT_VERSION = 0.3.4
FSCRYPT_SOURCE = fscrypt-$(FSCRYPT_VERSION).tar.gz
FSCRYPT_SITE = $(call github,fscrypt,fscrypt,$(FSCRYPT_VERSION))

FSCRYPT_DEPENDENCIES = linux-pam

FSCRYPT_LICENSE = GPL-2.0+
FSCRYPT_LICENSE_FILES = COPYING
FSCRYPT_LDFLAGS = -X main.version=$(FSCRYPT_VERSION)
#FSCRYPT_INSTALL_STAGING = YES
#FSCRYPT_SUPPORTS_OUT_SOURCE_BUILD = NO
FSCRYPT_GOMOD = ./
FSCRYPT_BUILD_TARGETS = \
	cmd/fscrypt

$(eval $(golang-package))
