include_directories(../authorization)

add_executable(test_hw_authorization test_aic_hw_authorization.c)

target_link_libraries(test_hw_authorization authorization)

set_target_properties(test_hw_authorization PROPERTIES INSTALL_RPATH "/usr/local/lib")

if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
	install(TARGETS test_hw_authorization RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_BINDIR
