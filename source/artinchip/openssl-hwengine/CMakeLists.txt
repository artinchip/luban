cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(openssl-hwengine LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

aux_source_directory(src SRC_AIC)
aux_source_directory(huk SRC_HUK)

# Find libkcapi for link
find_library(LIBKCAPI_PATH libkcapi.so)

add_library(engine_aic SHARED ${SRC_AIC})
add_library(engine_huk SHARED ${SRC_HUK})
target_link_libraries(engine_aic ${LIBKCAPI_PATH})
target_link_libraries(engine_huk ${LIBKCAPI_PATH})

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	set(CONF_FILE conf/openssl_aic.cnf)
	install(FILES ${CONF_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/../etc/ssl")
	install(TARGETS engine_aic LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
	install(TARGETS engine_huk LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
