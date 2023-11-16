#!/bin/bash

MKIMAGEPY=tools/scripts/mk_image.py
MKRESPRIVPY=tools/scripts/mk_private_resource.py
MKENVIMAGE=${HOST_DIR}/bin/mkenvimage
PINMUXCHECKPY=tools/scripts/pinmux_check.py
DTBDIR=${BINARIES_DIR}/u-boot.dtb

COLOR_BEGIN="\033["
COLOR_RED="${COLOR_BEGIN}41;37m"
COLOR_YELLOW="${COLOR_BEGIN}43;30m"
COLOR_WHITE="${COLOR_BEGIN}47;30m"
COLOR_END="\033[0m"

function mk_error()
{
	echo -e "      ${COLOR_RED}$*${COLOR_END}"
}

function mk_warn()
{
	echo -e "      ${COLOR_YELLOW}$*${COLOR_END}"
}

function mk_info()
{
	echo -e "      ${COLOR_WHITE}$*${COLOR_END}"
}

function run_cmd()
{
	echo -e "      $*"
	eval $1
	if [ $? -ne 0 ] ; then
		mk_error "Run command failed: $*"
		exit 1
	fi
}

function mk_uboot_env()
{
	local ENVTXT1
	local ENVTXT2

	mk_info "Install uboot env source file ..."

	ENVTXT1="${TARGET_BOARD_DIR}/env.txt"
	ENVTXT2="${TARGET_CHIP_DIR}/common/env.txt"

	rm -rf ${BINARIES_DIR}/env.txt
	if [ -f ${ENVTXT1} ]; then
		run_cmd "ln -sf ${ENVTXT1} ${BINARIES_DIR}/"
	elif [ -f ${ENVTXT2} ]; then
		run_cmd "ln -sf ${ENVTXT2} ${BINARIES_DIR}/"
	fi

	return 0
}

function mk_boot_logo()
{
	local LOGO

	LOGO="${TARGET_BOARD_DIR}/boot_logo.*"

	rm -rf ${BINARIES_DIR}/boot_logo.*
	if [ -f ${LOGO} ]; then
		mk_info "Install boot logo file ..."
		run_cmd "ln -sf ${LOGO} ${BINARIES_DIR}"
	fi

	return 0
}

function mk_rsa_key()
{
	local KEYDIR="${TARGET_BOARD_DIR}/keys"

	rm -rf ${BINARIES_DIR}/keys
	if [ -d ${KEYDIR} ]; then
		mk_info "Install rsa key file ..."
		run_cmd "ln -sf ${KEYDIR} ${BINARIES_DIR}"
	fi

	return 0
}

function install_pbp()
{
	mk_info "Install Pre-Boot-Program file ..."
	for pbp in $(find ${TARGET_CHIP_DIR}/common/ -maxdepth 1 -name "*.pbp")
	do
		rm -rf ${BINARIES_DIR}/`basename ${pbp}`
		run_cmd "ln -sf ${pbp} ${BINARIES_DIR}/"
	done
	for pbp in $(find ${TARGET_BOARD_DIR}/ -maxdepth 1 -name "*.pbp")
	do
		rm -rf ${BINARIES_DIR}/`basename ${pbp}`
		run_cmd "ln -sf ${pbp} ${BINARIES_DIR}/"
	done
}

function mk_resource_private()
{
	local DDR_INIT1
	local DDR_INIT2

	mk_info "Install ddr init config file ..."

	DDR_INIT1="${TARGET_BOARD_DIR}/ddr_init.json"
	DDR_INIT2="${TARGET_CHIP_DIR}/common/ddr_init.json"

	rm -rf ${BINARIES_DIR}/ddr_init.json
	if [ -f ${DDR_INIT1} ]; then
		run_cmd "ln -sf ${DDR_INIT1} ${BINARIES_DIR}/"
	elif [ -f ${DDR_INIT2} ]; then
		run_cmd "ln -sf ${DDR_INIT2} ${BINARIES_DIR}/"
	else
		return 0
	fi

	# Generate resource private data
	run_cmd "${MKRESPRIVPY} -v -c ${BINARIES_DIR}/ddr_init.json -o ${BINARIES_DIR}/ddr_init.bin"

	return 0
}

function mk_image_file()
{
	local IMGCFG

	mk_info "Create image file ..."

	run_cmd "cp ${BINARIES_DIR}/Image.gz ${BINARIES_DIR}/Recovery.gz"
	run_cmd "cp ${BINARIES_DIR}/Image.gz ${TARGET_CHIP_DIR}/common/Recovery.gz"

	return 0
}

function pinmux_check()
{
	mk_info "Pinmux check ..."

	python3 ${PINMUXCHECKPY} ${DTBDIR}
}

function main()
{
	mk_uboot_env
	mk_boot_logo
	mk_resource_private
	install_pbp
	pinmux_check
	mk_rsa_key
	mk_image_file
}

main
