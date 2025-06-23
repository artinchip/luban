cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inc)

add_executable(wifi main.c)
target_link_libraries(wifi wifimanager wpa_client)

add_compile_options(-w)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

install(TARGETS wifi DESTINATION ${CMAKE_INSTALL_BINDIR})
