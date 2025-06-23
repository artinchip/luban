
USERFS_DIR = $(BUILD_DIR)/luban-userfs

# Since this function will be called from within an $(eval ...)
# all variable references except the arguments must be $$-quoted.
define inner-userfs

USERFS_$(1)_NAME = userfs$(1)
USERFS_$(1)_TYPE = rootfs

ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_EXT4),y)
USERFS_$(1)_IMAGE_NAME ?= $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME)).ext4
USERFS_$(1)_DEPENDENCIES += host-e2fsprogs
else ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_UBIFS),y)
USERFS_$(1)_IMAGE_NAME ?= $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME)).ubifs
USERFS_$(1)_DEPENDENCIES = host-mtd
else ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_JFFS2),y)
USERFS_$(1)_IMAGE_NAME ?= $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME)).jffs2
USERFS_$(1)_DEPENDENCIES = host-mtd
else ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_SQUASHFS),y)
USERFS_$(1)_IMAGE_NAME ?= $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME)).squashfs
USERFS_$(1)_DEPENDENCIES = host-squashfs
else
USERFS_$(1)_IMAGE_NAME ?= userfs$(1).dummyimage
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_EXT4_TO_SPARSE),y)
USERFS_$(1)_SPARSE_IMAGE_NAME ?= $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME)).sparse
USERFS_$(1)_DEPENDENCIES += host-android-tools
endif

USERFS_$(1)_FINAL_IMAGE_NAME = $$(strip $$(USERFS_$(1)_IMAGE_NAME))
ifeq ($$(BR2_TARGET_USERFS$(1)_EXT4_TO_SPARSE),y)
USERFS_$(1)_FINAL_SPARSE_IMAGE_NAME = $$(strip $$(USERFS_$(1)_SPARSE_IMAGE_NAME))
endif

USERFS_$(1)_DIR = $$(USERFS_DIR)/fs$(1).$$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME))
USERFS_$(1)_TARGET_DIR = $$(USERFS_$(1)_DIR)/target

USERFS_$(1)_FINAL_RECURSIVE_DEPENDENCIES = $$(sort \
	$$(if $$(filter undefined,$$(origin USERFS_$(1)_FINAL_RECURSIVE_DEPENDENCIES__X)), \
		$$(eval USERFS_$(1)_FINAL_RECURSIVE_DEPENDENCIES__X := \
			$$(foreach p, \
				$$(USERFS_$(1)_DEPENDENCIES), \
				$$(p) \
				$$($$(call UPPERCASE,$$(p))_FINAL_RECURSIVE_DEPENDENCIES) \
			) \
		) \
	) \
	$$(USERFS_$(1)_FINAL_RECURSIVE_DEPENDENCIES__X))

ifeq ($$(BR2_TARGET_USERFS$(1)_GZIP),y)
USERFS_$(1)_COMPRESS_EXT = .gz
USERFS_$(1)_COMPRESS_CMD = gzip -9 -c -n
endif
ifeq ($$(BR2_TARGET_USERFS$(1)_BZIP2),y)
USERFS_$(1)_COMPRESS_EXT = .bz2
USERFS_$(1)_COMPRESS_CMD = bzip2 -9 -c
endif
ifeq ($$(BR2_TARGET_USERFS$(1)_LZMA),y)
USERFS_$(1)_DEPENDENCIES += host-lzma
USERFS_$(1)_COMPRESS_EXT = .lzma
USERFS_$(1)_COMPRESS_CMD = $$(LZMA) -9 -c
endif
ifeq ($$(BR2_TARGET_USERFS$(1)_LZ4),y)
USERFS_$(1)_DEPENDENCIES += host-lz4
USERFS_$(1)_COMPRESS_EXT = .lz4
USERFS_$(1)_COMPRESS_CMD = lz4 -l -9 -c
endif
ifeq ($$(BR2_TARGET_USERFS$(1)_LZO),y)
USERFS_$(1)_DEPENDENCIES += host-lzop
USERFS_$(1)_COMPRESS_EXT = .lzo
USERFS_$(1)_COMPRESS_CMD = $$(LZOP) -9 -c
endif
ifeq ($$(BR2_TARGET_USERFS$(1)_XZ),y)
USERFS_$(1)_DEPENDENCIES += host-xz
USERFS_$(1)_COMPRESS_EXT = .xz
USERFS_$(1)_COMPRESS_CMD = xz -9 -C crc32 -c
USERFS_$(1)_COMPRESS_CMD += -T $(PARALLEL_JOBS)
endif
ifeq ($(BR2_TARGET_USERFS$(1)_ZSTD),y)
USERFS_$(1)_DEPENDENCIES += host-zstd
USERFS_$(1)_COMPRESS_EXT = .zst
USERFS_$(1)_COMPRESS_CMD = zstd -19 -z -f -T$(PARALLEL_JOBS)
endif

######## EXT4 commands ########

USERFS_$(1)_EXT4_SIZE = $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_EXT4_SIZE))
USERFS_$(1)_EXT4_MKFS_OPTS = $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_EXT4_MKFS_OPTIONS))

# qstrip results in stripping consecutive spaces into a single one. So the
# variable is not qstrip-ed to preserve the integrity of the string value.
USERFS_$(1)_EXT4_LABEL = $(subst ",,$$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME)))
#" Syntax highlighting... :-/ )

USERFS_$(1)_EXT4_OPTS = \
	-d $$(USERFS_$(1)_TARGET_DIR) \
	-r 1 \
	-N $$(BR2_TARGET_USERFS$(1)_EXT4_INODES) \
	-m $$(BR2_TARGET_USERFS$(1)_EXT4_RESBLKS) \
	-L "$$(USERFS_$(1)_EXT4_LABEL)" \
	$$(USERFS_$(1)_EXT4_MKFS_OPTS)

USERFS_$(1)_EXT4_CMD = rm -rf $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME); \
	MKE2FS_CONFIG=$(HOST_DIR)/etc/mke2fs.conf $(HOST_DIR)/sbin/mkfs.ext4 $$(USERFS_$(1)_EXT4_OPTS) $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME) \
		"$$(USERFS_$(1)_EXT4_SIZE)" \
	|| { ret=$$?; \
	     echo "*** Maybe you need to increase the filesystem size (BR2_TARGET_USERFS$(1)_EXT4_SIZE)" 1>&2; \
	     exit $$ret; \
	}

ifeq ($$(BR2_TARGET_USERFS$(1)_EXT4_TO_SPARSE),y)
USERFS_$(1)_SPARSE_CMD = rm -rf $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_SPARSE_IMAGE_NAME); \
	$(HOST_DIR)/bin/img2simg $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME) $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_SPARSE_IMAGE_NAME) \
	|| { ret=$$?; \
	     echo "*** ext4 image format to sparse image format failed"; \
	     exit $$ret; \
	}
endif

######## UBIFS commands ########

ifeq ($$(BR2_TARGET_USERFS$(1)_UBIFS_RT_ZLIB),y)
USERFS_$(1)_UBIFS_OPTS += -x zlib
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_UBIFS_RT_LZO),y)
USERFS_$(1)_UBIFS_OPTS += -x lzo
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_UBIFS_RT_NONE),y)
USERFS_$(1)_UBIFS_OPTS += -x none
endif

USERFS_$(1)_UBIFS_OPTS += $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_UBIFS_OPTS))

ifeq ($(BR2_UBI_PARAM_USER_CUSTOM),y)
USERFS_$(1)_UBIFS_OPTS_CUSTOM = \
	-e $(BR2_TARGET_ROOTFS_UBIFS_LEBSIZE) \
	-c $$(BR2_TARGET_USERFS$(1)_UBIFS_MAXLEBCNT) \
	-m $(BR2_TARGET_ROOTFS_UBIFS_MINIOSIZE)

MK_UBIFS_$(1)_USER_CUSTOM = $(HOST_DIR)/sbin/mkfs.ubifs \
	-d $$(USERFS_$(1)_TARGET_DIR) $$(USERFS_$(1)_UBIFS_OPTS_CUSTOM) $$(USERFS_$(1)_UBIFS_OPTS) \
	-o $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME)
MK_UBIFS_$(1)_CMD += MK_UBIFS_$(1)_USER_CUSTOM
endif

ifeq ($(BR2_UBI_DEVICE_SPI_NAND_2K_128K),y)
UBIFS_LEBCNT_$(1)_2K_128K=$(shell echo "$(shell printf '%d' $(BR2_TARGET_USERFS$(1)_UBIFS_MAX_SIZE)) / 131072" | bc)
USERFS_$(1)_UBIFS_OPTS_2K_128K = \
	-e 0x1f000 \
	-m 0x800 \
	-c $$(UBIFS_LEBCNT_$(1)_2K_128K)
MK_UBIFS_$(1)_2K_128K = $(HOST_DIR)/sbin/mkfs.ubifs \
	-d $$(USERFS_$(1)_TARGET_DIR) $$(USERFS_$(1)_UBIFS_OPTS_2K_128K) $$(USERFS_$(1)_UBIFS_OPTS) \
	-o $$(BINARIES_DIR)/$$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME))_page_2k_block_128k.ubifs

MK_UBIFS_$(1)_CMD += MK_UBIFS_$(1)_2K_128K
endif

ifeq ($(BR2_UBI_DEVICE_SPI_NAND_4K_256K),y)
UBIFS_LEBCNT_$(1)_4K_256K=$(shell echo "$(shell printf '%d' $(BR2_TARGET_USERFS$(1)_UBIFS_MAX_SIZE)) / 262144" | bc)
USERFS_$(1)_UBIFS_OPTS_4K_256K = \
	-e 0x3e000 \
	-m 0x1000 \
	-c $$(UBIFS_LEBCNT_$(1)_4K_256K)
MK_UBIFS_$(1)_4K_256K = $(HOST_DIR)/sbin/mkfs.ubifs \
	-d $$(USERFS_$(1)_TARGET_DIR) $$(USERFS_$(1)_UBIFS_OPTS_4K_256K) $$(USERFS_$(1)_UBIFS_OPTS) \
	-o $$(BINARIES_DIR)/$$(call qstrip,$$(BR2_TARGET_USERFS$(1)_NAME))_page_4k_block_256k.ubifs

MK_UBIFS_$(1)_CMD += MK_UBIFS_$(1)_4K_256K
endif

USERFS_$(1)_UBIFS_CMD = $$(foreach MKCMD, $$(MK_UBIFS_$(1)_CMD), $$(call $$(MKCMD))$$(sep))

######## JFFS2 commands ########
USERFS_$(1)_JFFS2_OPTS = -e $$(BR2_TARGET_USERFS$(1)_JFFS2_EBSIZE) --with-xattr
USERFS_$(1)_JFFS2_SUMTOOL_OPTS = -e $$(BR2_TARGET_USERFS$(1)_JFFS2_EBSIZE)

ifeq ($(BR2_GENERATE_IMAGE_AUTO_CALCULATE_SIZE),y)
ifneq ($$(strip $$(BR2_TARGET_USERFS$(1)_JFFS2_PADSIZE)),0x0)
USERFS_$(1)_JFFS2_OPTS += --pad=$$(strip $$(BR2_TARGET_USERFS$(1)_JFFS2_PADSIZE))
else
USERFS_$(1)_JFFS2_OPTS += -p
endif
USERFS_$(1)_JFFS2_SUMTOOL_OPTS += -p
else ifeq ($$(BR2_TARGET_USERFS$(1)_JFFS2_PAD),y)
ifneq ($$(strip $$(BR2_TARGET_USERFS$(1)_JFFS2_PADSIZE)),0x0)
USERFS_$(1)_JFFS2_OPTS += --pad=$$(strip $$(BR2_TARGET_USERFS$(1)_JFFS2_PADSIZE))
else
USERFS_$(1)_JFFS2_OPTS += -p
endif
USERFS_$(1)_JFFS2_SUMTOOL_OPTS += -p
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_JFFS2_LE),y)
USERFS_$(1)_JFFS2_OPTS += -l
USERFS_$(1)_JFFS2_SUMTOOL_OPTS += -l
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_JFFS2_BE),y)
USERFS_$(1)_JFFS2_OPTS += -b
USERFS_$(1)_JFFS2_SUMTOOL_OPTS += -b
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_JFFS2_USE_CUSTOM_PAGESIZE),y)
USERFS_$(1)_JFFS2_OPTS += -s $$(BR2_TARGET_USERFS$(1)_JFFS2_CUSTOM_PAGESIZE)
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_JFFS2_NOCLEANMARKER),y)
USERFS_$(1)_JFFS2_OPTS += -n
USERFS_$(1)_JFFS2_SUMTOOL_OPTS += -n
endif

ifneq ($$(BR2_TARGET_USERFS$(1)_JFFS2_SUMMARY),)
USERFS_$(1)_JFFS2_CMD = \
	$(HOST_DIR)/sbin/mkfs.jffs2 $$(USERFS_$(1)_JFFS2_OPTS) \
	-d $$(USERFS_$(1)_TARGET_DIR) \
	-o $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME).nosummary \
	$$(USERFS_$(1)_JFFS2_SUMTOOL) $$(USERFS_$(1)_JFFS2_SUMTOOL_OPTS) \
	-i $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME).nosummary \
	-o $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME)
	rm $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME).nosummary
else
USERFS_$(1)_JFFS2_CMD = \
	$(HOST_DIR)/sbin/mkfs.jffs2 $$(USERFS_$(1)_JFFS2_OPTS) \
	-d $$(USERFS_$(1)_TARGET_DIR) \
	-o $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME)
endif

######## SQUASHFS commands ########

USERFS_$(1)_SQUASHFS_ARGS = -noappend -processors $$(PARALLEL_JOBS)

ifeq ($$(BR2_TARGET_USERFS$(1)_SQUASHFS_PAD),y)
USERFS_$(1)_SQUASHFS_ARGS += -nopad
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_SQUASHFS4_LZ4),y)
USERFS_$(1)_SQUASHFS_ARGS += -comp lz4 -Xhc
else ifeq ($$(BR2_TARGET_USERFS$(1)_SQUASHFS4_LZO),y)
USERFS_$(1)_SQUASHFS_ARGS += -comp lzo
else ifeq ($$(BR2_TARGET_USERFS$(1)_SQUASHFS4_LZMA),y)
USERFS_$(1)_SQUASHFS_ARGS += -comp lzma
else ifeq ($$(BR2_TARGET_USERFS$(1)_SQUASHFS4_XZ),y)
USERFS_$(1)_SQUASHFS_ARGS += -comp xz
else ifeq ($$(BR2_TARGET_USERFS$(1)_SQUASHFS4_ZSTD),y)
USERFS_$(1)_SQUASHFS_ARGS += -comp zstd
else
USERFS_$(1)_SQUASHFS_ARGS += -comp gzip
endif

USERFS_$(1)_SQUASHFS_CMD = rm -rf $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME); \
	$(HOST_DIR)/bin/mksquashfs $$(USERFS_$(1)_TARGET_DIR) \
	$$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME) \
	$$(USERFS_$(1)_SQUASHFS_ARGS);


$$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME): FAKEROOT_SCRIPT=$$(USERFS_$(1)_DIR)/fakeroot
$$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME): $$(USERFS_$(1)_DEPENDENCIES)
	@$$(call MESSAGE,"Generating filesystem image $$(USERFS_$(1)_FINAL_IMAGE_NAME)")
	mkdir -p $$(@D)
	mkdir -p $(USERFS_DIR)
	rm -rf $$(USERFS_$(1)_TARGET_DIR)
	mkdir -p $$(USERFS_$(1)_TARGET_DIR)
	rsync -auH \
		--exclude=/$$(notdir $$(TARGET_DIR_WARNING_FILE)) \
		$$(TARGET_USERFS$(1)_DIR)/ \
		$$(USERFS_$(1)_TARGET_DIR)
ifneq ($$(call qstrip,$$(BR2_TARGET_USERFS$(1)_OVERLAY)),)
	if test -d $$(call qstrip,$$(BR2_TARGET_USERFS$(1)_OVERLAY))/; then \
	rsync -auH --chmod=u=rwX,go=rX --exclude .empty --exclude '*~' \
		$$(call qstrip,$$(BR2_TARGET_USERFS$(1)_OVERLAY))/ \
		$$(USERFS_$(1)_TARGET_DIR); \
	fi
endif
	echo '#!/bin/sh' > $$(FAKEROOT_SCRIPT)
	echo "set -e" >> $$(FAKEROOT_SCRIPT)
	echo "chown -h -R 0:0 $$(USERFS_$(1)_TARGET_DIR)" >> $$(FAKEROOT_SCRIPT)
ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_EXT4),y)
	$$(call PRINTF,$$(USERFS_$(1)_EXT4_CMD)) >> $$(FAKEROOT_SCRIPT)
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_EXT4_TO_SPARSE),y)
	$$(call PRINTF,$$(USERFS_$(1)_SPARSE_CMD)) >> $$(FAKEROOT_SCRIPT)
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_UBIFS),y)
	$$(call PRINTF,$$(USERFS_$(1)_UBIFS_CMD)) >> $$(FAKEROOT_SCRIPT)
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_JFFS2),y)
	$$(call PRINTF,$$(USERFS_$(1)_JFFS2_CMD)) >> $$(FAKEROOT_SCRIPT)
endif

ifeq ($$(BR2_TARGET_USERFS$(1)_TYPE_SQUASHFS),y)
	$$(call PRINTF,$$(USERFS_$(1)_SQUASHFS_CMD)) >> $$(FAKEROOT_SCRIPT)
endif
	chmod a+x $$(FAKEROOT_SCRIPT)
	PATH=$$(BR_PATH) FAKEROOTDONTTRYCHOWN=1 $$(HOST_DIR)/bin/fakeroot -- $$(FAKEROOT_SCRIPT)
ifneq ($$(USERFS_$(1)_COMPRESS_CMD),)
	PATH=$$(BR_PATH) $$(USERFS_$(1)_COMPRESS_CMD) $$@ > $$@$$(USERFS_$(1)_COMPRESS_EXT)
endif

userfs$(1)-show-depends:
	@echo $$(USERFS_$(1)_DEPENDENCIES)

userfs$(1)-show-info:
	@:
	$$(info $$(call clean-json,{ $$(call json-info,USERFS_$(1)) }))

userfs$(1): $$(BINARIES_DIR)/$$(USERFS_$(1)_FINAL_IMAGE_NAME)

.PHONY: userfs$(1) userfs$(1)-show-depends userfs$(1)-show-info

ifeq ($$(BR2_TARGET_USERFS$(1)),y)
TARGETS_USERFS += userfs$(1)
PACKAGES += $$(filter-out userfs%,$$(USERFS_$(1)_FINAL_RECURSIVE_DEPENDENCIES))
endif
endef

# userfs1
$(eval $(call inner-userfs,1))
# userfs2
$(eval $(call inner-userfs,2))
# userfs3
$(eval $(call inner-userfs,3))
