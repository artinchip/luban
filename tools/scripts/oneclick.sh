#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2022 ArtInChip Technology Co., Ltd
# Matteo <duanmt@artinchip.com>

# $1 - 'quiet', run the install with no question

MY_NAME=$0
TOPDIR=$PWD
SDK_NAME=Luban

TRUE=1
FALSE=0

# some configuration of environment
INTERNET_IS_AVAILABLE=$FALSE
GCC_NAME=gcc-6.4.0
QUIET_MODE=$FALSE

# Return value
ERR_CANCEL=100
ERR_UNSURPPORTED=110
ERR_PKG_UNVAILABLE=111
ERR_NET_UNVAILABLE=112

COLOR_BEGIN="\033["
COLOR_RED="${COLOR_BEGIN}41;37m"
COLOR_YELLOW="${COLOR_BEGIN}43;30m"
COLOR_WHITE="${COLOR_BEGIN}47;30m"
COLOR_END="\033[0m"

pr_err()
{
	echo -e "${COLOR_RED}*** $*${COLOR_END}"
}

pr_warn()
{
	echo -e "${COLOR_YELLOW}!!! $*${COLOR_END}"
}

pr_info()
{
	echo -e "${COLOR_WHITE}>>> $*${COLOR_END}"
}

run_cmd()
{
	echo
	pr_info $1
	echo
	eval $1 || exit 120
}

input_an_answer()
{
	if [ $QUIET_MODE -eq $TRUE ]; then
		echo Y
		ANSWER="Y"
	else
		read ANSWER
	fi
}

check_root()
{
	CUR_USER=`whoami`
	if [ $CUR_USER = "root" ]; then
		pr_info "Current user is already root"
		return
	fi
	sudo -l -U `whoami` | grep ALL
	if [ $? -eq 0 ]; then
		pr_info "Sudo is available"
		return
	fi

	pr_warn $MY_NAME "must install package with 'sudo'. "
	pr_warn "Your passward will be safe and always used locally."
}

check_os()
{
	if [ -f /etc/lsb-release ]; then
		OS_VER=`cat /etc/lsb-release | grep RELEASE | awk -F '=' '{print $2}'`
		OS_TYPE="Ubuntu"
	elif [ -f /etc/redhat-release ]; then
		cat /etc/redhat-release 2>&1 | grep CentOS
		if [ $? -eq 0 ]; then
			OS_VER=`cat /etc/redhat-release | awk -F 'release ' '{print $2}' | awk '{print $1}'`
			OS_TYPE="CentOS"
		else
			OS_VER=`cat /etc/redhat-release | awk -F 'release ' '{print $2}' | awk '{print $1}'`
			OS_TYPE=`cat /etc/redhat-release | awk -F 'release ' '{print $1}'`
		fi
	else
		pr_err "Unknow system OS"
		exit $ERR_UNSURPPORTED
	fi
	pr_info "Current system is $OS_TYPE-$OS_VER"
}

check_work_path()
{
	# Assumption:
	# 1. The current path is the root folder of Luban
	# 2. The current path is Luban/tools/scripts/
	if [ -d source ]; then
		GCC_PKG_DIR=$TOPDIR/dl/gcc/
	else
		GCC_PKG_DIR=$TOPDIR/../../dl/gcc
	fi
}

# $1 - the tool name
pkg_is_too_old()
{
	pr_warn "Please install a newer $1 manually, then try $MY_NAME again"
}

pkg_is_ok()
{
	printf "\t\t\t\t\t\t\t\t[OK]\n"
}

# $1 - the package name
pkg_is_failed()
{
	pr_warn "Failed to install $1, please check the install log."
	INSTALL_RESULT=$FALSE
}

# $1: Lib directory
compile_install_gcc()
{
	GCC_TAR=$GCC_NAME".tar.xz"
	LIB_DIR=$1

	cd $GCC_PKG_DIR

	if [ ! -f $GCC_TAR ] && [ $INTERNET_IS_AVAILABLE -eq $TRUE ]; then
		wget ftp://ftp.gnu.org/gnu/gcc/$GCC_NAME/$GCC_TAR || \
				exit $ERR_PKG_UNVAILABLE
	fi
	if [ ! -f $GCC_TAR ]; then
		pr_err "The $GCC_TAR is unavailable!"
		exit $ERR_PKG_UNVAILABLE
	fi

	run_cmd "tar xJf $GCC_TAR"
	run_cmd "cd $GCC_NAME"

	if [ $INTERNET_IS_AVAILABLE -eq $TRUE ]; then
		run_cmd "./contrib/download_prerequisites"
	else
		MPFR=mpfr-2.4.2
		tar xjf ../../mpfr/$MPFR.tar.bz2 || exit $ERR_PKG_UNVAILABLE
		ln -sf $MPFR mpfr || exit $ERR_PKG_UNVAILABLE

		GMP=gmp-4.3.2
		tar xjf ../../gmp/$GMP.tar.bz2 || exit $ERR_PKG_UNVAILABLE
		ln -sf $GMP gmp || exit $ERR_PKG_UNVAILABLE

		MPC=mpc-0.8.1
		tar xzf ../../mpc/$MPC.tar.gz || exit $ERR_PKG_UNVAILABLE
		ln -sf $MPC mpc || exit $ERR_PKG_UNVAILABLE

		ISL=isl-0.15
		tar xjf ../../isl/$ISL.tar.bz2 || exit $ERR_PKG_UNVAILABLE
		# Fix trailing comma which errors with -pedantic for host GCC <= 4.3
		sed -e 's/isl_stat_ok = 0,/isl_stat_ok = 0/' isl-0.15/include/isl/ctx.h\
			> isl-0.15/include/isl/ctx.h.tem && \
			mv isl-0.15/include/isl/ctx.h.tem isl-0.15/include/isl/ctx.h
		ln -sf $ISL isl || exit $ERR_PKG_UNVAILABLE
	fi

	run_cmd "mkdir build -p && cd build"
	run_cmd "../configure --prefix=/usr/local/$GCC_NAME \
			--host=x86_64-redhat-linux --build=x86_64-redhat-linux \
			--enable-checking=release --enable-bootstrap \
			--enable-language=c,c++,objc,obj-c++ --disable-multilib \
			--enable-gather-detailed-mem-stats --with-system-zlib \
			--with-tune=generic --with-long-double-128"
	run_cmd "make -s -j4"
	run_cmd "make install"

	LIB_STDC=libstdc++.so.6
	CUR_STDC=`find $LIB_DIR -name $LIB_STDC | head -1`
	rm -rf $CUR_STDC
	ln -s /usr/local/$GCC_NAME/lib64/libstdc++.so.6 $CUR_STDC

	mv /usr/bin/gcc /usr/bin/gcc.old
	ln -s /usr/local/$GCC_NAME/bin/gcc /usr/bin/gcc
	if [ -f /usr/bin/x86_64-linux-gnu-gcc ]; then
		ln -sf /usr/bin/gcc /usr/bin/x86_64-linux-gnu-gcc
	fi

	cd $GCC_PKG_DIR
	rm $GCC_NAME -rf
}

# The version of GCC must >= 6.4
# $1: install callback
# $2: Lib directory
check_gcc_ver()
{
	INSTALL_CALLBACK=$1
	LIB_DIR=$2

	GCC_VER=`gcc -v 2>&1 | grep "gcc version" -w | awk '{print $3}'`
	MAIN_VER=`echo $GCC_VER | awk -F '.' '{print $1}'`
	MIN_VER=`echo $GCC_VER | awk -F '.' '{print $2}'`

	if [ $MAIN_VER -gt 6 ]; then
		return $TRUE
	fi

	if [ $MAIN_VER -eq 6 ] && [ $MIN_VER -gt 3 ]; then
		return $TRUE
	fi

	echo
	pr_warn "GCC-$GCC_VER is too old for $SDK_NAME, then try to Compile-Install $GCC_NAME"
	pr_warn "This may take more than an hour, continue? Y/N"

	input_an_answer
	if [ $ANSWER = "Y" ] || [ $ANSWER = "y" ]; then
		$INSTALL_CALLBACK wget
		if [ "$OS_TYPE" = "Ubuntu" ]; then
			$INSTALL_CALLBACK zip
			$INSTALL_CALLBACK zlib1g-dev
		else
			$INSTALL_CALLBACK zlib-devel nocmd
		fi
		compile_install_gcc $LIB_DIR
	else
		pkg_is_too_old "GCC-"$GCC_VER
		exit $ERR_PKG_UNVAILABLE
	fi
}

compile_install_make4()
{
	MAKE4_PKG=make-4.2.1
	MAKE4_PKG_TAR=$MAKE4_PKG".tar.bz2"

	if [ ! -f $TOPDIR/dl/make/$MAKE4_PKG_TAR ]; then
		pr_warn $MAKE4_PKG_TAR does not exist!
		exit $ERR_PKG_UNVAILABLE
	fi
	cd $TOPDIR/dl/make

	run_cmd "tar xjf $MAKE4_PKG_TAR"
	cd $MAKE4_PKG
	run_cmd "./configure --prefix=/usr/"
	run_cmd "make && make install"

	cd - > /dev/null
}

# The version of GLIBCXX must >= 3.4.22
# $1: Lib directory
check_libstdc_ver()
{
	LIB_DIR=$1

	pr_info "Check libstdc++ version"
	LIBCXX=`find $LIB_DIR -name libstdc++.so.6 | head -1`
	strings $LIBCXX | grep ^GLIBCXX_3.4.22
	if [ $? -eq 0 ]; then
		pkg_is_ok
		return
	fi

	echo
	CUR_VER=`strings $LIBCXX | grep ^GLIBCXX_ | grep "\." | tail -1`
	pr_err "The GLIBCXX version must >= 3.4.22. Current: $CUR_VER"
	pkg_is_too_old "GLIBCXX-"$CUR_VER
	exit $ERR_PKG_UNVAILABLE
}

# $1 - the command string
check_pkg_src()
{
	pr_info "Try to access the package source ..."
	$1
	if [ $? -ne 0 ]; then
		pr_err "The software source is not accessable! Please check it"
		pr_err "$MY_NAME must download package from a software source."
		exit $ERR_NET_UNVAILABLE
	fi
}

# $1 - package name
# $2 - need user confirmed
apt_install_pkg()
{
	PKG=$1
	CONFIRM=$2

	echo 
	pr_info "Check $PKG ..."
	dpkg -s $PKG 2>&1 | grep -E "Status|Version"
	if [ $? -eq 0 ]; then
		pkg_is_ok
		return 0
	fi

	if [ "x$CONFIRM" = "x" ]; then
		ANSWER="Y"
	else
		pr_warn "Will download and install $PKG, continue? Y/N"
		input_an_answer
	fi

	if [ $ANSWER = "Y" ] || [ $ANSWER = "y" ]; then
		pr_info "Try to install $PKG ..."
		apt-get install -y $PKG
		RET=$?
		if [ $RET -ne 0 ]; then
			pkg_is_failed $PKG
		fi
		return $RET
	else
		exit $ERR_CANCEL
	fi
}

# $1 - package name
yum_is_installed()
{
	PKG=$1

	yum info $PKG | grep ^Installed
	if [ $? -eq 0 ]; then
		echo Installed
		return $TRUE
	fi

	return $FALSE
}

# $1 - package name
# $2 - the relative command name
# $3 - need user confirmed
yum_install_pkg()
{
	PKG=$1
	if [ -z $2 ]; then
		CMD=$1
	else
		CMD=$2
	fi
	CONFIRM=$3

	echo 
	pr_info "Check $PKG ..."
	if [ "$CMD" = "nocmd" ]; then
		yum_is_installed $PKG
	else
		$CMD --version 2>&1 | grep "command not found"
	fi
	if [ $? -eq $TRUE ]; then
		pkg_is_ok
		return 0
	fi

	if [ "x$CONFIRM" = "x" ]; then
		ANSWER="Y"
	else
		pr_warn "Will download and install $PKG, continue? Y/N"
		input_an_answer
	fi

	if [ $ANSWER = "Y" ] || [ $ANSWER = "y" ]; then
		pr_info "Try to install $PKG ..."
		yum install -y $PKG
		RET=$?
		if [ $RET -ne 0 ]; then
			pkg_is_failed $PKG
		fi
		return $RET
	else
		exit $ERR_CANCEL
	fi
}

ubuntu_install()
{
	check_pkg_src "apt-get update"

	NEED_CONFIRM=("build-essential" "gcc")
	for i in ${NEED_CONFIRM[@]}
	do
		apt_install_pkg $i ask
	done

	PKGS=("rsync" "bc" "cpio" "file" "patch" "bzip2" "bison" "flex" "libncurses-dev")
	for i in ${PKGS[@]}
	do
		apt_install_pkg $i
	done

	# Maybe depends on the other command, 'make' etc
	check_gcc_ver "apt_install_pkg" /usr/lib
	check_libstdc_ver /usr/lib/x86_64-linux-gnu

	if [ "$OS_VER" = "14.04" ]; then
		compile_install_make4
	fi
}

redhat_install()
{
	yum makecache
	check_pkg_src "yum search ctags"

	yum_install_pkg gcc gcc ask
	yum_install_pkg gcc-c++ g++ ask

	# The name of package is same as the command
	PKGS=("make" "rsync" "bc" "file" "which" "perl" "patch" "zip" "bison" "autoconf" "flex" "ncurses-devel")
	for i in ${PKGS[@]}
	do
		yum_install_pkg $i
	done

	yum_install_pkg diffutils cmp
	yum_install_pkg perl-Thread-Queue nocmd
	yum_install_pkg bzip2 nocmd

	# Maybe depends on the other command, 'make' etc
	check_gcc_ver "yum_install_pkg" /usr/lib64
	check_libstdc_ver /usr/lib64
}

check_root
check_os
check_work_path

if [ "x$1" = "xquiet" ]; then
	QUIET_MODE=$TRUE
fi

INSTALL_RESULT=$TRUE
if [ $OS_TYPE = "Ubuntu" ]; then
	ubuntu_install
else
	redhat_install
fi

echo
if [ $INSTALL_RESULT -ne $FALSE ]; then
	pr_info "Congratulations! All the package is ready."
	pr_info "Enjoy the "$SDK_NAME"OS!"
	exit 0
else
	pr_warn "The install process is not complete. Please check it!"
	exit $ERR_PKG_UNVAILABLE
fi
