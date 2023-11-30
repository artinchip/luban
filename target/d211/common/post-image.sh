#!/bin/bash

MKIMAGEPY=tools/scripts/mk_image.py
MKRESPRIVPY=tools/scripts/mk_private_resource.py
MKENVIMAGE=${HOST_DIR}/bin/mkenvimage
PINMUXCHECKPY=tools/scripts/pinmux_check.py
DTBDIR=${BINARIES_DIR}/u-boot.dtb
PACKAGECHECKPY=tools/scripts/package_check.py
KERNEL_HEADER_DIR=${TOPDIR}/source/linux-5.10/include
UBOOT_HEADER_DIR=${TOPDIR}/source/uboot-2021.10/include

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
	local LOGO_DIR
	local BOOT_LOGO

	LOGO_DIR="${TARGET_BOARD_DIR}/logo"
	BOOT_LOGO="${TARGET_BOARD_DIR}/boot_logo.*"

	rm -rf ${BINARIES_DIR}/logo
	rm -rf ${BINARIES_DIR}/boot_logo.*
	if [ -d ${LOGO_DIR} ]; then
		mk_info "Install logo image ..."
		run_cmd "ln -sf ${LOGO_DIR} ${BINARIES_DIR}"
	elif [ -f ${BOOT_LOGO} ]; then
		mk_info "Install boot logo image ..."
		run_cmd "ln -sf ${BOOT_LOGO} ${BINARIES_DIR}"
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

function install_dts()
{
	local DTSDIR="${BINARIES_DIR}/dts"

	if [ ! -d ${DTSDIR} ]; then
		mkdir -p ${DTSDIR}
	fi

	mk_info "Install dts file ..."
	for dtsi in $(find ${TARGET_CHIP_DIR}/common/ ${TARGET_BOARD_DIR}/ -maxdepth 1 -name "*.dtsi")
	do
		rm -rf ${DTSDIR}/`basename ${dtsi}`
		run_cmd "ln -sf ${dtsi} ${DTSDIR}/"
		HEADER_FILES=`awk -F "[<>]" '/#include/ {print $2}' ${dtsi}`
		for HEADER_FILE in ${HEADER_FILES[@]}; do
			UBOOT_HEADER_FILE="${UBOOT_HEADER_DIR}/${HEADER_FILE}"
			KERNEL_HEADER_FILE="${KERNEL_HEADER_DIR}/${HEADER_FILE}"
			BINARIES_HEADER_DIR=`dirname ${DTSDIR}/${HEADER_FILE}`
			if [ ! -d ${BINARIES_HEADER_DIR} ]; then
				mkdir -p ${BINARIES_HEADER_DIR}
			fi

			rm -rf "${DTSDIR}/${HEADER_FILE}"
			if [ -f ${UBOOT_HEADER_FILE} ]; then
				# run_cmd "ln -sf ${UBOOT_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
				run_cmd "cp ${UBOOT_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
			fi
			if [ -f ${KERNEL_HEADER_FILE} ]; then
				# run_cmd "ln -sf ${KERNEL_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
				run_cmd "cp ${KERNEL_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
			fi
		done
	done
	for dts in $(find ${TARGET_CHIP_DIR}/common/ ${TARGET_BOARD_DIR}/ -maxdepth 1 -name "*.dts")
	do
		rm -rf ${DTSDIR}/`basename ${dts}`
		run_cmd "ln -sf ${dts} ${DTSDIR}/"
		HEADER_FILES=`awk -F "[<>]" '/#include/ {print $2}' ${dts}`
		for HEADER_FILE in ${HEADER_FILES[@]}; do
			UBOOT_HEADER_FILE="${UBOOT_HEADER_DIR}/${HEADER_FILE}"
			KERNEL_HEADER_FILE="${KERNEL_HEADER_DIR}/${HEADER_FILE}"
			BINARIES_HEADER_DIR=`dirname ${DTSDIR}/${HEADER_FILE}`
			if [ ! -d ${BINARIES_HEADER_DIR} ]; then
				mkdir -p ${BINARIES_HEADER_DIR}
			fi

			rm -rf ${DTSDIR}/${HEADER_FILE}
			if [ -f ${UBOOT_HEADER_FILE} ]; then
				# run_cmd "ln -sf ${UBOOT_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
				run_cmd "cp ${UBOOT_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
			fi
			if [ -f ${KERNEL_HEADER_FILE} ]; then
				# run_cmd "ln -sf ${KERNEL_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
				run_cmd "cp ${KERNEL_HEADER_FILE} ${BINARIES_HEADER_DIR}/"
			fi
		done
	done
}


function install_ota_image()
{
	mk_info "Install OTA recovery image file ..."
	for ota in $(find ${TARGET_CHIP_DIR}/common/ -maxdepth 1 -name "Recovery.gz")
	do
		rm -rf ${BINARIES_DIR}/`basename ${ota}`
		run_cmd "ln -sf ${ota} ${BINARIES_DIR}/"
	done
	for ota in $(find ${TARGET_BOARD_DIR}/ -maxdepth 1 -name "Recovery.gz")
	do
		rm -rf ${BINARIES_DIR}/`basename ${ota}`
		run_cmd "ln -sf ${ota} ${BINARIES_DIR}/"
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

	IMGCFG="${TARGET_BOARD_DIR}/image_cfg.json"

	rm -rf ${BINARIES_DIR}/image_cfg.json
	run_cmd "ln -sf ${IMGCFG} ${BINARIES_DIR}/"
	# Copy file in common first
	for its in $(find ${TARGET_CHIP_DIR}/common/ -maxdepth 1 -name "*.its")
	do
		rm -rf ${BINARIES_DIR}/`basename ${its}`
		run_cmd "ln -sf ${its} ${BINARIES_DIR}/"
	done
	# Use the file in board to overwrite it if file name is the same
	for its in $(find ${TARGET_BOARD_DIR}/ -maxdepth 1 -name "*.its")
	do
		rm -rf ${BINARIES_DIR}/`basename ${its}`
		run_cmd "ln -sf ${its} ${BINARIES_DIR}/"
	done

	# Generate image
	rm -rf ${BINARIES_DIR}/*.itb
	if [ "$(grep BR2_GENERATE_BURNER_IMAGE=y ${BR2_CONFIG})" != "" ]; then
		run_cmd "${MKIMAGEPY} -v -b -c ${BINARIES_DIR}/image_cfg.json -d ${BINARIES_DIR}"
	else
		run_cmd "${MKIMAGEPY} -v -c ${BINARIES_DIR}/image_cfg.json -d ${BINARIES_DIR}"
	fi

	return 0
}

function pinmux_check()
{
	mk_info "Pinmux check ..."

	python3 ${PINMUXCHECKPY} ${DTBDIR}
}

function package_check()
{
	mk_info "Package check ..."

	python3 ${PACKAGECHECKPY} ${DTBDIR}
}

function swupdate_pack_swu()
{
	local PACK_NAME="${LUBAN_CURRENT_OUT}"
	local SWU_DIR="${BINARIES_DIR}/swupdate"
	local SWU_IMAGES="${TARGET_BOARD_DIR}/swupdate/sw-images.cfg"
	mkdir -p ${SWU_DIR}

	mk_info "Pack swu ..."

	. $SWU_IMAGES

	cp "$SWU_IMAGES" "$SWU_DIR"
	rm -f "$SWU_DIR/sw-subimgs-fix.cfg"

	mk_info "Show images ..."
	echo ${swota_file_list[@]} | sed 's/ /\n/g'

	mk_info "Do copy ..."

	for line in ${swota_file_list[@]} ; do
		ori_file=$(echo $line | awk -F: '{print $1}')
		base_name=$(basename "$line")
		fix_name=${base_name#*:}
		[ ! -f "$ori_file" ] &&  mk_info "$ori_file not exist!!" && return 1
		cp $ori_file $SWU_DIR/$fix_name
		echo $fix_name >> "$SWU_DIR/sw-subimgs-fix.cfg"
	done
	cd "$SWU_DIR"

	mk_info "Do sha256 ..."
	cp sw-description sw-description.bk

	[ -f $SWU_DIR/sw-subimgs-fix.cfg ] && {
		while IFS= read -r line
		do
			item="$line"
			if grep -q -E "sha256 = \"@$item\"" sw-description ; then
				echo "sha256sum $item"
				item_hash=$(sha256sum "$item" | awk '{print $1}')
				sed -i "s/\(.*\)\(sha256 = \"@$item\"\)/\1sha256 = \"$item_hash\"/g" sw-description
			fi
		done < "$SWU_DIR/sw-subimgs-fix.cfg"
	}

	mk_info "Do cpio ..."
	while IFS= read -r line
	do
		echo "$line"
	done < "$SWU_DIR/sw-subimgs-fix.cfg" | cpio -ov -H crc > "$SWU_DIR/${LUBAN_CURRENT_OUT}.swu"

	mk_info "OTA image file is generated..."
	du -sh "$SWU_DIR/${LUBAN_CURRENT_OUT}.swu"

	cd - > /dev/null
}

function main()
{
	mk_uboot_env
	mk_boot_logo
	mk_resource_private
	install_dts
	install_pbp
	pinmux_check
	package_check
	mk_rsa_key
	install_ota_image
	mk_image_file
	if [ "$(grep BR2_PACKAGE_SWUPDATE=y ${BR2_CONFIG})" != "" ]; then
		swupdate_pack_swu
	fi
}

main
