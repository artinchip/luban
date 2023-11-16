# Copyright (C) 2022 ArtInChip Technology Co., Ltd

# Delete default rules. We don't use them. This saves a bit of time.
.SUFFIXES:

# we want bash as shell
SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	 else if [ -x /bin/bash ]; then echo /bin/bash; \
	 else echo sh; fi; fi)

# Set O variable if not already done on the command line;
# or avoid confusing packages that can use the O=<dir> syntax for out-of-tree
# build by preventing it from being forwarded to sub-make calls.
ifneq ("$(origin O)", "command line")
# If user set O= in command line, ok, just do as user's expectation, otherwise:
#	1. If user already make xxx_defconfig, output/.current shoulde be exist,
#	   get LUBAN_CURRENT_OUT from it.
#	2. If user make xxx_defconfig now, generate LUBAN_CURRENT_OUT=xxx from xxx_defconfig

GOAL_DEFCONFIG=$(firstword $(filter %_defconfig,$(MAKECMDGOALS)))
ifneq ($(wildcard $(CURDIR)/target/configs/$(GOAL_DEFCONFIG)),)
LUBAN_CURRENT_OUT=$(subst _defconfig,,$(GOAL_DEFCONFIG))
endif
ifeq ($(LUBAN_CURRENT_OUT),)
-include $(CURDIR)/output/.current
endif
export LUBAN_CURRENT_OUT
O := $(CURDIR)/output/$(LUBAN_CURRENT_OUT)
endif

# Check if the current Buildroot execution meets all the pre-requisites.
# If they are not met, Buildroot will actually do its job in a sub-make meeting
# its pre-requisites, which are:
#  1- Permissive enough umask:
#       Wrong or too restrictive umask will prevent Buildroot and packages from
#       creating files and directories.
#  2- Absolute canonical CWD (i.e. $(CURDIR)):
#       Otherwise, some packages will use CWD as-is, others will compute its
#       absolute canonical path. This makes harder tracking and fixing host
#       machine path leaks.
#  3- Absolute canonical output location (i.e. $(O)):
#       For the same reason as the one for CWD.

# Remove the trailing '/.' from $(O) as it can be added by the makefile wrapper
# installed in the $(O) directory.
# Also remove the trailing '/' the user can set when on the command line.
override O := $(patsubst %/,%,$(patsubst %.,%,$(O)))
# Make sure $(O) actually exists before calling realpath on it; this is to
# avoid empty CANONICAL_O in case on non-existing entry.
CANONICAL_O := $(shell mkdir -p $(O) >/dev/null 2>&1)$(realpath $(O))

# gcc fails to build when the srcdir contains a '@'
ifneq ($(findstring @,$(CANONICAL_O)),)
$(error The build directory can not contain a '@')
endif

CANONICAL_CURDIR = $(realpath $(CURDIR))

REQ_UMASK = 0022

# Make sure O= is passed (with its absolute canonical path) everywhere the
# toplevel makefile is called back.
EXTRAMAKEARGS := O=$(CANONICAL_O)

# Check Buildroot execution pre-requisites here.
ifneq ($(shell umask):$(CURDIR):$(O),$(REQ_UMASK):$(CANONICAL_CURDIR):$(CANONICAL_O))
.PHONY: _all $(MAKECMDGOALS)

$(MAKECMDGOALS): _all
	@:

_all:
	@umask $(REQ_UMASK) && \
		$(MAKE) -C $(CANONICAL_CURDIR) --no-print-directory \
			$(MAKECMDGOALS) $(EXTRAMAKEARGS)

else # umask / $(CURDIR) / $(O)

# This is our default rule, so must come first
all:
.PHONY: all

# Check for minimal make version (note: this check will break at make 10.x)
MIN_MAKE_VERSION = 3.81
ifneq ($(firstword $(sort $(RUNNING_MAKE_VERSION) $(MIN_MAKE_VERSION))),$(MIN_MAKE_VERSION))
$(error You have make '$(RUNNING_MAKE_VERSION)' installed. GNU make >= $(MIN_MAKE_VERSION) is required)
endif

include package/Makefile.sdk

.PHONY: help
help:
	@$(call STANDOUT_MESSAGE,>>>>>>>>>>>>>>>>>>>>>>>> Most frequently used commands <<<<<<<<<<<<<<<<<<<<<<<<<)
	@$(call STANDOUT_MESSAGE,Board defconfig:)
	@echo '   make list               - list defconfig for boards'
	@echo '   make <board>_defconfig  - apply one board defconfig to project'
	@echo
	@$(call STANDOUT_MESSAGE,Configuration:)
	@echo '   make menuconfig         - interactive curses-based configurator'
	@echo
	@$(call STANDOUT_MESSAGE,Build:)
	@echo '   make all                - build all package and generate image'
	@echo '   make info               - show basic information about current project'
	@echo
	@$(call STANDOUT_MESSAGE,Cleaning:)
	@echo '   make clean              - delete all files created by build'
	@echo '   make distclean          - delete all non-source files (including .config)'
	@echo
	@$(call STANDOUT_MESSAGE,Package-specific:)
	@echo '   make <pkg>              - Build and install <pkg> and all its dependencies'
	@echo '   make <pkg>-clean        - Remove <pkg> build directory'
	@echo '   make <pkg>-distclean    - Remove <pkg> build directory and extracted source'
	@echo '   make <pkg>-reconfigure  - Restart the build from the configure step'
	@echo '   make <pkg>-rebuild      - Restart the build from the build step'
	@echo
	@$(call STANDOUT_MESSAGE,Miscellaneous:)
	@echo '   make V=0|1              - 0 => quiet build (default), 1 => verbose build'
	@echo '   make O=dir              - Locate all output files in "dir", including .config'
	@echo
	@$(call STANDOUT_MESSAGE,How to build:)
	@echo '1. make list               - to list all board defconfigs'
	@echo '2. make <board>_defconfig  - to apply one defconfig'
	@echo '3. make all                - to build project'

# Dependency targets locate at Makefile.sdk
list: list-defconfigs
l: list-defconfigs

menuconfig: do_menuconfig savedefconfig
m: do_menuconfig savedefconfig

.PHONY: clean
clean: $(HIDDEN_PREFIX)-clean

.PHONY: distclean
distclean: $(HIDDEN_PREFIX)-distclean

# Show basic information about current project
.PHONY: info
i: info
info:
ifeq ($(BR2_HAVE_DOT_CONFIG),y)
	@echo "      Target arch: $(ARCH)"
	@echo "      Target chip: target/$(LUBAN_CHIP_NAME)"
	@echo "     Target board: target/$(LUBAN_CHIP_NAME)/$(LUBAN_BOARD_NAME)"
	@echo "   Root directory: $(TOPDIR)"
	@echo "    Out directory: $(subst $(TOPDIR)/,,$(O)/)"
	@echo "    SDK defconfig: $(subst $(TOPDIR)/,,$(shell cat $(O)/.defconfig_file))"
	@echo "  Linux defconfig: $(subst $(TOPDIR)/,,$(LINUX_SRCDIR))/arch/$(KERNEL_ARCH)/configs/$(BR2_LINUX_KERNEL_DEFCONFIG)_defconfig"
	@echo " U-Boot defconfig: $(subst $(TOPDIR)/,,$(UBOOT_SRCDIR))/configs/$(BR2_TARGET_UBOOT_BOARD_DEFCONFIG)_defconfig"
	@echo "Busybox defconfig: $(BR2_PACKAGE_BUSYBOX_CONFIG)"
	@echo "      RootFS Type: $(sort $(TARGETS_ROOTFS))"
	@echo "Toolchain tarball: $(BR2_TOOLCHAIN_EXTERNAL_URL)"
	@echo ""
else
	@$(call STANDOUT_MESSAGE,No project information)
	@echo "You can start one project by following commands"
	@echo ""
	@echo "    make list"
	@echo "    make <board>_defconfig"
	@echo "    make all"
	@echo ""
endif

.PHONY: add_board
add_board: $(BUILD_DIR)/luban-config/conf
	@$(TOPDIR)/tools/scripts/add_board.py -c $^ -t $(TOPDIR) -o $(BASE_DIR)

# Some quick commands
k:  linux-rebuild
km: linux-menuconfig
kc: linux-clean
u:  uboot-rebuild
b:  uboot-rebuild
uc: uboot-clean
bc: uboot-clean
bm: uboot-menuconfig
um: uboot-menuconfig
s:  opensbi-rebuild
sc: opensbi-clean

kernel: linux
kernel-patch: linux-patch
kernel-menuconfig: linux-menuconfig
kernel-savedefconfig: linux-savedefconfig
kernel-clean: linux-clean
kernel-distclean: linux-distclean
kernel-reconfigure: linux-reconfigure
kernel-rebuild: linux-rebuild
kernel-reinstall: linux-reinstall

# Generate rootfs and firmware image
f: world
r: world
h: help

endif #umask / $(CURDIR) / $(O)
