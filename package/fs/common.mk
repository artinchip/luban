#
# Macro that builds the needed Makefile target to create a root
# filesystem image.
#
# The following variable must be defined before calling this macro
#
#  ROOTFS_$(FSTYPE)_CMD, the command that generates the root
#  filesystem image. A single command is allowed. The filename of the
#  filesystem image that it must generate is $$@.
#
# The following variables can optionaly be defined
#
#  ROOTFS_$(FSTYPE)_DEPENDENCIES, the list of dependencies needed to
#  build the root filesystem (usually host tools)
#
#  ROOTFS_$(FSTYPE)_PRE_GEN_HOOKS, a list of hooks to call before
#  generating the filesystem image
#
#  ROOTFS_$(FSTYPE)_POST_GEN_HOOKS, a list of hooks to call after
#  generating the filesystem image
#
# In terms of configuration option, this macro assumes that the
# BR2_TARGET_ROOTFS_$(FSTYPE) config option allows to enable/disable
# the generation of a filesystem image of a particular type. If
# the configuration options BR2_TARGET_ROOTFS_$(FSTYPE)_GZIP,
# BR2_TARGET_ROOTFS_$(FSTYPE)_BZIP2 or
# BR2_TARGET_ROOTFS_$(FSTYPE)_LZMA exist and are enabled, then the
# macro will automatically generate a compressed filesystem image.

FS_DIR = $(BUILD_DIR)/luban-fs
ROOTFS_DEVICE_TABLES = $(call qstrip,$(BR2_ROOTFS_DEVICE_TABLE) \
	$(BR2_ROOTFS_STATIC_DEVICE_TABLE))

ROOTFS_USERS_TABLES = $(call qstrip,$(BR2_ROOTFS_USERS_TABLES))

ROOTFS_FULL_DEVICES_TABLE = $(FS_DIR)/full_devices_table.txt
ROOTFS_FULL_USERS_TABLE = $(FS_DIR)/full_users_table.txt

ROOTFS_COMMON_NAME = rootfs-common
ROOTFS_COMMON_TYPE = rootfs
ROOTFS_COMMON_DEPENDENCIES = \
	host-fakeroot host-makedevs host-uboot-tools \
	$(BR2_TAR_HOST_DEPENDENCY) \
	$(if $(PACKAGES_USERS)$(ROOTFS_USERS_TABLES),host-mkpasswd)

ifeq ($(BR2_REPRODUCIBLE),y)
define ROOTFS_REPRODUCIBLE
	find $(TARGET_DIR) -print0 | xargs -0 -r touch -hd @$(SOURCE_DATE_EPOCH)
endef
endif

ifeq ($(BR2_PACKAGE_REFPOLICY),y)
define ROOTFS_SELINUX
	$(HOST_DIR)/sbin/setfiles -m -r $(TARGET_DIR) \
		-c $(TARGET_DIR)/etc/selinux/targeted/policy/policy.$(BR2_PACKAGE_LIBSEPOL_POLICY_VERSION) \
		$(TARGET_DIR)/etc/selinux/targeted/contexts/files/file_contexts \
		$(TARGET_DIR)
endef
ROOTFS_COMMON_DEPENDENCIES += host-policycoreutils
endif

PACKAGES_BUILD += $(ROOTFS_COMMON_DEPENDENCIES)

ROOTFS_COMMON_FINAL_RECURSIVE_DEPENDENCIES = $(sort \
	$(if $(filter undefined,$(origin ROOTFS_COMMON_FINAL_RECURSIVE_DEPENDENCIES__X)), \
		$(eval ROOTFS_COMMON_FINAL_RECURSIVE_DEPENDENCIES__X := \
			$(foreach p, \
				$(ROOTFS_COMMON_DEPENDENCIES), \
				$(p) \
				$($(call UPPERCASE,$(p))_FINAL_RECURSIVE_DEPENDENCIES) \
			) \
		) \
	) \
	$(ROOTFS_COMMON_FINAL_RECURSIVE_DEPENDENCIES__X))

.PHONY: rootfs-common
rootfs-common: $(ROOTFS_COMMON_DEPENDENCIES) target-finalize
	@$(call MESSAGE,"Generating root filesystems common tables")
	$(Q)rm -rf $(FS_DIR)
	$(Q)mkdir -p $(FS_DIR)

	$(Q)$(call PRINTF,$(PACKAGES_USERS)) >> $(ROOTFS_FULL_USERS_TABLE)
ifneq ($(ROOTFS_USERS_TABLES),)
	cat $(ROOTFS_USERS_TABLES) >> $(ROOTFS_FULL_USERS_TABLE)
endif

	$(Q)$(call PRINTF,$(PACKAGES_PERMISSIONS_TABLE)) > $(ROOTFS_FULL_DEVICES_TABLE)
ifneq ($(ROOTFS_DEVICE_TABLES),)
	cat $(ROOTFS_DEVICE_TABLES) >> $(ROOTFS_FULL_DEVICES_TABLE)
endif
ifeq ($(BR2_ROOTFS_DEVICE_CREATION_STATIC),y)
	$(Q)$(call PRINTF,$(PACKAGES_DEVICES_TABLE)) >> $(ROOTFS_FULL_DEVICES_TABLE)
endif

rootfs-common-show-depends:
	@echo $(ROOTFS_COMMON_DEPENDENCIES)

.PHONY: rootfs-common-show-info
rootfs-common-show-info:
	@:
	$(info $(call clean-json,{ $(call json-info,ROOTFS_COMMON) }))

# Since this function will be called from within an $(eval ...)
# all variable references except the arguments must be $$-quoted.
define inner-rootfs

ROOTFS_$(2)_NAME = rootfs-$(1)
ROOTFS_$(2)_TYPE = rootfs
ROOTFS_$(2)_IMAGE_NAME ?= rootfs.$(1)
ROOTFS_$(2)_FINAL_IMAGE_NAME = $$(strip $$(ROOTFS_$(2)_IMAGE_NAME))
ROOTFS_$(2)_DIR = $$(FS_DIR)/$(1)
ROOTFS_$(2)_TARGET_DIR = $$(ROOTFS_$(2)_DIR)/target

ROOTFS_$(2)_DEPENDENCIES += rootfs-common

ROOTFS_$(2)_FINAL_RECURSIVE_DEPENDENCIES = $$(sort \
	$$(if $$(filter undefined,$$(origin ROOTFS_$(2)_FINAL_RECURSIVE_DEPENDENCIES__X)), \
		$$(eval ROOTFS_$(2)_FINAL_RECURSIVE_DEPENDENCIES__X := \
			$$(foreach p, \
				$$(ROOTFS_$(2)_DEPENDENCIES), \
				$$(p) \
				$$($$(call UPPERCASE,$$(p))_FINAL_RECURSIVE_DEPENDENCIES) \
			) \
		) \
	) \
	$$(ROOTFS_$(2)_FINAL_RECURSIVE_DEPENDENCIES__X))

ifeq ($$(BR2_TARGET_ROOTFS_$(2)_GZIP),y)
ROOTFS_$(2)_COMPRESS_EXT = .gz
ROOTFS_$(2)_COMPRESS_CMD = gzip -9 -c -n
endif
ifeq ($$(BR2_TARGET_ROOTFS_$(2)_BZIP2),y)
ROOTFS_$(2)_COMPRESS_EXT = .bz2
ROOTFS_$(2)_COMPRESS_CMD = bzip2 -9 -c
endif
ifeq ($$(BR2_TARGET_ROOTFS_$(2)_LZMA),y)
ROOTFS_$(2)_DEPENDENCIES += host-lzma
ROOTFS_$(2)_COMPRESS_EXT = .lzma
ROOTFS_$(2)_COMPRESS_CMD = $$(LZMA) -9 -c
endif
ifeq ($$(BR2_TARGET_ROOTFS_$(2)_LZ4),y)
ROOTFS_$(2)_DEPENDENCIES += host-lz4
ROOTFS_$(2)_COMPRESS_EXT = .lz4
ROOTFS_$(2)_COMPRESS_CMD = lz4 -l -9 -c
endif
ifeq ($$(BR2_TARGET_ROOTFS_$(2)_LZO),y)
ROOTFS_$(2)_DEPENDENCIES += host-lzop
ROOTFS_$(2)_COMPRESS_EXT = .lzo
ROOTFS_$(2)_COMPRESS_CMD = $$(LZOP) -9 -c
endif
ifeq ($$(BR2_TARGET_ROOTFS_$(2)_XZ),y)
ROOTFS_$(2)_DEPENDENCIES += host-xz
ROOTFS_$(2)_COMPRESS_EXT = .xz
ROOTFS_$(2)_COMPRESS_CMD = xz -9 -C crc32 -c
ifeq ($(BR2_REPRODUCIBLE),)
ROOTFS_$(2)_COMPRESS_CMD += -T $(PARALLEL_JOBS)
endif
endif
ifeq ($(BR2_TARGET_ROOTFS_$(2)_ZSTD),y)
ROOTFS_$(2)_DEPENDENCIES += host-zstd
ROOTFS_$(2)_COMPRESS_EXT = .zst
ROOTFS_$(2)_COMPRESS_CMD = zstd -19 -z -f -T$(PARALLEL_JOBS)
endif

ifeq ($(BR2_GENERATE_IMAGE_AUTO_CALCULATE_SIZE),y)

IMAGE_CFG_ADDR := $(TARGET_BOARD_DIR)/image_cfg.json

ifeq ($(BR2_TARGET_ROOTFS_EXT2),y)
TARGET_EXTFS_MAX_SIZE := "BR2_TARGET_ROOTFS_EXT2_SIZE"
BR2_TARGET_ROOTFS_EXT2_SIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_EXTFS_MAX_SIZE)))

ifneq ($(BR2_TARGET_ROOTFS_EXT2_SIZE_X),)
BR2_TARGET_ROOTFS_EXT2_SIZE := $(BR2_TARGET_ROOTFS_EXT2_SIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS1),y)
TARGET_EXTFS_MAX_SIZE1 := "BR2_TARGET_USERFS1_EXT4_SIZE"
BR2_TARGET_USERFS1_EXT4_SIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_EXTFS_MAX_SIZE1)))

ifneq ($(BR2_TARGET_USERFS1_EXT4_SIZE_X),)
BR2_TARGET_USERFS1_EXT4_SIZE := $(BR2_TARGET_USERFS1_EXT4_SIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS2),y)
TARGET_EXTFS_MAX_SIZE2 := "BR2_TARGET_USERFS2_EXT4_SIZE"
BR2_TARGET_USERFS2_EXT4_SIZE := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_EXTFS_MAX_SIZE2)))

ifneq ($(BR2_TARGET_USERFS2_EXT4_SIZE_X),)
BR2_TARGET_USERFS2_EXT4_SIZE := $(BR2_TARGET_USERFS2_EXT4_SIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS3),y)
TARGET_EXTFS_MAX_SIZE3 := "BR2_TARGET_USERFS3_EXT4_SIZE"
BR2_TARGET_USERFS3_EXT4_SIZE := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_EXTFS_MAX_SIZE3)))

ifneq ($(BR2_TARGET_USERFS3_EXT4_SIZE_X),)
BR2_TARGET_USERFS3_EXT4_SIZE := $(BR2_TARGET_USERFS3_EXT4_SIZE_X)
endif
endif


ifeq ($(BR2_TARGET_ROOTFS_UBI),y)
TARGET_UBIFS_MAX_SIZE := "BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE"
BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_UBIFS_MAX_SIZE)))

ifneq ($(BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE_X),)
BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE := $(BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS1),y)
TARGET_UBIFS_MAX_SIZE1 := "BR2_TARGET_USERFS1_UBIFS_MAX_SIZE"
BR2_TARGET_USERFS1_UBIFS_MAX_SIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_UBIFS_MAX_SIZE1)))

ifneq ($(BR2_TARGET_USERFS1_UBIFS_MAX_SIZE_X),)
BR2_TARGET_USERFS1_UBIFS_MAX_SIZE := $(BR2_TARGET_USERFS1_UBIFS_MAX_SIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS2),y)
TARGET_UBIFS_MAX_SIZE2 := "BR2_TARGET_USERFS2_UBIFS_MAX_SIZE"
BR2_TARGET_USERFS2_UBIFS_MAX_SIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_UBIFS_MAX_SIZE2)))

ifneq ($(BR2_TARGET_USERFS2_UBIFS_MAX_SIZE_X),)
BR2_TARGET_USERFS2_UBIFS_MAX_SIZE := $(BR2_TARGET_USERFS2_UBIFS_MAX_SIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS3),y)
TARGET_UBIFS_MAX_SIZE3 := "BR2_TARGET_USERFS3_UBIFS_MAX_SIZE"
BR2_TARGET_USERFS3_UBIFS_MAX_SIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_UBIFS_MAX_SIZE3)))

ifneq ($(BR2_TARGET_USERFS3_UBIFS_MAX_SIZE_X),)
BR2_TARGET_USERFS3_UBIFS_MAX_SIZE := $(BR2_TARGET_USERFS3_UBIFS_MAX_SIZE_X)
endif
endif


ifeq ($(BR2_TARGET_ROOTFS_JFFS2),y)
TARGET_JFFS2_PADSIZE := "BR2_TARGET_ROOTFS_JFFS2_PADSIZE"
BR2_TARGET_ROOTFS_JFFS2_PADSIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_JFFS2_PADSIZE)))

ifneq ($(BR2_TARGET_ROOTFS_JFFS2_PADSIZE_X),)
BR2_TARGET_ROOTFS_JFFS2_PADSIZE := $(BR2_TARGET_ROOTFS_JFFS2_PADSIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS1),y)
TARGET_JFFS2_PADSIZE1 := "BR2_TARGET_USERFS1_JFFS2_PADSIZE"
BR2_TARGET_USERFS1_JFFS2_PADSIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_JFFS2_PADSIZE1)))

ifneq ($(BR2_TARGET_USERFS1_JFFS2_PADSIZE_X),)
BR2_TARGET_USERFS1_JFFS2_PADSIZE := $(BR2_TARGET_USERFS1_JFFS2_PADSIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS2),y)
TARGET_JFFS2_PADSIZE2 := "BR2_TARGET_USERFS2_JFFS2_PADSIZE"
BR2_TARGET_USERFS2_JFFS2_PADSIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_JFFS2_PADSIZE2)))

ifneq ($(BR2_TARGET_USERFS2_JFFS2_PADSIZE_X),)
BR2_TARGET_USERFS2_JFFS2_PADSIZE := $(BR2_TARGET_USERFS2_JFFS2_PADSIZE_X)
endif
endif

ifeq ($(BR2_TARGET_USERFS3),y)
TARGET_JFFS2_PADSIZE3 := "BR2_TARGET_USERFS3_JFFS2_PADSIZE"
BR2_TARGET_USERFS3_JFFS2_PADSIZE_X := $(if $(IMAGE_CFG_ADDR),$(shell tools/scripts/get_fs_max_size.sh $(IMAGE_CFG_ADDR) $(TARGET_JFFS2_PADSIZE3)))

ifneq ($(BR2_TARGET_USERFS3_JFFS2_PADSIZE_X),)
BR2_TARGET_USERFS3_JFFS2_PADSIZE := $(BR2_TARGET_USERFS3_JFFS2_PADSIZE_X)
endif
endif


endif

$$(BINARIES_DIR)/$$(ROOTFS_$(2)_FINAL_IMAGE_NAME): ROOTFS=$(2)
$$(BINARIES_DIR)/$$(ROOTFS_$(2)_FINAL_IMAGE_NAME): FAKEROOT_SCRIPT=$$(ROOTFS_$(2)_DIR)/fakeroot
$$(BINARIES_DIR)/$$(ROOTFS_$(2)_FINAL_IMAGE_NAME): $$(ROOTFS_$(2)_DEPENDENCIES)
	@$$(call MESSAGE,"Generating filesystem image $$(ROOTFS_$(2)_FINAL_IMAGE_NAME)")
ifeq ($(BR2_GENERATE_IMAGE_AUTO_CALCULATE_SIZE),y)
ifeq ($(BR2_TARGET_ROOTFS_EXT2),y)
	@$$(call MESSAGE,"BR2_TARGET_ROOTFS_EXT2_SIZE $$(BR2_TARGET_ROOTFS_EXT2_SIZE)")
endif
ifeq ($(BR2_TARGET_USERFS1),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS1_EXT4_SIZE $$(BR2_TARGET_USERFS1_EXT4_SIZE)")
endif
ifeq ($(BR2_TARGET_USERFS2),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS2_EXT4_SIZE $$(BR2_TARGET_USERFS2_EXT4_SIZE)")
endif
ifeq ($(BR2_TARGET_USERFS3),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS3_EXT4_SIZE $$(BR2_TARGET_USERFS3_EXT4_SIZE)")
endif

ifeq ($(BR2_TARGET_ROOTFS_UBI),y)
	@$$(call MESSAGE,"BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE $$(BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE)")
endif
ifeq ($(BR2_TARGET_USERFS1),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS1_UBIFS_MAX_SIZE $$(BR2_TARGET_USERFS1_UBIFS_MAX_SIZE)")
endif
ifeq ($(BR2_TARGET_USERFS2),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS2_UBIFS_MAX_SIZE $$(BR2_TARGET_USERFS2_UBIFS_MAX_SIZE)")
endif
ifeq ($(BR2_TARGET_USERFS3),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS3_UBIFS_MAX_SIZE $$(BR2_TARGET_USERFS3_UBIFS_MAX_SIZE)")
endif

ifeq ($(BR2_TARGET_ROOTFS_JFFS2),y)
	@$$(call MESSAGE,"BR2_TARGET_ROOTFS_JFFS2_PADSIZE $$(BR2_TARGET_ROOTFS_JFFS2_PADSIZE)")
endif
ifeq ($(BR2_TARGET_USERFS1),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS1_JFFS2_PADSIZE $$(BR2_TARGET_USERFS1_JFFS2_PADSIZE)")
endif
ifeq ($(BR2_TARGET_USERFS2),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS2_JFFS2_PADSIZE $$(BR2_TARGET_USERFS2_JFFS2_PADSIZE)")
endif
ifeq ($(BR2_TARGET_USERFS3),y)
	@$$(call MESSAGE,"BR2_TARGET_USERFS3_JFFS2_PADSIZE $$(BR2_TARGET_USERFS3_JFFS2_PADSIZE)")
endif

endif
	$(Q)mkdir -p $$(@D)
	$(Q)rm -rf $$(ROOTFS_$(2)_DIR)
	$(Q)mkdir -p $$(ROOTFS_$(2)_DIR)
	$(Q)rsync -auH \
		--exclude=/$$(notdir $$(TARGET_DIR_WARNING_FILE)) \
		$$(BASE_TARGET_DIR)/ \
		$$(TARGET_DIR)

	$(Q)echo '#!/bin/sh' > $$(FAKEROOT_SCRIPT)
	$(Q)echo "set -e" >> $$(FAKEROOT_SCRIPT)

	$(Q)echo "chown -h -R 0:0 $$(TARGET_DIR)" >> $$(FAKEROOT_SCRIPT)
	$(Q)PATH=$$(BR_PATH) $$(TOPDIR)/tools/support/scripts/mkusers $$(ROOTFS_FULL_USERS_TABLE) $$(TARGET_DIR) >> $$(FAKEROOT_SCRIPT)
	$(Q)echo "$$(HOST_DIR)/bin/makedevs -d $$(ROOTFS_FULL_DEVICES_TABLE) $$(TARGET_DIR)" >> $$(FAKEROOT_SCRIPT)
	$(Q)$$(foreach hook,$$(ROOTFS_PRE_CMD_HOOKS),\
		$$(call PRINTF,$$($$(hook))) >> $$(FAKEROOT_SCRIPT)$$(sep))
	$(Q)$$(foreach s,$$(call qstrip,$$(BR2_ROOTFS_POST_FAKEROOT_SCRIPT)),\
		echo "echo '$$(TERM_BOLD)>>>   Executing fakeroot script $$(s)$$(TERM_RESET)'" >> $$(FAKEROOT_SCRIPT); \
		echo $$(EXTRA_ENV) $$(s) $$(TARGET_DIR) $$(BR2_ROOTFS_POST_SCRIPT_ARGS) >> $$(FAKEROOT_SCRIPT)$$(sep))

	$(Q)$$(foreach hook,$$(ROOTFS_$(2)_PRE_GEN_HOOKS),\
		$$(call PRINTF,$$($$(hook))) >> $$(FAKEROOT_SCRIPT)$$(sep))
	$(Q)$$(call PRINTF,$$(ROOTFS_REPRODUCIBLE)) >> $$(FAKEROOT_SCRIPT)
	$(Q)$$(call PRINTF,$$(ROOTFS_SELINUX)) >> $$(FAKEROOT_SCRIPT)
	$(Q)$$(call PRINTF,$$(ROOTFS_$(2)_CMD)) >> $$(FAKEROOT_SCRIPT)
ifeq ($(BR2_TARGET_ROOTFS_EXT2_TO_SPARSE),y)
	$(Q)$$(call PRINTF,$$(ROOTFS_SPARSE_CMD)) >> $$(FAKEROOT_SCRIPT)
endif
	$(Q)chmod a+x $$(FAKEROOT_SCRIPT)
	$(Q)PATH=$$(BR_PATH) FAKEROOTDONTTRYCHOWN=1 $$(HOST_DIR)/bin/fakeroot -- $$(FAKEROOT_SCRIPT)
ifneq ($$(ROOTFS_$(2)_COMPRESS_CMD),)
	$(Q)if [ -f $$@ ]; then \
		PATH=$$(BR_PATH) $$(ROOTFS_$(2)_COMPRESS_CMD) $$@ > $$@$$(ROOTFS_$(2)_COMPRESS_EXT); \
	fi
	$$(foreach hook,$$(ROOTFS_$(2)_COMPRESS_HOOKS),$$(call $$(hook))$$(sep))
endif
	$$(foreach hook,$$(ROOTFS_$(2)_POST_GEN_HOOKS),$$(call $$(hook))$$(sep))
	$(Q)cd $$(BINARIES_DIR) && echo In $$(BINARIES_DIR) && ls -og --time-style=iso rootfs*

rootfs-$(1)-show-depends:
	@echo $$(ROOTFS_$(2)_DEPENDENCIES)

rootfs-$(1)-show-info:
	@:
	$$(info $$(call clean-json,{ $$(call json-info,ROOTFS_$(2)) }))

rootfs-$(1): $$(BINARIES_DIR)/$$(ROOTFS_$(2)_FINAL_IMAGE_NAME)

.PHONY: rootfs-$(1) rootfs-$(1)-show-depends rootfs-$(1)-show-info

ifeq ($$(BR2_TARGET_ROOTFS_$(2)),y)
TARGETS_ROOTFS += rootfs-$(1)
PACKAGES += $$(filter-out rootfs-%,$$(ROOTFS_$(2)_FINAL_RECURSIVE_DEPENDENCIES))
PACKAGES_BUILD += $$(filter-out rootfs-%,$$(ROOTFS_$(2)_FINAL_RECURSIVE_DEPENDENCIES))
endif

# Check for legacy POST_TARGETS rules
ifneq ($$(ROOTFS_$(2)_POST_TARGETS),)
$$(error Filesystem $(1) uses post-target rules, which are no longer supported.\
	Update $(1) to use post-gen hooks instead)
endif

endef

# $(pkgname) also works well to return the filesystem name
rootfs = $(call inner-rootfs,$(pkgname),$(call UPPERCASE,$(pkgname)))

include $(sort $(wildcard package/fs/*/*.mk))
