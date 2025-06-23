cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(aicp-dec LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC} ${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
        ${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS} ${ARCH})

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(FILES libmpp_aicp_dec_${GNU_TARGET_NAME}.so DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" RENAME libmpp_aicp_dec.so)
endif() # CMAKE_INSTALL_FULL_LIBDIR
