cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(aic-mpp LANGUAGES C)

# include directory
include_directories(include)
include_directories(base/include)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

# ${ARCH} pass from package/artinchip/aic-mpp/aic-mpp.mk
if ( ${ARCH} STREQUAL "RISCV")
	add_definitions(-DARCH_RISCV)
endif()

# Add compile option
add_compile_options(-fPIC)

# Debug -Wall -w
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -DDEBUG -Wall")

# Release
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")

if (LINUX_VERSION_6)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLINUX_VERSION_6")
endif()

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

# sub base
add_subdirectory(base)

# sub ve
add_subdirectory(ve)

# sub ge
add_subdirectory(ge)

# sub mpp_test
add_subdirectory(mpp_test)

# sub middle_media
if (MIDDLEWARE)
	add_subdirectory(middle_media)
endif()

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(FILES include/mpp_dec_type.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
	install(FILES include/mpp_decoder.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
	install(FILES include/mpp_ge.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
	install(FILES include/dma_allocator.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
	install(FILES include/frame_allocator.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
	install(FILES middle_media/player/include/aic_player.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
endif()
