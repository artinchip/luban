
add_library(mpp_base SHARED
    memory/dma_allocator.c
    memory/mpp_mem.c
)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS mpp_base RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
