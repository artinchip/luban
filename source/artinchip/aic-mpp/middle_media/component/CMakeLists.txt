set(MM_DEMUXER
    src/mm_demuxer_component.c
)

set(MM_VDEC
    src/mm_vdec_component.c
)
set(MM_VIDEO_RENDER
    src/mm_video_render_component.c
)
set(MM_ADEC
    src/mm_adec_component.c
)

set(MM_AUDIO_RENDER
    src/mm_audio_render_component.c
)

set(MM_CLOCK
    src/mm_clock_component.c
)

set(MM_MUXER
    src/mm_muxer_component.c
)

set(MM_VENC
    src/mm_venc_component.c
)

add_library(mm_component SHARED
    ${MM_DEMUXER}
    ${MM_VDEC}
    ${MM_VIDEO_RENDER}
    ${MM_ADEC}
    ${MM_AUDIO_RENDER}
    ${MM_CLOCK}
    ${MM_MUXER}
    ${MM_VENC}
    mm_core.c
)

if (AAC_DECODER)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_DECODER")
endif()

target_link_libraries(mm_component mpp_audio_decoder mpp_decoder mpp_encoder mpp_ge mm_base mpp_base)

target_include_directories(mm_component PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/../audio_decoder/include"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mm_component RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

