################################################################################
#
# V4L utils
#
################################################################################
V4L_UTILS_VERSION = 1.28.1
V4L_UTILS_SOURCE = v4l-tuils-$(V4L_UTILS_VERSION).tar.xz
V4L_UTILS_SITE = http://git.linuxtv.org/v4l-utils.git
V4L_UTILS_LICENSE = LGPL-2.0+
V4L_UTILS_LICENSE_FILES = COPYING COPYING.libdvbv5 COPYING.libv4l
V4L_UTILS_INSTALL_STAGING = YES

V4L_UTILS_LDFLAGS = $(TARGET_LDFLAGS) $(TARGET_NLS_LIBS)

$(eval $(meson-package))
