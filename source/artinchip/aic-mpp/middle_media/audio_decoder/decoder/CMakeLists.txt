set(DECODER_MP3
    mp3/mp3_decoder.c
)

if (AAC_DECODER)
set(DECODER_AAC
    aac/aac_decoder.c
)
add_library(mpp_audio_decoder SHARED
    ${DECODER_MP3}
    ${DECODER_AAC}
    aic_audio_decoder.c
    audio_frame_manager.c
    audio_packet_manager.c
)
else ()
add_library(mpp_audio_decoder SHARED
    ${DECODER_MP3}
    aic_audio_decoder.c
    audio_frame_manager.c
    audio_packet_manager.c
)
endif()

target_include_directories(mpp_audio_decoder PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)


target_link_libraries(mpp_audio_decoder mpp_base mad)
if (AAC_DECODER)
	target_link_libraries(mpp_audio_decoder faad)
endif()

if (AAC_DECODER)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_DECODER")
endif()

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_audio_decoder RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
