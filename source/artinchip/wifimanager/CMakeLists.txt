cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(wifimanager LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

#add_compile_options(-Wall -Werror)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

install(FILES files/wpa_supplicant.conf DESTINATION ${TARGET_DIR}/etc/wifi)
#install(FILES files/wifimanager.init DESTINATION ${TARGET_DIR}/etc/init.d RENAME S99wifimanager
#        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_EXECUTE)
install(FILES inc/wifimanager.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

# libwifimanager.so
add_subdirectory(src)

# demo of wifimanager
add_subdirectory(demo)
