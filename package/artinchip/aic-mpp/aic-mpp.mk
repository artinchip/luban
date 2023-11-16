AIC_MPP_VERSION =
AIC_MPP_ENABLE_TARBALL = NO
AIC_MPP_ENABLE_PATCH = NO
AIC_MPP_INSTALL_STAGING = YES

AIC_MPP_DEPENDENCIES += test-common
ifeq ("$(BR2_riscv)","y")
AIC_MPP_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local -DARCH=RISCV
endif
ifeq ("$(BR2_arm)","y")
AIC_MPP_CONF_OPTS += -DCMAKE_INSTALL_PREFIX=/usr/local -DARCH=ARM
endif

ifeq ($(BR2_PACKAGE_AIC_MPP_MIDDLEWARE),y)
AIC_MPP_DEPENDENCIES += libmad alsa-lib
AIC_MPP_CONF_OPTS += -DMIDDLEWARE=enable
endif

ifeq ($(BR2_PACKAGE_AIC_MPP_AAC_DECODER),y)
AIC_MPP_DEPENDENCIES += faad2
AIC_MPP_CONF_OPTS += -DAAC_DECODER=enable
endif

ifeq ($(BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE), "6.5")
AIC_MPP_CONF_OPTS += -DLINUX_VERSION_6=enable
endif

define AIC_MPP_REMOVE_HEADERS_IN_TARGET
	rm -rf $(TARGET_DIR)/usr/local/include/
endef
AIC_MPP_POST_INSTALL_TARGET_HOOKS += AIC_MPP_REMOVE_HEADERS_IN_TARGET

$(eval $(cmake-package))