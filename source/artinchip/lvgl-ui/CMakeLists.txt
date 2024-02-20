cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# project name
project(lvgl-ui)

# set app folder
set(APP_FOLDER base_ui)
#set(APP_FOLDER qc_test) # qc-test is only used for AIC chip testing
#set(APP_FOLDER aic_demo)

# set app out put name
set(APP_OUT_NAME test_lvgl)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC} ${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
    ${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

# Add compile option
add_compile_options(-Wall)

# check install directories
if(NOT CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

### build lvgl library
add_subdirectory(lvgl)

# lvgl HAL source
set(LVGL_HAL ${CMAKE_CURRENT_SOURCE_DIR}/lv_drivers)
set(DRIVER_SOURCE_FILES
    ${LVGL_HAL}/aic_dec.c
    ${LVGL_HAL}/lv_fbdev.c
    ${LVGL_HAL}/lv_ge2d.c
    ${LVGL_HAL}/lv_port_indev.c
    ${LVGL_HAL}/lv_port_disp.c
)

# obtain app source file
if (NOT APP_FOLDER STREQUAL qc_test)
    file(GLOB_RECURSE APP_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/${APP_FOLDER}/*.c)

    file(GLOB APP_SOURCE_FILES_MAIN
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

    string(APPEND ${APP_SOURCE_FILES} ${APP_SOURCE_FILES_MAIN})
endif()

# obtain qc_test source file  --> only used for AIC chip testing, please ignore
if (APP_FOLDER STREQUAL qc_test)
    file(GLOB APP_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/${APP_FOLDER}/*.c)
endif()

set(APP_SOURCE_FILES ${APP_SOURCE_FILES} ${APP_SOURCE_FILES_MAIN} ${DRIVER_SOURCE_FILES})

# set include path
set(INCLUDE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/${APP_FOLDER}
    ${LVGL_HAL}
)

add_executable(${APP_OUT_NAME} ${APP_SOURCE_FILES})
target_link_libraries(${APP_OUT_NAME} lvgl)
target_link_libraries(${APP_OUT_NAME} lvgl_examples)
target_link_libraries(${APP_OUT_NAME} lvgl_demos)
target_link_libraries(${APP_OUT_NAME} media_player)
target_link_libraries(${APP_OUT_NAME} m)
target_link_libraries(${APP_OUT_NAME} mpp_base mpp_ve mpp_ge mpp_decoder)

if (APP_FOLDER STREQUAL qc_test OR APP_FOLDER STREQUAL aic_demo)
    target_link_libraries(${APP_OUT_NAME} pthread)

    # using screen mirroring
    if (APP_FOLDER STREQUAL aic_demo)
        #set(use_mirror "use_screen_mirror")
        add_definitions(-DLV_USE_AIC_DEMO=1)
    endif()

    if(use_mirror STREQUAL "use_screen_mirror")
        target_link_libraries(${APP_OUT_NAME} LinkSupport LinkUtils MSNUtils mpp_decoder asound)
        add_definitions(-DUSE_MSNLINK)
    endif()
endif()

# use freetype font
if(DEFINED USE_FREETYPE)
    if(${USE_FREETYPE} STREQUAL "yes")
        target_include_directories(lvgl SYSTEM PUBLIC ${STAGING_DIR}/usr/include/freetype2/)
        target_link_libraries(${APP_OUT_NAME} libfreetype.so)
    endif()
endif()

# Find libts for link
if(${USE_RTP_TSLIB} STREQUAL "yes")
    add_definitions(-DUSE_RTP_TSLIB)
    find_library(LIBTS libts.so)
    target_link_libraries(${APP_OUT_NAME} ${LIBTS})
endif()

# add include path
target_include_directories(${APP_OUT_NAME} PRIVATE ${INCLUDE_PATH})

set_target_properties(${APP_OUT_NAME} PROPERTIES INSTALL_RPATH "/usr/local/lib")

add_definitions(-DLVGL_DIR="L:${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data/")

if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
    install(TARGETS ${APP_OUT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_BINDIR

if(DEFINED CMAKE_INSTALL_FULL_DATAROOTDIR)
    install(DIRECTORY ${APP_FOLDER}/assets/
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data)
endif() # CMAKE_INSTALL_FULL_DATAROOTDIR
