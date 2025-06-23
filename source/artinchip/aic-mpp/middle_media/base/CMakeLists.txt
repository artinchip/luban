set(BASE_MSG
    message/aic_message.c
    comm/aic_tag.c
)

set(BASE_STREAM
    stream/aic_stream.c
    stream/file/aic_file_stream.c
)

if (WAV_DEMUXER)
set(DEMUXER_WAV
    parser/wav/aic_wav_parser.c
    parser/wav/wav.c
)
endif()

if (AVI_DEMUXER)
set(DEMUXER_AVI
    parser/avi/aic_avi_parser.c
    parser/avi/avi.c
)
endif()

if (MKV_DEMUXER)
set(DEMUXER_MKV
    parser/mkv/aic_mkv_parser.c
    parser/mkv/matroska.c
    parser/mkv/matroska_tag.c
)
endif()

if (TS_DEMUXER)
set(DEMUXER_TS
    parser/ts/aic_ts_parser.c
    parser/ts/mpegts.c
    parser/ts/mpegts_audio.c
)
endif()

if (FLV_DEMUXER)
set(DEMUXER_FLV
    parser/flv/aic_flv_parser.c
    parser/flv/flv.c
)
endif()

if (RTSP_DEMUXER)
set(DEMUXER_RTSP
    parser/rtsp/aic_rtsp_parser.cpp
    parser/rtsp/rtsp.cpp
)
endif()

set(BASE_PARSER
    parser/aic_parser.c
    parser/mov/aic_mov_parser.c
    parser/mov/mov_tags.c
    parser/mov/mov.c
    parser/rawdata/aic_raw_parser.c
    parser/mp3/aic_mp3_parser.c
    parser/mp3/mp3.c
    ${DEMUXER_WAV}
    ${DEMUXER_AVI}
    ${DEMUXER_MKV}
    ${DEMUXER_TS}
    ${DEMUXER_FLV}
    ${DEMUXER_RTSP}
)

set(BASE_MUXER
    muxer/aic_muxer.c
    muxer/mp4/aic_mp4_muxer.c
    muxer/mp4/mp4_muxer.c
)

set(BASE_RENDER
    render/aic_video_render.c
)

set(BASE_AUDIO_RENDER
    render/aic_audio_render.c
)

add_library(mm_base SHARED
    ${BASE_MSG}
    ${BASE_STREAM}
    ${BASE_PARSER}
    ${BASE_MUXER}
    ${BASE_RENDER}
    ${BASE_AUDIO_RENDER}
)

target_link_libraries(mm_base mpp_base asound)

if (RTSP_DEMUXER)
    target_link_libraries(mm_base BasicUsageEnvironment groupsock liveMedia UsageEnvironment)
endif()

if (AAC_DECODER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_DECODER")
endif()

if (WAV_DEMUXER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWAV_DEMUXER")
endif()

if (AVI_DEMUXER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAVI_DEMUXER")
endif()

if (MKV_DEMUXER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMKV_DEMUXER")
endif()

if (TS_DEMUXER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTS_DEMUXER")
endif()

if (FLV_DEMUXER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLV_DEMUXER")
endif()

if (RTSP_DEMUXER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRTSP_DEMUXER")
endif()

target_include_directories(mm_base PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/comm"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/mov"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/avi"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/mkv"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/flv"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/rawdata"
    "${CMAKE_CURRENT_SOURCE_DIR}/stream/file"
    "${CMAKE_CURRENT_SOURCE_DIR}/../audio_decoder/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/muxer/mp4"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/rtsp"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mm_base RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

