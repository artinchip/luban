################################################################################
#
# Build the ubifs root filesystem image
#
################################################################################

ifeq ($(BR2_UBI_PARAM_USER_CUSTOM),y)
UBIFS_OPTS_CUSTOM = \
	-e $(BR2_TARGET_ROOTFS_UBIFS_LEBSIZE) \
	-c $(BR2_TARGET_ROOTFS_UBIFS_MAXLEBCNT) \
	-m $(BR2_TARGET_ROOTFS_UBIFS_MINIOSIZE)

MK_UBIFS_HOOK += MK_UBIFS_USER_CUSTOM
endif

UBIFS_SIZE_DEC = $(shell printf "%d" $(BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE))
ifeq ($(BR2_UBI_DEVICE_SPI_NAND_2K_128K),y)
UBIFS_LEBCNT_2K_128K = $(shell echo "$(UBIFS_SIZE_DEC) / 131072 " | bc)
UBIFS_OPTS_2K_128K = \
	-e 0x1f000 \
	-m 0x800 \
	-c $(UBIFS_LEBCNT_2K_128K)

MK_UBIFS_HOOK += MK_UBIFS_PAGE_2K_BLOCK_128K
endif

ifeq ($(BR2_UBI_DEVICE_SPI_NAND_4K_256K),y)
UBIFS_LEBCNT_4K_256K = $(shell echo "$(UBIFS_SIZE_DEC) / 262144" | bc)
UBIFS_OPTS_4K_256K = \
	-e 0x3e000 \
	-m 0x1000 \
	-c $(UBIFS_LEBCNT_4K_256K)

MK_UBIFS_HOOK += MK_UBIFS_PAGE_4K_BLOCK_256K
endif

ifeq ($(BR2_TARGET_ROOTFS_UBIFS_RT_ZLIB),y)
UBIFS_OPTS += -x zlib
endif
ifeq ($(BR2_TARGET_ROOTFS_UBIFS_RT_LZO),y)
UBIFS_OPTS += -x lzo
endif
ifeq ($(BR2_TARGET_ROOTFS_UBIFS_RT_NONE),y)
UBIFS_OPTS += -x none
endif

UBIFS_OPTS += $(call qstrip,$(BR2_TARGET_ROOTFS_UBIFS_OPTS))

ROOTFS_UBIFS_DEPENDENCIES = host-mtd

define MK_UBIFS_PAGE_2K_BLOCK_128K
	$(HOST_DIR)/sbin/mkfs.ubifs \
		-d $(TARGET_DIR) $(UBIFS_OPTS_2K_128K) $(UBIFS_OPTS) \
		-o $(subst .ubifs,_page_2k_block_128k.ubifs,$@)
endef

define MK_UBIFS_PAGE_4K_BLOCK_256K
	$(HOST_DIR)/sbin/mkfs.ubifs \
		-d $(TARGET_DIR) $(UBIFS_OPTS_4K_256K) $(UBIFS_OPTS) \
		-o $(subst .ubifs,_page_4k_block_256k.ubifs,$@)
endef

define MK_UBIFS_USER_CUSTOM
	$(HOST_DIR)/sbin/mkfs.ubifs \
		-d $(TARGET_DIR) $(UBIFS_OPTS_CUSTOM) $(UBIFS_OPTS) -o $@
endef

define ROOTFS_UBIFS_CMD
	$(foreach hook, $(MK_UBIFS_HOOK), $(call $(hook))$(sep))
endef

define MK_UBIFS_PAGE_2K_BLOCK_128K_COMPRESS
	$(Q)PATH=$(BR_PATH) $(ROOTFS_UBIFS_COMPRESS_CMD) \
		$(subst .ubifs,_page_2k_block_128k.ubifs,$@) > \
		$(subst .ubifs,_page_2k_block_128k.ubifs,$@)$(ROOTFS_UBIFS_COMPRESS_EXT)
endef

define MK_UBIFS_PAGE_4K_BLOCK_256K_COMPRESS
	$(Q)PATH=$(BR_PATH) $(ROOTFS_UBIFS_COMPRESS_CMD) \
		$(subst .ubifs,_page_4k_block_256k.ubifs,$@) > \
		$(subst .ubifs,_page_4k_block_256k.ubifs,$@)$(ROOTFS_UBIFS_COMPRESS_EXT)
endef

define MK_UBIFS_USER_CUSTOM_COMPRESS
	$(Q)PATH=$(BR_PATH) $(ROOTFS_UBIFS_COMPRESS_CMD) $@ > $@$(ROOTFS_UBIFS_COMPRESS_EXT)
endef

ROOTFS_UBIFS_COMPRESS_HOOKS += MK_UBIFS_PAGE_2K_BLOCK_128K_COMPRESS
ROOTFS_UBIFS_COMPRESS_HOOKS += MK_UBIFS_PAGE_4K_BLOCK_256K_COMPRESS

ifeq ($(BR2_UBI_PARAM_USER_CUSTOM),y)
ROOTFS_UBIFS_COMPRESS_HOOKS += MK_UBIFS_USER_CUSTOM_COMPRESS
endif

$(eval $(rootfs))
