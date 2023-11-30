include_directories(../ve/include)

add_executable(mpp_test mpp_dec_test.c bit_stream_parser.c)

target_link_libraries(mpp_test mpp_decoder mpp_encoder)
target_link_libraries(mpp_test pthread)

set_target_properties(mpp_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### pic_test ###############################
add_executable(fv_test four_video_dec_test.c bit_stream_parser.c)

target_link_libraries(fv_test mpp_decoder)
target_link_libraries(fv_test pthread)

set_target_properties(fv_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS fv_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### pic_test ###############################
add_executable(pic_test picture_decoder_test.c)

target_link_libraries(pic_test mpp_decoder mpp_ge)

set_target_properties(pic_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS pic_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### pic_ext_test ###############################
add_executable(pic_ext_test pic_decode_ext_test.c)

target_link_libraries(pic_ext_test mpp_decoder)

set_target_properties(pic_ext_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS pic_ext_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### pic_ext2_test ###############################
add_executable(pic_ext2_test pic_decode_ext2_test.c)

target_link_libraries(pic_ext2_test mpp_decoder)

set_target_properties(pic_ext2_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS pic_ext2_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### pic_err_test ###############################
add_executable(pic_err_test pic_error_test.c)

target_link_libraries(pic_err_test mpp_decoder)

set_target_properties(pic_err_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS pic_err_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### gz_test #######################################
add_executable(gz_test gz_test.c)

target_link_libraries(gz_test mpp_decoder)

set_target_properties(gz_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS gz_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### mpp_heap_fb_test #######################################
add_executable(mpp_heap_fb_test mpp_heap_fb_test.c)

target_link_libraries(mpp_heap_fb_test mpp_decoder)

set_target_properties(mpp_heap_fb_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_heap_fb_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### hwpng_test #######################################
add_executable(hwpng_test hwpng_test.c)

target_link_libraries(hwpng_test mpp_decoder)

set_target_properties(hwpng_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS hwpng_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### hwjpeg_test #######################################
add_executable(hwjpeg_test hwjpeg_test.c)

target_link_libraries(hwjpeg_test mpp_decoder)

set_target_properties(hwjpeg_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS hwjpeg_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### ge_test #######################################
add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/ge_data")

file(GLOB APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/ge_*.c)
foreach(sourcefile ${APP_SOURCES})
	file(RELATIVE_PATH filename ${CMAKE_CURRENT_SOURCE_DIR} ${sourcefile})
	string( REPLACE ".c" "" file ${filename} )
	list (APPEND bin ${file})
	add_executable(${file} ${filename})
	target_link_libraries(${file} mpp_ge)
	if(${file} STREQUAL "ge_rotate")
		target_link_libraries(${file} m)
	endif()
	set_target_properties(${file} PROPERTIES INSTALL_RPATH "/usr/local/lib")
endforeach(sourcefile ${APP_SOURCES})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	foreach(binfile ${bin})
		install(TARGETS ${binfile}
			RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
	endforeach(binfile ${bin})
endif() # CMAKE_INSTALL_FULL_LIBDIR

if(DEFINED CMAKE_INSTALL_FULL_DATAROOTDIR)
	install(DIRECTORY ge_data/
		DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/ge_data)
endif() # CMAKE_INSTALL_FULL_DATAROOTDIR

############################### vegede_test #######################################
add_executable(vegede_test ve_ge_de_test.c)

target_link_libraries(vegede_test mpp_decoder mpp_ge)

set_target_properties(vegede_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS vegede_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### jpeg_encode #######################################
add_executable(jpeg_encode_test jpeg_encode_test.c)

target_link_libraries(jpeg_encode_test mpp_encoder)

set_target_properties(jpeg_encode_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS jpeg_encode_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### jpeg_decode_test #######################################
add_executable(jpeg_decode_test_1606 jpeg_decode_test_1606.c)

target_link_libraries(jpeg_decode_test_1606 mpp_decoder)

set_target_properties(jpeg_decode_test_1606 PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS jpeg_decode_test_1606 RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### jpeg_encode #######################################
add_executable(jpeg_encode_test_1606 jpeg_encode_test_1606.c)

target_link_libraries(jpeg_encode_test_1606 mpp_encoder)

set_target_properties(jpeg_encode_test_1606 PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS jpeg_encode_test_1606 RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

############################### mpp_video #######################################
add_executable(mpp_video_test mpp_video.c mpp_video_test.c bit_stream_parser.c)

target_link_libraries(mpp_video_test mpp_decoder pthread)

set_target_properties(mpp_video_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_video_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
