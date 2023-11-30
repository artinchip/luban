
include_directories(../include ../base/include ../audio_decoder/include ../../base/include)

add_executable(muxer_demo muxer_demo.c)

target_link_libraries(muxer_demo mpp_base openmax_base)

target_link_libraries(muxer_demo pthread)

set_target_properties(muxer_demo PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS muxer_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS muxer_demo RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR


add_library(media_recorder SHARED
	aic_recorder.c
)
target_link_libraries(media_recorder mpp_base openmax)

target_link_libraries(media_recorder pthread)

target_include_directories(media_recorder PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS media_recorder RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR


include_directories(./include ../openmax/include ../base/include ../audio_decoder/include ../../base/include)

add_executable(recorder_demo recorder_demo.c )

target_link_libraries(recorder_demo mpp_base openmax_base openmax media_recorder cjson)

target_link_libraries(recorder_demo pthread)

set_target_properties(recorder_demo PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS recorder_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS recorder_demo RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR


