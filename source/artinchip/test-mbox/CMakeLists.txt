cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(test-mbox LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall -Werror)

add_executable(test_mbox test_mbox.c)

target_link_libraries(test_mbox pthread)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS test_mbox RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
