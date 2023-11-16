cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# project name
project(awtk-ui)
set(PROJ awtk)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}    ${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
    ${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

# Add compile option
add_compile_options(-fPIC)
add_compile_options(-Wall)
set(NANOVG_BACKEND "AGGE")

# set awtk path
set(TK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/awtk)

# set google pinyin
set(INPUT_ENGINE "pinyin")
# set(INPUT_ENGINE "null")

### build config
add_definitions(-DLINUX)
add_definitions(-DHAS_STD_MALLOC)
add_definitions(-DHAS_STDIO)
add_definitions(-DHAS_FAST_MEMCPY)
add_definitions(-DWITH_VGCANVAS)
add_definitions(-DWITH_UNICODE_BREAK)
add_definitions(-DLOAD_ASSET_WITH_MMAP=1)
add_definitions(-DWITH_SOCKET=1)
add_definitions(-DWITH_ASSET_LOADER)
add_definitions(-DWITH_FS_RES)
add_definitions(-DHAS_GET_TIME_US64=1)
add_definitions(-DSTBTT_STATIC)
add_definitions(-DSTB_IMAGE_STATIC)
add_definitions(-DWITH_STB_IMAGE)
add_definitions(-DWITH_STB_FONT)
add_definitions(-DWITH_TEXT_BIDI=1)
add_definitions(-DAPP_TYPE=APP_MOBILE)
add_definitions(-DLINUX_FB)
add_definitions(-DWITH_NANOVG_AGGE)
add_definitions(-DWITH_LINUX_FB)
add_definitions(-DWITH_FAST_LCD_PORTRAIT)
add_definitions(-DHAS_PTHREAD)
add_definitions(-DWITH_DATA_READER_WRITER)
add_definitions(-DWITH_G2D)
add_definitions(-DWITH_AIC_G2D)

if (INPUT_ENGINE STREQUAL "pinyin")
    add_definitions(-DWITH_IME_PINYIN)
else()
    add_definitions(-DWITH_IME_NULL)
endif()

### all source file
set(SOURCE_FILES "")

# add linux fb root
set(TK_LINUX_FB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/awtk-linux-fb)
set(SOURCE_FILES ${SOURCE_FILES}
    ${TK_LINUX_FB_ROOT}/awtk-port/input_thread/input_thread.c
    ${TK_LINUX_FB_ROOT}/awtk-port/input_thread/input_dispatcher.c
    ${TK_LINUX_FB_ROOT}/awtk-port/input_thread/mouse_thread.c
    ${TK_LINUX_FB_ROOT}/awtk-port/lcd_linux/lcd_linux_fb.c
    ${TK_LINUX_FB_ROOT}/awtk-port/lcd_linux/lcd_mem_others.c
    ${TK_LINUX_FB_ROOT}/awtk-port/devices.c
    ${TK_LINUX_FB_ROOT}/awtk-port/main_loop_linux.c
)

# add lcd
set(LCD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/awtk-linux-fb/awtk-port/lcd_linux)
set(SOURCE_FILES ${SOURCE_FILES}
    ${LCD_DIR}/lcd_linux_fb.c
)

# add input
set(INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/awtk-linux-fb/awtk-port/input_thread)
set(SOURCE_FILES ${SOURCE_FILES}
    ${INPUT_DIR}/input_dispatcher.c
    ${INPUT_DIR}/touch_thread.c
)

# add agge
if (NANOVG_BACKEND STREQUAL "AGGE")
    set(AGGE_ROOT ${TK_ROOT}/3rd/agge)
    aux_source_directory(${AGGE_ROOT}/agge SRC)
    set(SOURCE_FILES ${SOURCE_FILES} ${SRC})
endif()

# add nanovg
if (NANOVG_BACKEND STREQUAL "AGGE")
    set(NANOVG_ROOT ${TK_ROOT}/3rd/nanovg)
    aux_source_directory(${NANOVG_ROOT}/base SRC)
    set(SOURCE_FILES ${SOURCE_FILES} ${SRC})
endif()

# add aic g2d
set(AIC_G2D ${CMAKE_CURRENT_SOURCE_DIR}/awtk-linux-fb/awtk-port/aic_g2d)
set(SOURCE_FILES ${SOURCE_FILES}
    ${AIC_G2D}/aic_g2d.c
    ${AIC_G2D}/aic_linux_mem.c
    ${AIC_G2D}/aic_decode_asset.c
    ${AIC_G2D}/aic_graphic_buffer.c
)

set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/3rd/nanovg/agge/nanovg_agge.cpp)
set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/3rd/nanovg/agge/nanovg_vertex.cpp)

if (NANOVG_BACKEND STREQUAL AGGE)
    set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/src/vgcanvas/vgcanvas_nanovg_soft.c)

else()
    set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/src/vgcanvas/vgcanvas_nanovg_gl.c)
endif()

file(GLOB LCD_MEM_SRC ${TK_ROOT}/src/lcd/lcd_mem_*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${LCD_MEM_SRC})

# add gpinyin source file
set(GPINYIN ${TK_ROOT}/3rd/gpinyin)
aux_source_directory(${GPINYIN}/src SRC)
set(SOURCE_FILES ${SOURCE_FILES} ${SRC})
set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/src/input_methods/input_method_creator.c)

if (INPUT_ENGINE STREQUAL "pinyin")
    set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/src/input_engines/input_engine_pinyin.cpp)
else()
    set(SOURCE_FILES ${SOURCE_FILES} ${TK_ROOT}/src/input_engines/input_engine_null.c)
endif()

# add linebreak
set(LIBUNIBREAK ${TK_ROOT}/3rd/libunibreak)
set(SOURCE_FILES ${SOURCE_FILES}
    ${LIBUNIBREAK}/graphemebreak.c
    ${LIBUNIBREAK}/linebreak.c
    ${LIBUNIBREAK}/linebreakdef.c
    ${LIBUNIBREAK}/unibreakbase.c
    ${LIBUNIBREAK}/unibreakdef.c
    ${LIBUNIBREAK}/wordbreak.c
    ${LIBUNIBREAK}/emojidef.c
    ${LIBUNIBREAK}/linebreakdata.c
)

# add fribidi
set(FRIBIDI ${TK_ROOT}/3rd/fribidi)
file(GLOB FRIBIDI_SRC ${FRIBIDI}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${FRIBIDI_SRC})

# add compressors
set(COMPRESSORS ${TK_ROOT}/src/compressors)
file(GLOB COMPRESSORS_SRC ${COMPRESSORS}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${COMPRESSORS_SRC})

# add miniz
set(MINIZ ${TK_ROOT}/3rd/miniz)
set(MINIZ_SRC
    ${MINIZ}/miniz.c
    ${MINIZ}/miniz_tdef.c
    ${MINIZ}/miniz_tinfl.c
    ${MINIZ}/miniz_zip.c
)
set(SOURCE_FILES ${SOURCE_FILES} ${MINIZ_SRC})

# add csv
set(CSV ${TK_ROOT}/src/csv)
file(GLOB CSV_SRC ${CSV}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${CSV_SRC})

# add ubjson
set(UBJSON ${TK_ROOT}/src/ubjson)
file(GLOB UBJSON_SRC ${UBJSON}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${UBJSON_SRC})

# add cjson
set(CJSON ${TK_ROOT}/3rd/cjson)
file(GLOB CJSON_SRC ${CJSON}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${CJSON_SRC})

# add conf_io
set(CONF_IO ${TK_ROOT}/src/conf_io)
file(GLOB CONF_IO_SRC ${CONF_IO}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${CONF_IO_SRC})

# add hal
set(HAL ${TK_ROOT}/src/hal)
file(GLOB HAL_SRC
    ${HAL}/*.c
    ${HAL}/linux/*.c
)
set(SOURCE_FILES ${SOURCE_FILES} ${HAL_SRC})

# add mbedtls
set(MBEDTLS ${TK_ROOT}/3rd/mbedtls)
file(GLOB MBEDTLS_SRC
    ${MBEDTLS}/library/*.c
    ${MBEDTLS}/3rdparty/everest/library/everest.c
    ${MBEDTLS}/3rdparty/everest/library/x25519.c
    ${MBEDTLS}3rdparty/everest/library/Hacl_Curve25519_joined.c
)
set(SOURCE_FILES ${SOURCE_FILES} ${MBEDTLS_SRC})

# add streams
set(STREAMS ${TK_ROOT}/src/streams)
file(GLOB STREAMS_SRC
    ${STREAMS}/buffered/*.c
    ${STREAMS}/file/*.c
    ${STREAMS}/inet/*.c
    ${STREAMS}/mem/*.c
    ${STREAMS}/serial/*.c
    ${STREAMS}/shdlc/*.c
    ${STREAMS}/noisy/*.c
    ${STREAMS}/misc/*.c
    ${STREAMS}/statistics/*.c
)
set(SOURCE_FILES ${SOURCE_FILES} ${STREAMS_SRC})

# add debugger
set(DEBUGGER ${TK_ROOT}/src/debugger)
file(GLOB DEBUGGER_SRC ${DEBUGGER}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${DEBUGGER_SRC})

# add charset
set(CHARSET ${TK_ROOT}/src/charset)
file(GLOB CHARSET_SRC ${CHARSET}/*.c)
set(SOURCE_FILES ${SOURCE_FILES} ${CHARSET_SRC})

# add awtk core
file(GLOB AWTK_SRC
    ${TK_ROOT}/src/layouters/*.c
    ${TK_ROOT}/src/base/*.c
    ${TK_ROOT}/src/ui_loader/*.c
    ${TK_ROOT}/src/xml/*.c
    ${TK_ROOT}/src/svg/*.c
    ${TK_ROOT}/src/clip_board/*.c
    ${TK_ROOT}/src/font_loader/*.c
    ${TK_ROOT}/src/blend/*.c
    ${TK_ROOT}/src/image_loader/*.c
    ${TK_ROOT}/src/designer_support/*.c
    ${TK_ROOT}/src/widget_animators/*.c
    ${TK_ROOT}/src/misc/*.c
    ${TK_ROOT}/src/window_animators/*.c
    ${TK_ROOT}/src/dialog_highlighters/*.c
    ${TK_ROOT}/src/window_manager/window_manager_default.c
    ${TK_ROOT}/src/graphic_buffer/graphic_buffer_default.c
    ${TK_ROOT}/src/native_window/native_window_raw.c
)
set(SOURCE_FILES ${SOURCE_FILES} ${AWTK_SRC})

file(GLOB SRC_SRC
    ${TK_ROOT}/src/awtk_global.c
    ${TK_ROOT}/src/widgets/*.c
    ${TK_ROOT}/src/tkc/*.c
    ${TK_ROOT}/src/fscript_ext/*.c
    ${TK_ROOT}/src/platforms/pc/*.c
    ${TK_ROOT}/src/ext_widgets/*.c
    ${TK_ROOT}/src/ext_widgets/*/*.c
)
set(SOURCE_FILES ${SOURCE_FILES} ${SRC_SRC})

set(SOURCE_FILES ${SOURCE_FILES}
    ${TK_ROOT}/src/lcd/lcd_mono.c
    ${TK_ROOT}/src/main_loop/main_loop_simple.c
)

add_library(${PROJ} SHARED ${SOURCE_FILES})

target_link_libraries(${PROJ} PUBLIC m dl pthread stdc++)

# add path
set(CPPPATH
    ${TK_ROOT}/src
    ${TK_ROOT}/3rd
    ${TK_ROOT}/src/ext_widgets
    ${TK_ROOT}/3rd/pixman
    ${TK_ROOT}/3rd/agge
    ${TK_ROOT}/3rd/cjson
    ${TK_ROOT}/3rd/fribidi
    ${TK_ROOT}/3rd/mbedtls/include
    ${TK_ROOT}/3rd/mbedtls/3rdparty/everest/include
    ${TK_ROOT}/3rd/nanovg
    ${TK_ROOT}/3rd/nanovg/gl
    ${TK_ROOT}/3rd/nanovg/base
    ${TK_ROOT}/3rd/nanovg/agge
    ${TK_ROOT}/3rd/SDL/src
    ${TK_ROOT}/3rd/SDL/include
    ${TK_ROOT}/3rd/SDL/src/video/khronos
    ${TK_ROOT}/3rd/agge/src
    ${TK_ROOT}/3rd/agge/include
    ${TK_ROOT}/3rd/gpinyin/include
    ${TK_ROOT}/3rd/libunibreak
    ${TK_ROOT}/3rd/nativefiledialog/src
    ${TK_ROOT}/3rd/nativefiledialog/src/include
    ${TK_ROOT}/3rd/gtest/googletest
    ${TK_ROOT}/3rd/gtest/googletest/include
    ${TK_ROOT}/tools
    ${TK_LINUX_FB_ROOT}
    ${TK_LINUX_FB_ROOT}/awtk-port/input_thread
    ${TK_LINUX_FB_ROOT}/awtk-port/lcd_linux
    ${TK_LINUX_FB_ROOT}/awtk-port/aic_g2d
    ${TK_ROOT}/src/native_window
)
target_include_directories(${PROJ} PRIVATE ${CPPPATH})

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS ${PROJ} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

# set project
set(PRJ_APP demo_char)

set(USER_APPS_ROOT  ${CMAKE_CURRENT_SOURCE_DIR}/user_apps)
set(USER_DEMO_CHART ${USER_APPS_ROOT}/awtk-demo-chart)

## demo source file
set(DEMO_SOURCE_FILES "")

# add user 3rd
set(USER_DEMO_3RD ${USER_DEMO_CHART}/3rd/awtk-widget-chart-view/src)
file(GLOB DEMO_3RD_SRC
    ${USER_DEMO_3RD}/*.c
    ${USER_DEMO_3RD}/base/*.c
    ${USER_DEMO_3RD}/chart_view/*.c
    ${USER_DEMO_3RD}/pie_slice/*.c
)
set(DEMO_SOURCE_FILES ${DEMO_SOURCE_FILES} ${DEMO_3RD_SRC})

# add user src
set(USER_DEMO_SRC ${USER_DEMO_CHART}/src)
file(GLOB USER_DEMO_SRC
    ${USER_DEMO_SRC}/*.c
    ${USER_DEMO_SRC}/pages/*.c
    ${USER_DEMO_SRC}/common/*.c
)
set(DEMO_SOURCE_FILES ${DEMO_SOURCE_FILES} ${USER_DEMO_SRC})

# add link lib
find_library(LIBMPP_GE libmpp_ge.so)
find_library(LIBMPP_BASE libmpp_base.so)
find_library(LIBMPP_VE libmpp_ve.so)
find_library(LIBMPP_DEC libmpp_decoder.so)

set(PRJ_APP demo_chart)
add_executable(${PRJ_APP} ${DEMO_SOURCE_FILES})
target_include_directories(${PRJ_APP} PRIVATE ${CPPPATH})
target_link_libraries(${PRJ_APP} PUBLIC awtk ${LIBMPP_GE} ${LIBMPP_DEC} ${LIBMPP_BASE} ${LIBMPP_VE})
set_target_properties(${PRJ_APP} PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
    install(TARGETS ${PRJ_APP} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_BINDIR
