#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2022-2023 ArtInChip Technology Co., Ltd
# Dehuang Wu <dehuang.wu@artinchip.com>

MAX_LINES=20
export LUBAN_PRJ_TOP_DIR=
LUBAN_PRJ_FFF=

backup_list=
display_list=
display_list_total=

# $1 - the command name
function _unalias()
{
	CMD=$1
	alias | grep $CMD"=" -w > /dev/null
	if [ $? -eq 0 ]; then
		unalias $CMD
	fi
}

function _clear_env()
{
	unset menuconfig
	unset mc
	unset aicupg
	unset addboard
	_unalias m
	_unalias ab
	_unalias ma
	_unalias mc
	_unalias ms
	_unalias mu
	_unalias mb
	_unalias km
	_unalias update
}

function hmm()
{
	echo "Luban SDK commands:"
	_hline "hmm|h" "" "Get this help."
	_hline "lunch" "[keyword]" "Start with selected defconfig.e.g. lunch mmc"
	_hline "m" "" "make all modules and generate final image"
	_hline "c" "" "clean all"
	_hline "mm" "" "make module only in module's source code root directory"
	_hline "me" "" "menuconfig of the SDK"
	_hline "km" "" "menuconfig of the Linux Kernel"
	_hline "um|bm" "" "menuconfig of the U-Boot"
	_hline "croot|cr" "" "cd to SDK root directory."
	_hline "cout|co" "" "cd to build output directory."
	_hline "cbuild|cb" "" "cd to build root directory."
	_hline "ckernel|ck" "" "cd to kernel source code directory."
	_hline "cuboot|cu" "" "cd to U-Boot source code directory."
	_hline "ctarget|ct" "" "cd to target board directory."
	_hline "godir|gd" "[keyword]" "Go/jump to selected directory."
	_hline "genindex|gi" "[keyword]" "Generate directory list for quick jump."
	_hline "goexplorer|ge" "[keyword]" "Open explorer with selected directory."
	_hline "list" "" "List all SDK defconfig."
	_hline "list_module" "" "List all enabled modules."
	_hline "i" "" "Get current project's information."
	_hline "buildall"   "" "Build all the *defconfig in target/configs"
	_hline "rebuildall" "" "Clean and build all the *defconfig in target/configs"
	echo ""
}
alias h=hmm

function lunch()
{
	local keyword="$*"
	local defconfigs=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		return
	}

	cd ${LUBAN_PRJ_TOP_DIR} || exit
	# Get the SDK's defconfig list
	defconfigs=$(_get_defconfig_list)

	select_item=
	if [ "${keyword}" != "" ]; then
		select_item=$(echo "${defconfigs}" | grep "${keyword}")
	fi
	if [ "${keyword}" == "" -o "${select_item}" != "${keyword}" ]; then
		_display_list_init "${defconfigs}"
		select_item=
		_search_in_list "${keyword}"

		# Not select any defconfig, cancel lunch
		[[ ${select_item} == "" ]] && {
			_display_list_clear
			return
		}
	fi

	defconfig=${select_item}
	echo "make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} ${defconfig}"
	make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} ${defconfig}
	_display_list_clear
}

function croot()
{
	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		echo "Not lunch project yet"
		return
	}
	cd ${LUBAN_PRJ_TOP_DIR} || exit
}
alias cr=croot

function ckernel()
{
	local kernel_dir=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		echo "Not lunch project yet"
		return
	}

	_get_linux_ver
	kernel_dir="${LUBAN_PRJ_TOP_DIR}/source/linux-"${LINUX_VER}
	cd ${kernel_dir} || exit
}
alias ck=ckernel

function cuboot()
{
	local uboot_dir=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		echo "Not lunch project yet"
		return
	}

	_get_uboot_ver
	uboot_dir="${LUBAN_PRJ_TOP_DIR}/source/uboot-"${UBOOT_VER}
	cd ${uboot_dir} || exit
}
alias cu=cuboot

function ctarget()
{
	local build_dir=
	local target_dir=
	local chip_dir=
	local board_dir=
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	build_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
	eval ${build_dir}
	build_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/

	chip_dir=$(grep "LUBAN_CHIP_NAME=" ${build_dir}/.config)
	eval ${chip_dir}
	board_dir=$(grep "LUBAN_BOARD_NAME=" ${build_dir}/.config)
	eval ${board_dir}

	target_dir=${LUBAN_PRJ_TOP_DIR}/target/${LUBAN_CHIP_NAME}/${LUBAN_BOARD_NAME}/
	cd ${target_dir} || exit
}
alias ct=ctarget

function cout()
{
	local build_dir=
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	build_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
	eval ${build_dir}
	build_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/
	cd ${build_dir} || exit
}
alias co=cout

function cbuild()
{
	local build_dir=
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	build_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
	eval ${build_dir}
	build_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/build
	cd ${build_dir} || exit
}
alias cb=cbuild

function time_begin()
{
	start_sec=$(date +"%s")
}

function time_end()
{
	local end_sec="$(date +"%s")"
	local interval=$(($end_sec - $start_sec))
	local hour=$(($interval / 3600))
	local min=$((($interval % 3600) / 60))
	local sec=$(($interval % 60))

	printf "\t\t\t\t\tUsed time: %02d:%02d:%02d\n" $hour $min $sec
}

function time_now()
{
	TM_STR=$(date +"%Y-%m-%d %H:%M:%S")
	echo "$(date -d "$TM_STR" +%s)"
}

# $1: before, in seconds
# $2: after, in seconds
function time_diff()
{
	BEFORE=$1
	AFTER=$2
	HOUR=0
	MINUTER=0
	SECOND=0

	INTERVAL=`expr $AFTER - $BEFORE`

	if [ ! $INTERVAL -lt 3600 ]; then
		HOUR=`expr $INTERVAL / 3600`
		INTERVAL=`expr $INTERVAL % 3600`
	elif [ ! $INTERVAL -lt 60 ]; then
		MINUTER=`expr $INTERVAL / 60`
		SECOND=`expr $INTERVAL % 30`
	else
		SECOND=$INTERVAL
	fi
	printf "%02d:%02d:%02d" $HOUR $MINUTER $SECOND
}

function m()
{
	local ret=

	time_begin

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} all
	ret=$?
	if [ $ret -ne 0 ]; then
		return $ret
	fi
	time_end
}

# Make module in package source code root directory (rebuild)
function mm()
{
	local ret=

	time_begin

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	spath=${PWD}
	spath=${spath/${LUBAN_PRJ_TOP_DIR}\//}
	if [[ "${spath##source*}" != "" ]]; then
		echo "Should use this command in package's source root directory."
		return
	fi
	srcpkgname=`basename ${spath}`
	pkgs=`make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} show-all-packages`
	targetname=""
	for pkg in ${pkgs}
	do
		if [ ${srcpkgname} == ${pkg} ]; then
			targetname=${pkg}
			break
		fi
		ver=${srcpkgname/${pkg}-/}
		if [[ "${ver}" == "${srcpkgname}" ]]; then
			# Not a package source
			continue
		fi
		if [[ ! "${ver}" =~ "-" ]]; then
			# Not inculde "-", it is a version name
			targetname=${pkg}
			break
		fi
	done
	if [ "${targetname}" == "" ]; then
		echo -e "\e[1;36m${srcpkgname}\e[0m is not a package."
		return
	fi

	make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} "${targetname}-rebuild"
	ret=$?
	if [ $ret -ne 0 ]; then
		return $ret
	fi
	time_end
}

# $1: defconfig name
# $2 - if clean before make
# Note: Must define the global variable:
#     $BUILD_CNT, $RESULT_FILE, $WARNING_FILE, $LOG_DIR
function build_one_solution()
{
	DEFCONFIG_NAME=$1
	NEED_CLEAN=$2

	DEFCONFIG_NAME_SHORT=${DEFCONFIG_NAME::-10}
	LOG_FILE=${LOG_DIR}/${DEFCONFIG_NAME_SHORT}.log
	echo
	echo --------------------------------------------------------------
	echo Build $DEFCONFIG_NAME_SHORT
	echo --------------------------------------------------------------

	make $DEFCONFIG_NAME

	if [ ! -z $NEED_CLEAN ]; then
		make clean
	fi

	TIME1=`time_now`
	make 2>&1 | tee $LOG_FILE
	TIME2=`time_now`
	INTERVAL=`time_diff $TIME1 $TIME2`

	BUILD_CNT=`expr $BUILD_CNT + 1`
	grep "Luban is built successfully" $LOG_FILE -w > /dev/null
	if [ $? -eq 0 ]; then
		WAR_CNT=`grep -E "warning:|warning |The conflicting pin|conflicts with" $LOG_FILE -ic`
		printf "%2s) %-28s is OK. Warning: %s. Time: %s\n" \
			$BUILD_CNT $DEFCONFIG_NAME_SHORT $WAR_CNT $INTERVAL >> $RESULT_FILE
		if [ $WAR_CNT -gt 0 ]; then
			echo [$DEFCONFIG_NAME_SHORT]: >> $WARNING_FILE
			grep -E "warning:|warning |The conflicting pin|conflicts with" $LOG_FILE -i >> $WARNING_FILE

			echo >> $WARNING_FILE
		fi
	else
		printf "%2s) %-28s is failed. Time: %s\n" \
			$BUILD_CNT $DEFCONFIG_NAME_SHORT $INTERVAL >> $RESULT_FILE
	fi
}

# $1 - if clean before make
function build_check_all()
{
	BUILD_CNT=0
	LOG_DIR=$LUBAN_PRJ_TOP_DIR/.log
	RESULT_FILE=$LOG_DIR/result.log
	WARNING_FILE=$LOG_DIR/warning.log

	if [ ! -d $LOG_DIR ]; then
		mkdir $LOG_DIR
	fi
	rm -f $RESULT_FILE $WARNING_FILE

	defconfigs=$(_get_defconfig_list)

	for config in $defconfigs
	do
		build_one_solution $config $1
	done

	echo
	echo --------------------------------------------------------------
	echo The build result of all solution:
	echo --------------------------------------------------------------
	cat $RESULT_FILE

	if [ -f $WARNING_FILE ]; then
		echo
		echo --------------------------------------------------------------
		echo The warning information of all solution:
		echo --------------------------------------------------------------
		cat $WARNING_FILE
		echo
	fi
}

function buildall()
{
	build_check_all
}

function rebuildall()
{
	build_check_all clean
}

function c()
{
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} clean
}

function me()
{
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	make menuconfig --no-print-directory  -C $LUBAN_PRJ_TOP_DIR
}

function km()
{
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	make kernel-menuconfig --no-print-directory  -C $LUBAN_PRJ_TOP_DIR
}

function um()
{
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	make uboot-menuconfig --no-print-directory  -C $LUBAN_PRJ_TOP_DIR
}
alias bm=um

function godir()
{
	local keyword="$*"
	local dir_list=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		return
	}
	dir_list=$(_get_dir_list)
	_display_list_init "${dir_list}"
	select_item=""
	_search_in_list "${keyword}"
	# change directory
	[[ ! ${select_item} == "" ]] && {
		cd ${LUBAN_PRJ_TOP_DIR}/${select_item} || exit
	}
	_display_list_clear
}
alias gd=godir

function goexplorer()
{
	local keyword="$*"
	local dir_list=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		return
	}
	[[ -z ${LUBAN_PRJ_FFF} ]] && {
		return
	}
	dir_list=$(_get_dir_list)
	_display_list_init "${dir_list}"
	select_item=""
	_search_in_list "${keyword}"
	# open directory with fff
	[[ ! ${select_item} == "" ]] && {
		${LUBAN_PRJ_FFF} ${LUBAN_PRJ_TOP_DIR}/${select_item}
	}
	_display_list_clear
}
alias ge=goexplorer

function genindex()
{
	local keyword="$*"
	local gen_options=`printf "linux\nuboot\nthird-party\nall\n"`
	local result=
	local gen_path=
	local dir_list1=
	local dir_list2=
	local topdir_tmp=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		return
	}

	if [ "${keyword}" != "" ]; then
		result=$(echo "${gen_options}" | grep "${keyword}")
	fi
	if [ "${result}" == "" ]; then
		_display_list_init "${gen_options}"
		select_item=""
		_search_in_list ""
		# change directory
		if [ ! ${select_item} == "" ]; then
			result="${select_item}"
		else
			_display_list_clear
			return
		fi
	fi

	case ${result} in
		"linux")
			_get_linux_ver
			gen_path=${LUBAN_PRJ_TOP_DIR}"/source/linux-"${LINUX_VER}
		;;
		"uboot")
			_get_uboot_ver
			gen_path=${LUBAN_PRJ_TOP_DIR}"/source/uboot-"${UBOOT_VER}
		;;
		"third-party")
			gen_path=${LUBAN_PRJ_TOP_DIR}/source/third-party
		;;
		"all")
			gen_path=${LUBAN_PRJ_TOP_DIR}
		;;
	esac

	printf "Generating directory list ..."
	dir_list1=`find ${gen_path}/ -type d ! -path "*/.*"`
	topdir_tmp=${LUBAN_PRJ_TOP_DIR//\//\\\/}"\\/"
	dir_list2=`echo "${dir_list1}" | sort | sed 's/'"${topdir_tmp}"'//g'`

	echo "${dir_list2}" >${LUBAN_PRJ_TOP_DIR}/.dirlist

	printf " Done\n"
	_display_list_clear
}
alias gi=genindex

function _info()
{
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} info
}
alias i=_info

function list()
{
	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		echo "Not lunch project yet"
		return
	}

	make --no-print-directory -C ${LUBAN_PRJ_TOP_DIR} list
}

function list_module()
{
	local ret=

	ret=$(_lunch_check)
	if [ "${ret}" == "false" ]; then
		echo "Not lunch project yet"
		return
	fi

	images_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
	eval ${images_dir}
	images_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/images
	printf "Load modules information from ${LUBAN_CURRENT_OUT}\n"

	if [ ! -f ${LUBAN_PRJ_TOP_DIR}/output/${LUBAN_CURRENT_OUT}/host/bin/python3 ]; then
		echo "The SDK environment for Python3 is not prepared yet"
		make host-python3-fdt
	fi

	if [ ! -f ${images_dir}/u-boot.dtb ]; then
		echo "The u-boot.dtb file is not prepared yet"
		make b
	fi

	${LUBAN_PRJ_TOP_DIR}/output/${LUBAN_CURRENT_OUT}/host/bin/python3 \
	${LUBAN_PRJ_TOP_DIR}/tools/scripts/list_module.py -d ${images_dir}
}

function _hline()
{
	local cmd="$1"
	local opt="$2"
	local txt="$3"

	if [ "${cmd}" == "" ]; then
		printf "  ${txt}\n"
	elif [ "${opt}" == "" ]; then
		printf "  \e[1;36m%-25s\e[0m : %s\n" "${cmd}" "${txt}"
	else
		printf "  \e[1;36m%-14s\e[0m \e[0;35m%-10s\e[0m : %s\n" "${cmd}" "${opt}" "${txt}"
	fi
}

function _get_defconfig_list()
{
	local defconfigs=

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		return
	}

	[[ ! -d ${LUBAN_PRJ_TOP_DIR}/target/configs/ ]] && {
		return
	}
	cd ${LUBAN_PRJ_TOP_DIR}/target/configs/ || exit
	defconfigs=`ls -1 *_defconfig`
	cd - > /dev/null || exit
	echo "${defconfigs}"
}

function _get_build_dir()
{
	build_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
	eval ${build_dir}
	build_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/
}

function _get_linux_ver()
{
	_get_build_dir
	DEFCONF_FILE=${build_dir}/.config

	LINUX_VER=`grep BR2_LINUX_KERNEL_VERSION $DEFCONF_FILE | awk -F '=' '{print $2}'`
	LINUX_VER=`echo ${LINUX_VER//\"/}`
}

function _get_uboot_ver()
{
	_get_build_dir
	DEFCONF_FILE=${build_dir}/.config

	UBOOT_VER=`grep BR2_TARGET_UBOOT_VERSION $DEFCONF_FILE | awk -F '=' '{print $2}'`
	UBOOT_VER=`echo ${UBOOT_VER//\"/}`
}

function _lunch_check()
{
	local build_dir

	[[ -z ${LUBAN_PRJ_TOP_DIR} ]] && {
		echo "false"
		return
	}

	if [ -f ${LUBAN_PRJ_TOP_DIR}/output/.current ]; then
		build_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
		eval ${build_dir}
		build_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/
	else
		echo "false"
		return
	fi
	if [ -f "${build_dir}/.config" ]; then
		echo "true"
		return
	fi
	echo "false"
}

function _get_dir_list()
{
	local dir_list1
	local dir_list2
	local topdir_tmp
	local build_dir
	local sep

	sep="\n"

	dir_list1=`find ${LUBAN_PRJ_TOP_DIR}/package/ -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/target/ -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/dl/ -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/prebuilt/ -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/tools/ -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/source/ -maxdepth 1 -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/source/artinchip/ -maxdepth 1 -type d ! -path "*/.*"`
	dir_list1+="${sep}"
	dir_list1+=`find ${LUBAN_PRJ_TOP_DIR}/source/third-party/ -maxdepth 1 -type d ! -path "*/.*"`

	if [ -f ${LUBAN_PRJ_TOP_DIR}/output/.current ]; then
		build_dir=$(cat ${LUBAN_PRJ_TOP_DIR}/output/.current)
		eval ${build_dir}
		build_dir="${LUBAN_PRJ_TOP_DIR}/output/"${LUBAN_CURRENT_OUT}/build
	fi
	if [ -d "${build_dir}" ]; then
		dir_list1+="${sep}"
		dir_list1+=`find ${build_dir}/ -maxdepth 1 -type d ! -path "*/.*"`
	fi

	topdir_tmp=${LUBAN_PRJ_TOP_DIR//\//\\\/}"\\/"
	dir_list2=`echo -e "${dir_list1}" | sort | sed 's/'"${topdir_tmp}"'//g'`

	# If cached directory list exist, load cached list
	if [ -f ${LUBAN_PRJ_TOP_DIR}/.dirlist ]; then
		local cached_list
		cached_list=`cat ${LUBAN_PRJ_TOP_DIR}/.dirlist`
		dir_list2=`echo -e "${dir_list2}\n${cached_list}" | sort -u`
	fi
	# echo -e "${dir_list2}" >debug.txt
	echo -e "${dir_list2}"
}

function _mark_topdir()
{
	# User may source this file in Luban top dir, or in envsetup.sh dir
	if [ -f tools/envsetup.sh ]; then
		LUBAN_PRJ_TOP_DIR=$(pwd)
	elif [ -f ../tools/envsetup.sh ]; then
		LUBAN_PRJ_TOP_DIR=$(cd .. && pwd)
	else
		echo 'Please "source tools/envsetup.sh" in Luban SDK Root directory'
		return
	fi
	if [ -f ${LUBAN_PRJ_TOP_DIR}/tools/scripts/bin/fff ]; then
		LUBAN_PRJ_FFF=${LUBAN_PRJ_TOP_DIR}/tools/scripts/bin/fff
	fi
}

function _setup_terminal()
{
	# Setup the terminal for the TUI.
	# '\e[?1049h': Use alternative screen buffer.
	# '\e[?7l':    Disable line wrapping.
	printf '\e[?1049h\e[?7l'

	# Hide echoing of user input
	stty -echo
}

function _reset_terminal()
{
	printf "\n"
	# Clear lines
	for ((i=0;i<MAX_LINES;i++)); {
		printf '\e[K'
		printf '\n'
	}
	# Move cursor back to input line
	((JUMPBACK_INPUTLINE=${MAX_LINES}+1))
	printf '\e[%sA' ${JUMPBACK_INPUTLINE}

	# Reset the terminal to a useable state (undo all changes).
	# '\e[K':     Clear line
	# '\e[?7h':   Re-enable line wrapping.
	# '\e[?25h':  Unhide the cursor.
	# '\e[2J':    Clear the terminal.
	# '\e[;r':    Set the scroll region to its default value.
	#             Also sets cursor to (0,0).
	# '\e[?1049l: Restore main screen buffer.
	printf '\e[K\e[?7h\e[?25h'
	# Show user input.
	stty echo
}

function _get_term_size()
{
	local max_items=
	local IFSBK=

	MAX_LINES=20
	IFSBK=${IFS}
	IFS=$' \t\n'
	# Get terminal size ('stty' is POSIX and always available).
	# This can't be done reliably across all bash versions in pure bash.
	read -r LINES COLUMNS < <(stty size)
	IFS=${IFSBK}

	# Max list items that fit in the scroll area.
	((max_items=LINES-3))
	((MAX_LINES>max_items)) &&
		MAX_LINES=${max_items}
}

function _arrow_key()
{
	case ${1} in
		# Scroll down.
		# 'B' is what bash sees when the down arrow is pressed
		# ('\e[B' or '\eOB').
		$'\e[B'|\
		$'\eOB')
			((scroll<display_list_total)) && {
				((scroll++))
				_redraw
			}
		;;

		# Scroll up.
		# 'A' is what bash sees when the up arrow is pressed
		# ('\e[A' or '\eOA').
		$'\e[A'|\
		$'\eOA')
			((scroll>0)) && {
				((scroll--))
				_redraw
			}
		;;
	esac
}

function _key_loop()
{
	local input_kw="${2}"
	local new_key
	local array

	select_item=""
	while IFS= read -rsn 1 -p $'\r\e[K'"${1}${input_kw}" new_key; do

		[[ ${new_key} == $'\e' ]] && {
			read "${read_flags[@]}" -rsn 2 new_key2

			# Esc key to exit
			[[ ${new_key2} == "" ]] && return
			_arrow_key ${new_key}${new_key2}
			continue
		}

		case ${new_key} in
			# Backspace.
			$'\177'|$'\b')
				input_kw=${input_kw%?}
			;;

			# Enter/Return/Tab
			""|$'\t')
				array=(${display_list[@]})
				select_item=${array[$scroll]}
				return
			;;
			# Anything else, add it to read reply.
			*)
				input_kw+=${new_key}
			;;
		esac

		# Filter with keyword
		_update_display_with_kw "${input_kw}"

		scroll=0
		_redraw
	done

}

function _draw_line()
{
	# Format the list item and print it.
	local file_name=$2
	local format

	# If the list item is under the cursor.
	(($1 == scroll)) && format+="\\e[1;36;7m"

	# Escape the directory string.
	# Remove all non-printable characters.
	file_name=${file_name//[^[:print:]]/^[}

	# Clear line before changing it.
	printf '\e[K'
	printf '%b%s\e[m\n' "  ${format}" "${file_name}"
}

function _update_display_with_kw()
{
	local input_kw="$1"
	local kw=
	local match_list=

	# Filter with keyword
	if [ ! -z "${input_kw}" ]; then
		kw=${input_kw// /.*}
		kw=${kw//\\/.*}
		kw=${kw//\//\\\/}
		match_list=`echo "${backup_list}" | sed -n '/'"${kw}"'/p'`
		# debug
		# echo "${match_list}" >match.list
		display_list=(${match_list[@]})
		# ((display_list_total=${#display_list[@]}-1))
		((display_list_total=${#display_list[@]}))

	else
		match_list=${backup_list}
		display_list=(${match_list[@]})
		# ((display_list_total=${#display_list[@]}-1))
		((display_list_total=${#display_list[@]}))
	fi

}

function _display_list_init()
{
	local input_list="$1"

	# Save the original data in a second list as a backup.
	backup_list="${input_list}"
	_update_display_with_kw ""
}

function _display_list_clear()
{
	backup_list=""
	display_list=""
	display_list_total=0
}

function _redraw()
{
	# If no content in list, don't draw it
	[[ -z ${backup_list} ]] && return

	start_item=0
	((scroll>=MAX_LINES)) && {
		((start_item=${scroll}-${MAX_LINES}+1))
	}

	((end_item=${start_item}+${MAX_LINES}))

	# '\e[?25l': Hide the cursor.
	printf '\e[?25l\n'
	for ((i=start_item;i<end_item;i++)); {
		if [ ${i} -le ${display_list_total} ]; then
			_draw_line $i ${display_list[i]}
		else
			_draw_line $i ""
		fi
	}

	# '\e[NA':  Move cursor up N line
	# '\e[?25h': Unhide the cursor.
	printf '\e[21A\e[?25h>'
}

function _search_in_list()
{
	local keyword="$*"
	local match_list

	# Trap the exit signal (we need to reset the terminal to a useable state.)
	trap '_reset_terminal' EXIT

	# Trap the window resize signal (handle window resize events).
	trap '_get_term_size; _redraw' WINCH

	# bash 5 and some versions of bash 4 don't allow SIGWINCH to interrupt
	# a 'read' command and instead wait for it to complete. In this case it
	# causes the window to not redraw on resize until the user has pressed
	# a key (causing the read to finish). This sets a read timeout on the
	# affected versions of bash.
	# NOTE: This shouldn't affect idle performance as the loop doesn't do
	# anything until a key is pressed.
	# SEE: https://github.com/dylanaraps/fff/issues/48
	((BASH_VERSINFO[0] > 3)) &&
		read_flags=(-t 0.05)
	_get_term_size

	[[ ! -z "${keyword}" ]] && {
		_update_display_with_kw "${keyword}"
	}
	scroll=0
	_redraw

	_key_loop "> " "${keyword}"
	_reset_terminal
}

_clear_env
_mark_topdir

export PATH=$PATH:${LUBAN_PRJ_TOP_DIR}/tools/scripts/bin

# Avoid conflict with Luban-Lite me command
type me | grep alias > /dev/null
if [ $? -eq 0 ]; then
	unalias me
fi
