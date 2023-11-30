set(OPENMAX_DEMUXER
    component/OMX_DemuxerComponent.c
)

set(OPENMAX_VDEC
    component/OMX_VdecComponent.c
)
set(OPENMAX_VIDEO_RENDER
    component/OMX_VideoRenderComponent.c
)
set(OPENMAX_ADEC
    component/OMX_AdecComponent.c
)

set(OPENMAX_AUDIO_RENDER
    component/OMX_AudioRenderComponent.c
)

set(OPENMAX_CLOCK
    component/OMX_ClockComponent.c
)

set(OPENMAX_MUXER
    component/OMX_MuxerComponent.c
)

set(OPENMAX_VENC
    component/OMX_VencComponent.c
)

add_library(openmax SHARED
    ${OPENMAX_DEMUXER}
    ${OPENMAX_VDEC}
    ${OPENMAX_VIDEO_RENDER}
    ${OPENMAX_ADEC}
    ${OPENMAX_AUDIO_RENDER}
    ${OPENMAX_CLOCK}
    ${OPENMAX_MUXER}
    ${OPENMAX_VENC}
	OMX_Core.c
)

if (AAC_DECODER)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_DECODER")
endif()

target_link_libraries(openmax mpp_audio_decoder mpp_decoder mpp_encoder mpp_ge openmax_base mpp_base)

target_include_directories(openmax PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/component"
	"${CMAKE_CURRENT_SOURCE_DIR}/../audio_decoder/include"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS openmax RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

