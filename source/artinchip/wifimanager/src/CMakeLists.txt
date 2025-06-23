cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

aux_source_directory(./lib LIB_SRCS)

include_directories(include)

add_library(wifimanager SHARED ${LIB_SRCS})
target_link_libraries(wifimanager pthread)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

install(TARGETS wifimanager DESTINATION ${CMAKE_INSTALL_LIBDIR})
