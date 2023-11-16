################################################################################
#
# python3-fdt
#
################################################################################

PYTHON3_FDT_VERSION = 0.3.3
PYTHON3_FDT_SOURCE = fdt-$(PYTHON3_FDT_VERSION).tar.gz
PYTHON3_FDT_SITE =https://files.pythonhosted.org/packages/0b/69/ab0f63a898c7b8c9d350416222a20dffc8f9b8798830b3c0a2cf155e6bac
PYTHON3_FDT_SETUP_TYPE = setuptools
HOST_PYTHON3_FDT_DL_SUBDIR = python-fdt
HOST_PYTHON3_FDT_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
