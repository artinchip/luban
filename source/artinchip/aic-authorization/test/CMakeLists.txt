include_directories(../authorization)

add_executable(aic_hw_authorization_test test_aic_hw_authorization.c)

target_link_libraries(aic_hw_authorization_test authorization)

set_target_properties(aic_hw_authorization_test PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
	install(TARGETS aic_hw_authorization_test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_BINDIR
