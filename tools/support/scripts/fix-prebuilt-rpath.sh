#!/bin/bash

PKG_NAME="${1}"
PKG_BASENAME="${2}"
PKG_BUILDDIR="${3}"

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

fix_tree()
{
    local treedir="${1}"
    local base_str="ARTINCHIP_PREBUILT_BASE"
    local staging_base="ARTINCHIP_PREBUILT_STAGING_BASE"

    while read filepath ; do
        # check if it's an ELF file
        if [ -L "${filepath}" ]; then
            linkpath=$(readlink "${filepath}")
            changed_lpath=$(echo ${linkpath} | sed "s@${base_str}@${BASE_DIR}@g")
            if test "${linkpath}" != "${changed_lpath}" ; then
                ln -snf "${changed_lpath}" "${filepath}"
            fi
        fi

	is_text_file "${filepath}"
        if [ $? -eq 1 ]; then
            # make files writable if necessary
            changed=$(chmod -c u+w "${filepath}")

            sed -i "s@${base_str}@${BASE_DIR}@g" "${filepath}"
            sed -i "s@${staging_base}@${STAGING_SUBDIR}@g" "${filepath}"

            # restore the original permission
            test "${changed}" != "" && chmod u-w "${filepath}"
            continue
	fi
	is_elf_file "${filepath}"
        if [ $? -eq 1 ]; then
            rpath=$(${PATCHELF} --print-rpath "${filepath}" 2>&1)
            if test $? -ne 0 ; then
                continue
            fi

            # make files writable if necessary
            changed=$(chmod -c u+w "${filepath}")

            changed_rpath=$(echo ${rpath} | sed "s@${base_str}@${BASE_DIR}@g")
            if test "${rpath}" != "${changed_rpath}" ; then
                ${PATCHELF} --set-rpath ${changed_rpath} "${filepath}"
            fi

            # restore the original permission
            test "${changed}" != "" && chmod u-w "${filepath}"
            continue
        fi
    done < <(find "${treedir}" \( -type f -o -type l \) )
}

fix_tree_quick()
{
    local treedir="${1}"
    local base_str="ARTINCHIP_PREBUILT_BASE"
    local staging_base="ARTINCHIP_PREBUILT_STAGING_BASE"

    while read -r line
    do
	filepath=${PREBUILT_PKGDIR2}/${line}

        # check if it's an ELF file
        if [ -L "${filepath}" ]; then
            linkpath=$(readlink "${filepath}")
            changed_lpath=$(echo ${linkpath} | sed "s@${base_str}@${BASE_DIR}@g")
            if test "${linkpath}" != "${changed_lpath}" ; then
                ln -snf "${changed_lpath}" "${filepath}"
            fi
        fi

	is_text_file "${filepath}"
        if [ $? -eq 1 ]; then
            # make files writable if necessary
            changed=$(chmod -c u+w "${filepath}")

            sed -i "s@${base_str}@${BASE_DIR}@g" "${filepath}"
            sed -i "s@${staging_base}@${STAGING_SUBDIR}@g" "${filepath}"

            # restore the original permission
            test "${changed}" != "" && chmod u-w "${filepath}"
            continue
	fi
	is_elf_file "${filepath}"
        if [ $? -eq 1 ]; then
            rpath=$(${PATCHELF} --print-rpath "${filepath}" 2>&1)
            if test $? -ne 0 ; then
                continue
            fi

            # make files writable if necessary
            changed=$(chmod -c u+w "${filepath}")

            changed_rpath=$(echo ${rpath} | sed "s@${base_str}@${BASE_DIR}@g")
            if test "${rpath}" != "${changed_rpath}" ; then
                ${PATCHELF} --set-rpath ${changed_rpath} "${filepath}"
            fi

            # restore the original permission
            test "${changed}" != "" && chmod u-w "${filepath}"
            continue
        fi
    done < $1
}

main()
{
    if [ ! -d ${PREBUILT_PKGDIR} ]; then
	return 0
    fi

    if [ -f ${PREBUILT_PKG_RESOLVE} ]; then
        fix_tree_quick ${PREBUILT_PKG_RESOLVE}
	return 0
    fi
    if [ -d ${PREBUILT_HOST_DIR} ]; then
        fix_tree ${PREBUILT_HOST_DIR}
    fi
    if [ -d ${PREBUILT_STAGING_DIR} ]; then
        fix_tree ${PREBUILT_STAGING_DIR}
    fi
    if [ -d ${PREBUILT_TARGET_DIR} ]; then
        fix_tree ${PREBUILT_TARGET_DIR}
    fi
}

main
