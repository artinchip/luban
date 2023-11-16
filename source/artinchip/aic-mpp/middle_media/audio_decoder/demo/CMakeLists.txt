
include_directories(../include ../base/include ../../base/include)

add_executable(audio_decoder_test audio_decoder_test.c )

target_link_libraries(audio_decoder_test mpp_base mpp_audio_decoder)

target_link_libraries(audio_decoder_test pthread)

set_target_properties(audio_decoder_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS audio_decoder_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR


include_directories(../include ../base/include ../../base/include)

add_executable(audio_decoder_render_test audio_decoder_render_test.c pcm2wav.c)

target_link_libraries(audio_decoder_render_test mpp_base openmax_base mpp_audio_decoder)

target_link_libraries(audio_decoder_render_test pthread)

set_target_properties(audio_decoder_render_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS audio_decoder_render_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

