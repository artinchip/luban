################################################################################
#
# Embed the ubifs image into an ubi image
#
################################################################################


ifeq ($(BR2_UBI_PARAM_USER_CUSTOM),y)
UBI_UBINIZE_OPTS_CUSTOM = -m $(BR2_TARGET_ROOTFS_UBIFS_MINIOSIZE)
UBI_UBINIZE_OPTS_CUSTOM += -p $(BR2_TARGET_ROOTFS_UBI_PEBSIZE)
ifneq ($(BR2_TARGET_ROOTFS_UBI_SUBSIZE),0)
UBI_UBINIZE_OPTS_CUSTOM += -s $(BR2_TARGET_ROOTFS_UBI_SUBSIZE)
endif
UBI_UBINIZE_OPTS_CUSTOM += $(call qstrip,$(BR2_TARGET_ROOTFS_UBI_OPTS))

MK_ROOTFS_UBI_HOOK += MK_ROOTFS_UBI_CMD_CUSTOM
endif

ifeq ($(BR2_UBI_DEVICE_SPI_NAND_2K_128K),y)
UBI_UBINIZE_OPTS_2K_128K = -m 0x800 -p 0x20000
UBI_UBINIZE_OPTS_2K_128K += $(call qstrip,$(BR2_TARGET_ROOTFS_UBI_OPTS))
MK_ROOTFS_UBI_HOOK += MK_ROOTFS_UBI_CMD_2K_128K
endif

ifeq ($(BR2_UBI_DEVICE_SPI_NAND_4K_256K),y)
UBI_UBINIZE_OPTS_4K_256K = -m 0x1000 -p 0x40000
UBI_UBINIZE_OPTS_4K_256K += $(call qstrip,$(BR2_TARGET_ROOTFS_UBI_OPTS))
MK_ROOTFS_UBI_HOOK += MK_ROOTFS_UBI_CMD_4K_256K
endif

ROOTFS_UBI_DEPENDENCIES = rootfs-ubifs

ifeq ($(BR2_TARGET_ROOTFS_UBI_USE_CUSTOM_CONFIG),y)
UBI_UBINIZE_CONFIG_FILE_PATH = $(call qstrip,$(BR2_TARGET_ROOTFS_UBI_CUSTOM_CONFIG_FILE))
else
UBI_UBINIZE_CONFIG_FILE_PATH = package/fs/ubi/ubinize.cfg
endif

define MK_ROOTFS_UBI_CMD_CUSTOM
	sed 's;BR2_ROOTFS_UBIFS_PATH;$@fs;;s;BINARIES_DIR;$(BINARIES_DIR);' \
		$(UBI_UBINIZE_CONFIG_FILE_PATH) > $(BUILD_DIR)/ubinize.custom.cfg
	$(HOST_DIR)/sbin/ubinize -o $@ $(UBI_UBINIZE_OPTS_CUSTOM) $(BUILD_DIR)/ubinize.custom.cfg
	rm $(BUILD_DIR)/ubinize.custom.cfg
endef

define MK_ROOTFS_UBI_CMD_2K_128K
	sed 's;BR2_ROOTFS_UBIFS_PATH;$(subst .ubifs,_page_2k_block_128k.ubifs,$@fs);;s;BINARIES_DIR;$(BINARIES_DIR);' \
		$(UBI_UBINIZE_CONFIG_FILE_PATH) > $(BUILD_DIR)/ubinize.2k_128k.cfg
	$(HOST_DIR)/sbin/ubinize \
		-o $(subst .ubi,_page_2k_block_128k.ubi,$@) \
		$(UBI_UBINIZE_OPTS_2K_128K) \
		$(BUILD_DIR)/ubinize.2k_128k.cfg
endef

define MK_ROOTFS_UBI_CMD_4K_256K
	sed 's;BR2_ROOTFS_UBIFS_PATH;$(subst .ubifs,_page_4k_block_256k.ubifs,$@fs);;s;BINARIES_DIR;$(BINARIES_DIR);' \
		$(UBI_UBINIZE_CONFIG_FILE_PATH) > $(BUILD_DIR)/ubinize.4k_256k.cfg
	$(HOST_DIR)/sbin/ubinize \
		-o $(subst .ubi,_page_4k_block_256k.ubi,$@) \
		$(UBI_UBINIZE_OPTS_4K_256K) \
		$(BUILD_DIR)/ubinize.4k_256k.cfg
endef

# don't use sed -i as it misbehaves on systems with SELinux enabled when this is
# executed through fakeroot (see #9386)
define ROOTFS_UBI_CMD
	$(foreach hook, $(MK_ROOTFS_UBI_HOOK), $(call $(hook))$(sep))
endef

$(eval $(rootfs))
