cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(test-spi LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall -Werror)

add_executable(test_spi test_spi.c)
add_executable(spidev_fdx spidev_fdx.c)
add_executable(spidev_test spidev_test.c)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS test_spi RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
	install(TARGETS spidev_fdx RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
	install(TARGETS spidev_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
