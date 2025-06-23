cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(p2p_auto LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall -Werror)

add_executable(p2p_auto p2p_auto.c)

target_link_libraries(p2p_auto wpa_client)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

install(FILES files/p2p_supplicant.conf DESTINATION ${TARGET_DIR}/etc/wifi)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS p2p_auto RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
