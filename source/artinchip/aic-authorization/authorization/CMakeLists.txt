add_library(authorization SHARED
    authorization.c
    rsa_pad.c
)

# Find libkcapi for link
find_library(LIBKCAPI_PATH libkcapi.so)
target_link_libraries(authorization ${LIBKCAPI_PATH})

target_include_directories(authorization PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS authorization RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
if(DEFINED CMAKE_INSTALL_FULL_INCLUDEDIR)
    install(FILES authorization.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/artinchip/")
endif() # CMAKE_INSTALL_FULL_INCLUDEDIR
