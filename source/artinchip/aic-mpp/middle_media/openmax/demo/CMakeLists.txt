include_directories(../include ../base/include ../../base/include)

add_executable(demuxer_test demuxer.c)

target_link_libraries(demuxer_test mpp_base openmax_base openmax)

target_link_libraries(demuxer_test pthread)

set_target_properties(demuxer_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS demuxer_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR



include_directories(../include ../base/include ../../base/include)

add_executable(vdecoder_test vdecoder.c bit_stream_parser.c)

target_link_libraries(vdecoder_test mpp_base openmax_base openmax)

target_link_libraries(vdecoder_test pthread)

set_target_properties(vdecoder_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS vdecoder_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR


include_directories(../include ../base/include ../../base/include)

add_executable(vdec2vo_test vdec2vo.c bit_stream_parser.c)

target_link_libraries(vdec2vo_test mpp_base openmax_base openmax)

target_link_libraries(vdec2vo_test pthread)

set_target_properties(vdec2vo_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS vdec2vo_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

include_directories(../include ../base/include ../audio_decoder/include ../../base/include)

add_executable(adecoder_test adecoder.c)

target_link_libraries(adecoder_test mpp_base openmax_base openmax)

target_link_libraries(adecoder_test pthread)

set_target_properties(adecoder_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS adecoder_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR



include_directories(../include ../base/include ../audio_decoder/include ../../base/include)

add_executable(adec2ao_test adec2ao.c)

target_link_libraries(adec2ao_test mpp_base openmax_base openmax)

target_link_libraries(adec2ao_test pthread)

set_target_properties(adec2ao_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS adec2ao_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

