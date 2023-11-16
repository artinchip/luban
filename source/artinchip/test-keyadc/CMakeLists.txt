cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(test-keyadc LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

add_executable(keyadc_test keyadc_test.c)
add_definitions(-D_GNU_SOURCE)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS keyadc_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
