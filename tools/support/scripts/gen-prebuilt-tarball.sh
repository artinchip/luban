#!/bin/bash

PKG_NAME="${1}"
PKG_BASENAME="${2}"
PKG_BUILDDIR="${3}"
HOST_ARCH="${4}"
TARGET_ARCH="${5}"

# Prepare directory
PREBUILT_PKGDIR=${PKG_BUILDDIR}/prebuilt
PREBUILT_PKGDIR2=${PKG_BUILDDIR}/prebuilt/${PKG_BASENAME}
PREBUILT_HOST_DIR=${PREBUILT_PKGDIR2}/host
PREBUILT_STAGING_DIR=${PREBUILT_PKGDIR2}/${STAGING_SUBDIR}
PREBUILT_IMAGES_DIR=${PREBUILT_PKGDIR2}/images
PREBUILT_TARGET_DIR=${PREBUILT_PKGDIR2}/target
PREBUILT_PKG_RESOLVE=${PREBUILT_PKGDIR2}/resolve-files.txt

: ${PATCHELF:=${HOST_DIR}/bin/patchelf}

is_text_file()
{
    local filename="${1}"

    mimetype=`file -bi "${filename}"`
    if echo "${mimetype}" | grep -q "text" ; then
        return 1
    fi

    return 0
}

is_elf_file()
{
    local filename="${1}"

    mimetype=`file -bi "${filename}"`
    if echo "${mimetype}" | grep -q "application" ; then
        return 1
    fi

    return 0
}

replace_absolute_path()
{
    local filetype="${1}"
    local filepath="${2}"
    local base_str
    local staging_base

    base_str="ARTINCHIP_PREBUILT_BASE"
    staging_base="ARTINCHIP_PREBUILT_STAGING_BASE"

    if [ -L "${filepath}" ]; then
        linkpath=$(readlink "${filepath}")
        changed_lpath=$(echo ${linkpath} | sed "s@${BASE_DIR}@${base_str}@g")
        if test "${linkpath}" != "${changed_lpath}" ; then
            ln -snf "${changed_lpath}" "${filepath}"
	    echo "${filepath}" |sed "s@${PREBUILT_PKGDIR2}/@@g" >> ${PREBUILT_PKG_RESOLVE}
        fi
    fi

    is_text_file "${filepath}"
    if [ $? -eq 1 ]; then
	needed1=$(grep ${BASE_DIR} "${filepath}")
	needed2=$(grep ${STAGING_SUBDIR} "${filepath}")
	if [ "${needed1}${needed2}" != "" ]; then
            # make files writable if necessary
            changed=$(chmod -c u+w "${filepath}")
            sed -i "s@${BASE_DIR}@${base_str}@g" "${filepath}"
            sed -i "s@${STAGING_SUBDIR}@${staging_base}@g" "${filepath}"
            # restore the original permission
            test "${changed}" != "" && chmod u-w "${filepath}"
	    echo "${filepath}" |sed "s@${PREBUILT_PKGDIR2}/@@g" >> ${PREBUILT_PKG_RESOLVE}
	fi
	return 0
    fi

    # patchelf tool not exist, just return
    if [ ! -f ${PATCHELF} ]; then
        echo "patchelf is not exist, cannot fixup rpath"
        return 0;
    fi
    is_elf_file "${filepath}"
    if [ $? -eq 1 ]; then
        rpath=$(${PATCHELF} --print-rpath "${filepath}" 2>&1)
        if test $? -ne 0 ; then
            return 0
        fi

        # make files writable if necessary
        changed=$(chmod -c u+w "${filepath}")

        changed_rpath=$(echo ${rpath} | sed "s@${BASE_DIR}@${base_str}@g")
        if test "${rpath}" != "${changed_rpath}" ; then
            ${PATCHELF} --set-rpath ${changed_rpath} "${filepath}"
	    echo "${filepath}" |sed "s@${PREBUILT_PKGDIR2}/@@g" >> ${PREBUILT_PKG_RESOLVE}
        fi

        # restore the original permission
        test "${changed}" != "" && chmod u-w "${filepath}"
    fi

    return 0
}

prepare_prebuilt_files()
{
    local filetype="${1}"
    local filelist="${2}"
    local src_root="${3}"
    local dst_root="${4}"
    local count=0

    if [ ! -f ${filelist} ]; then
        return 0
    fi

    while read line; do
        if [ ! -d "${src_root}/${line}" ]; then
            mkdir -p `dirname "${dst_root}/${line}"`
        fi
        cp -rdf "${src_root}/${line}" "${dst_root}/${line}"
	count=`expr ${count} + 1`
        # Replace the absolute path to ARTINCHIP_PREBUILT_HOST
        replace_absolute_path ${filetype} "${dst_root}/${line}"
    done < <( sed "s/${PKG_NAME},\.\///" ${filelist}; )

    return ${count}
}

main()
{
    local filelist
    local is_target
    local subdir

    rm -rf ${PREBUILT_PKGDIR}

    mkdir -p ${PREBUILT_PKGDIR2}
    touch ${PREBUILT_PKG_RESOLVE}

    filelist=${PKG_BUILDDIR}/.files-list-staging.txt
    prepare_prebuilt_files staging ${filelist} ${STAGING_DIR} ${PREBUILT_STAGING_DIR}
    if [ $? -ne 0 ]; then
	    is_target=y
    fi

    filelist=${PKG_BUILDDIR}/.files-list-images.txt
    prepare_prebuilt_files images ${filelist} ${BINARIES_DIR} ${PREBUILT_IMAGES_DIR}
    if [ $? -ne 0 ]; then
	    is_target=y
    fi

    filelist=${PKG_BUILDDIR}/.files-list.txt
    prepare_prebuilt_files target ${filelist} ${TARGET_DIR} ${PREBUILT_TARGET_DIR}
    if [ $? -ne 0 ]; then
	    is_target=y
    fi

    filelist=${PKG_BUILDDIR}/.files-list-host.txt
    prepare_prebuilt_files host ${filelist} ${HOST_DIR} ${PREBUILT_HOST_DIR}

    if [ "${is_target}" = "y" ]; then
	    subdir=${TARGET_ARCH}
    else
	    subdir=${HOST_ARCH}
    fi

    if [ ! -d ${PREBUILT_PKGDIR} ]; then
	    return
    fi
    # Remove m4 frozen file
    find ${PREBUILT_PKGDIR} -name "*.m4f" |xargs -I {} rm {}

    if [ "${PKG_NAME}" = "host-tar" ]; then
	    # host-tar is required to use cpio, otherwise no other tar program to extract it
	    cd ${PREBUILT_PKGDIR}
	    find ${PKG_BASENAME} | cpio --quiet -oH newc > ${PKG_BASENAME}.cpio
	    # cd -
	    mv ${PREBUILT_PKGDIR2}.cpio ${PREBUILT_DIR}/${subdir}/${PKG_BASENAME}.cpio
	    cd ${PREBUILT_DIR}/${subdir} && echo In $PWD: && ls -og --time-style=iso ${PKG_BASENAME}.cpio
    else
	    tar -C ${PREBUILT_PKGDIR} -czf ${PREBUILT_PKGDIR2}.tar.gz ${PKG_BASENAME}
	    mkdir -p ${PREBUILT_DIR}/${subdir}/
	    mv ${PREBUILT_PKGDIR2}.tar.gz ${PREBUILT_DIR}/${subdir}/${PKG_BASENAME}.tar.gz
	    cd ${PREBUILT_DIR}/${subdir} && echo In $PWD: && ls -og --time-style=iso ${PKG_BASENAME}.tar.gz
    fi
    rm -rf ${PREBUILT_PKGDIR}
}

main
