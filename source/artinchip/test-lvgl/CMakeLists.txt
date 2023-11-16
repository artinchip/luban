cmake_minimum_required(VERSION 3.1)

project(test_lvgl)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC} ${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
 ${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

set(CMAKE_C_FLAGS "-O0 -g0")

include_directories(. ./aic_ui)

add_executable(${PROJECT_NAME} main.c
	aic_ui/aic_dec.c
	aic_ui/aic_ui.c
	aic_ui/cpu_mem.c
	aic_ui/font/ui_font_Big.c
	aic_ui/font/ui_font_Title.c
	aic_ui/font/ui_font_H1.c
	lv_fbdev.c
	lv_ge2d.c
	lv_port_indev.c
	lv_port_disp.c
)

# Find libts for link
if ( ${USE_RTP_TSLIB} STREQUAL "yes")
	add_definitions(-DUSE_RTP_TSLIB)
	find_library(LIBTS libts.so)
	target_link_libraries(${PROJECT_NAME} ${LIBTS})
endif()

find_library(LIBMPP_GE libmpp_ge.so)
find_library(LIBMPP_BASE libmpp_base.so)
find_library(LIBMPP_VE libmpp_ve.so)
find_library(LIBMPP_DEC libmpp_decoder.so)
target_link_libraries(${PROJECT_NAME} ${LIBMPP_GE} ${LIBMPP_DEC} ${LIBMPP_BASE} ${LIBMPP_VE})
#target_link_libraries(${PROJECT_NAME} lvgl lvgl_examples lvgl_demos)
target_link_libraries(${PROJECT_NAME} lvgl )
target_link_libraries(${PROJECT_NAME} media_player)
target_link_libraries(${PROJECT_NAME} m)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "/usr/local/lib")

include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif()

add_definitions(-DLVGL_DIR="L:${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data/")

if(DEFINED CMAKE_INSTALL_FULL_DATAROOTDIR)
	install(DIRECTORY aic_ui/lvgl_data/
		DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data)
endif() # CMAKE_INSTALL_FULL_DATAROOTDIR
