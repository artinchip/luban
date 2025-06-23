#!/bin/bash

array=("BR2_TARGET_ROOTFS_UBIFS_MAX_SIZE",
	"BR2_TARGET_USERFS1_UBIFS_MAX_SIZE",
	"BR2_TARGET_USERFS2_UBIFS_MAX_SIZE",
	"BR2_TARGET_USERFS3_UBIFS_MAX_SIZE",
	"BR2_TARGET_ROOTFS_EXT2_SIZE",
	"BR2_TARGET_USERFS1_EXT4_SIZE",
	"BR2_TARGET_USERFS2_EXT4_SIZE",
	"BR2_TARGET_USERFS3_EXT4_SIZE"
	"BR2_TARGET_ROOTFS_JFFS2_PADSIZE",
	"BR2_TARGET_USERFS1_JFFS2_PADSIZE",
	"BR2_TARGET_USERFS2_JFFS2_PADSIZE",
	"BR2_TARGET_USERFS3_JFFS2_PADSIZE"
	)

if [[ "${array[@]}" =~ "${2}" ]]; then
	if [ -z "${1}" ] || [ -z "${2}" ]
	then
		exit 1
	else
		RES=$(python3 tools/scripts/get_fs_max_size.py -c ${1} -d ${2} )
		echo ${RES}
	fi
elif [[ ! "${array[@]}" =~ "${2}" ]]; then
	echo "$2 not exists"
	exit 1
fi

exit 1
