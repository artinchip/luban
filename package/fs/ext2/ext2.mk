################################################################################
#
# Build the ext2 root filesystem image
#
################################################################################

ROOTFS_EXT2_SIZE = $(call qstrip,$(BR2_TARGET_ROOTFS_EXT2_SIZE))
ifeq ($(BR2_TARGET_ROOTFS_EXT2)-$(ROOTFS_EXT2_SIZE),y-)
$(error BR2_TARGET_ROOTFS_EXT2_SIZE cannot be empty)
endif

ROOTFS_EXT2_MKFS_OPTS = $(call qstrip,$(BR2_TARGET_ROOTFS_EXT2_MKFS_OPTIONS))

# qstrip results in stripping consecutive spaces into a single one. So the
# variable is not qstrip-ed to preserve the integrity of the string value.
ROOTFS_EXT2_LABEL = $(subst ",,$(BR2_TARGET_ROOTFS_EXT2_LABEL))
#" Syntax highlighting... :-/ )

ROOTFS_EXT2_OPTS = \
	-d $(TARGET_DIR) \
	-r $(BR2_TARGET_ROOTFS_EXT2_REV) \
	-N $(BR2_TARGET_ROOTFS_EXT2_INODES) \
	-m $(BR2_TARGET_ROOTFS_EXT2_RESBLKS) \
	-L "$(ROOTFS_EXT2_LABEL)" \
	$(ROOTFS_EXT2_MKFS_OPTS)

ROOTFS_EXT2_DEPENDENCIES = host-e2fsprogs
ifeq ($(BR2_TARGET_ROOTFS_EXT2_TO_SPARSE),y)
ROOTFS_EXT2_DEPENDENCIES = host-android-tools
endif

define ROOTFS_EXT2_CMD
	rm -f $@
	MKE2FS_CONFIG=$(HOST_DIR)/etc/mke2fs.conf $(HOST_DIR)/sbin/mkfs.ext$(BR2_TARGET_ROOTFS_EXT2_GEN) $(ROOTFS_EXT2_OPTS) $@ \
		"$(ROOTFS_EXT2_SIZE)" \
	|| { ret=$$?; \
	     echo "*** Maybe you need to increase the filesystem size (BR2_TARGET_ROOTFS_EXT2_SIZE)" 1>&2; \
	     exit $$ret; \
	}
endef

ifeq ($(BR2_TARGET_ROOTFS_EXT2_TO_SPARSE),y)
define ROOTFS_SPARSE_CMD
	rm -f $(BINARIES_DIR)/rootfs.sparse
	$(HOST_DIR)/bin/img2simg $@ $(BINARIES_DIR)/rootfs.sparse \
	|| { ret=$$?; \
	     echo "*** ext2 image format to sparse image format failed"; \
	     exit $$ret; \
	}
endef
endif

ifneq ($(BR2_TARGET_ROOTFS_EXT2_GEN),2)
define ROOTFS_EXT2_SYMLINK
	ln -sf rootfs.ext2$(ROOTFS_EXT2_COMPRESS_EXT) $(BINARIES_DIR)/rootfs.ext$(BR2_TARGET_ROOTFS_EXT2_GEN)$(ROOTFS_EXT2_COMPRESS_EXT)
endef
ROOTFS_EXT2_POST_GEN_HOOKS += ROOTFS_EXT2_SYMLINK
endif

$(eval $(rootfs))
