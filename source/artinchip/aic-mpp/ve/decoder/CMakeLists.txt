set(DECODER_MJPEG
    jpeg/jpeg_hal.c
    jpeg/mjpeg_decoder.c
)

set(DECODER_PNG
    png/png_decoder.c
    png/png_hal.c
)

set(DECODER_AVC
    h264/h264_decoder.c
    h264/h264_hal.c
    h264/h264_nal.c
    h264/h264_refs.c
)

add_library(mpp_decoder SHARED
    ${DECODER_MJPEG}
    ${DECODER_PNG}
    ${DECODER_AVC}
    aicp/aicp_wrapper.c
    mpp_decoder.c
    frame_manager.c
    packet_manager.c
)

target_link_libraries(mpp_decoder mpp_base mpp_ve dl)

target_include_directories(mpp_decoder PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/jpeg"
    "${CMAKE_CURRENT_SOURCE_DIR}/png"
    "${CMAKE_CURRENT_SOURCE_DIR}/h264"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_decoder RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
