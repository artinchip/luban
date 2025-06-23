cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# project name
project(lvgl-ui)

# set demo folder
set(DEMO_FOLDER aic_demo)

# set app folder
if(DEFINED LVGL_FBDEV)
    if(${LVGL_FBDEV} STREQUAL "yes")
        set(APP_FOLDER ${DEMO_FOLDER}/base_demo)
        #set(APP_FOLDER ${DEMO_FOLDER}/hub_demo)
    endif()
endif()

if(DEFINED LVGL_DRM)
    if(${LVGL_DRM} STREQUAL "yes")
        set(APP_FOLDER ${DEMO_FOLDER}/drm_base)
    endif()
endif()

# set LVGL lib path
set(LVGL_LIB_PATH lvgl_v8)

if(DEFINED LVGL_V_8)
    if(${LVGL_V_8} STREQUAL "yes")
        set(LVGL_LIB_PATH lvgl_v8)
    endif()
endif()

if(DEFINED LVGL_V_9)
    if(${LVGL_V_9} STREQUAL "yes")
        set(LVGL_LIB_PATH lvgl_v9)
    endif()
endif()

# set app out put name
set(APP_OUT_NAME test_lvgl)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC} ${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
    ${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

# Add compile option
add_compile_options(-Wall)

add_definitions(-DLV_USE_CONF_CUSTOM)

# set custom config path, used by lvgl library
set(LV_CONF_CUSTOM_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${APP_FOLDER})

# check install directories
if(NOT CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

### build lvgl library
add_subdirectory(${LVGL_LIB_PATH}/lvgl)

# lvgl HAL source
set(LVGL_DRIVER_PATH fbdev)

if(DEFINED LVGL_FBDEV)
    if((${LVGL_FBDEV} STREQUAL "yes") AND (${LVGL_V_9} STREQUAL "yes"))
        set(LVGL_DRIVER_PATH fbdev)
    else()
        set(LVGL_DRIVER_PATH )
    endif()
endif()

if(DEFINED LVGL_DRM)
    if(${LVGL_DRM} STREQUAL "yes")
        set(LVGL_DRIVER_PATH drm)
    endif()
endif()

set(LVGL_HAL ${CMAKE_CURRENT_SOURCE_DIR}/${LVGL_LIB_PATH}/lv_drivers/${LVGL_DRIVER_PATH})

file(GLOB_RECURSE DRIVER_SOURCE_FILES
    ${LVGL_HAL}/*.c)

# obtain app source file
file(GLOB_RECURSE APP_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/${APP_FOLDER}/*.c)

if  (NOT APP_FOLDER STREQUAL aic_demo/lv_perf)
    file(GLOB APP_SOURCE_FILES_MAIN
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
    list(APPEND APP_SOURCE_FILES ${APP_SOURCE_FILES_MAIN})
endif()

set(APP_SOURCE_FILES ${APP_SOURCE_FILES} ${APP_SOURCE_FILES_MAIN} ${DRIVER_SOURCE_FILES})

# set include path
set(INCLUDE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/${APP_FOLDER}
    ${CMAKE_CURRENT_SOURCE_DIR}/${LVGL_LIB_PATH}
    ${LVGL_HAL}
)

add_executable(${APP_OUT_NAME} ${APP_SOURCE_FILES})
target_link_libraries(${APP_OUT_NAME} lvgl)
target_link_libraries(${APP_OUT_NAME} lvgl_examples)
target_link_libraries(${APP_OUT_NAME} lvgl_demos)

if(DEFINED LVGL_FBDEV)
    if(${LVGL_FBDEV} STREQUAL "yes")
        target_link_libraries(${APP_OUT_NAME} media_player)
        target_link_libraries(${APP_OUT_NAME} m)
        target_link_libraries(${APP_OUT_NAME} mpp_base mpp_ve mpp_ge mpp_decoder)
    endif()
endif()

target_link_libraries(${APP_OUT_NAME} pthread)

if(DEFINED LVGL_DRM)
    if(${LVGL_DRM} STREQUAL "yes")
        find_library(LIBDRM libdrm.so)
        find_path(VAR xf86drm.h)

        target_link_libraries(${APP_OUT_NAME} ${LIBDRM})
        target_include_directories(${APP_OUT_NAME} PUBLIC "${VAR}/drm/")
    endif()
endif()

if (APP_FOLDER STREQUAL aic_demo/qc_test OR APP_FOLDER STREQUAL aic_demo/hub_demo)
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
if(DEFINED USE_RTP_TSLIB)
    if(${USE_RTP_TSLIB} STREQUAL "yes")
        add_definitions(-DUSE_RTP_TSLIB)
        target_link_libraries(${APP_OUT_NAME} libts.so)
    endif()
endif()

# Framebuffer cacheable
if(DEFINED USE_CACHE_FRAMEBUFFER)
    if(${USE_CACHE_FRAMEBUFFER} STREQUAL "yes")
        add_definitions(-DUSE_CACHE_FRAMEBUFFER)
    endif()
endif()

# rotation
if(DEFINED LV_DISPLAY_ROTATE_EN)
    if(${LV_DISPLAY_ROTATE_EN} STREQUAL "yes")
        add_definitions(-DLV_DISPLAY_ROTATE_EN=1)
    endif()
endif()

if(DEFINED LV_ROTATE_DEGREE_0)
    if(${LV_ROTATE_DEGREE_0} STREQUAL "yes")
        add_definitions(-DLV_ROTATE_DEGREE=0)
    endif()
endif()

if(DEFINED LV_ROTATE_DEGREE_90)
    if(${LV_ROTATE_DEGREE_90} STREQUAL "yes")
        add_definitions(-DLV_ROTATE_DEGREE=90)
    endif()
endif()

if(DEFINED LV_ROTATE_DEGREE_180)
    if(${LV_ROTATE_DEGREE_180} STREQUAL "yes")
        add_definitions(-DLV_ROTATE_DEGREE=180)
    endif()
endif()

if(DEFINED LV_ROTATE_DEGREE_270)
    if(${LV_ROTATE_DEGREE_270} STREQUAL "yes")
        add_definitions(-DLV_ROTATE_DEGREE=270)
    endif()
endif()

# add include path
target_include_directories(${APP_OUT_NAME} PRIVATE ${INCLUDE_PATH})

set_target_properties(${APP_OUT_NAME} PROPERTIES INSTALL_RPATH "/usr/local/lib")

add_definitions(-DLVGL_DIR="L:${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data/")

if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
    install(TARGETS ${APP_OUT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_BINDIR

if(DEFINED CMAKE_INSTALL_FULL_DATAROOTDIR)
    if (NOT APP_FOLDER STREQUAL aic_demo/lv_perf)
        install(DIRECTORY ${APP_FOLDER}/assets/
            DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data)
    endif()
endif() # CMAKE_INSTALL_FULL_DATAROOTDIR
