
add_library(media_player SHARED
	aic_player.c
)

target_link_libraries(media_player mpp_base openmax)
target_link_libraries(media_player pthread)

target_include_directories(media_player PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS media_player RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR



include_directories(./include ../base/include ../../base/include)

add_executable(player_demo player_demo.c)

target_link_libraries(player_demo mpp_base openmax media_player)

target_link_libraries(player_demo pthread)

set_target_properties(player_demo PROPERTIES INSTALL_RPATH "/usr/local/lib")

#install(TARGETS mpp_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS player_demo RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR


