set(JPEG_ENCODER
    jpeg/jpeg_tables.c
    jpeg/jpeg_enc.c
    jpeg/jpeg_enc_hal.c
)

add_library(mpp_encoder SHARED
    ${JPEG_ENCODER}
)

target_link_libraries(mpp_encoder mpp_base mpp_ve)

target_include_directories(mpp_encoder PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/jpeg"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_encoder RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
