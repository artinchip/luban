cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(test-ce LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

aux_source_directory(kcapi SRC_KCAPI)
add_executable(crypto_kcapi ${SRC_KCAPI})

# Find libkcapi for link
find_library(LIBKCAPI_PATH libkcapi.so)
target_link_libraries(crypto_kcapi ${LIBKCAPI_PATH})

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
	install(TARGETS crypto_kcapi RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
	# Create directory first
	# Install test scripts and data
endif() # CMAKE_INSTALL_FULL_BINDIR
