set(BASE_MSG
    message/aic_message.c
)

set(BASE_STREAM
    stream/aic_stream.c
    stream/file/aic_file_stream.c
)

set(BASE_PARSER
    parser/aic_parser.c
    parser/mov/aic_mov_parser.c
    parser/mov/mov_tags.c
    parser/mov/mov.c
    parser/rawdata/aic_raw_parser.c
    parser/mp3/aic_mp3_parser.c
    parser/mp3/mp3.c
)

set(BASE_RENDER
    render/aic_video_render.c
)

set(BASE_AUDIO_RENDER
    render/aic_audio_render.c
)

add_library(openmax_base SHARED
    ${BASE_MSG}
    ${BASE_STREAM}
    ${BASE_PARSER}
    ${BASE_RENDER}
    ${BASE_AUDIO_RENDER}
)

target_link_libraries(openmax_base mpp_base asound)

if (AAC_DECODER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_DECODER")
endif()

target_include_directories(openmax_base PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/mov"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/rawdata"
    "${CMAKE_CURRENT_SOURCE_DIR}/stream/file"
    "${CMAKE_CURRENT_SOURCE_DIR}/../audio_decoder/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/mp3"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS openmax_base RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

