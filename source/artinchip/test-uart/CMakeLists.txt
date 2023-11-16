cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(test-uart LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

add_executable(aic_uart testuart.c)

target_link_libraries(aic_uart pthread)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS aic_uart RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
