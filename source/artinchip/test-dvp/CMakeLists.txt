cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(test-dvp LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

add_executable(test_dvp test_dvp.c)

if(DEFINED USE_GE_ROTATION)
	if(USE_GE_ROTATION STREQUAL "yes")
		add_definitions(-DSUPPORT_ROTATION)
		target_link_libraries(test_dvp mpp_ge)
	endif()
endif()

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS test_dvp RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
