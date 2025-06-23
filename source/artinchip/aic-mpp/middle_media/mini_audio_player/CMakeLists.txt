add_library(media_audio_player SHARED
	mini_audio_player.c
)

target_link_libraries(media_audio_player mm_base mpp_base mpp_audio_decoder)
target_link_libraries(media_audio_player pthread)

target_include_directories(media_audio_player PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../audio_decoder/include"
)


if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS media_audio_player RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR



include_directories(./include ../base/include ../../base/include)

add_executable(audio_player_demo mini_audio_player_demo.c)

target_link_libraries(audio_player_demo mpp_base media_audio_player)

set_target_properties(audio_player_demo PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS audio_player_demo RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR