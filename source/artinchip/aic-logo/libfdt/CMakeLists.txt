add_library(logo_fdt SHARED
    fdt.c
    fdt_addresses.c
    fdt_empty_tree.c
    fdt_overlay.c
    fdt_ro.c
    fdt_rw.c
    fdt_sw.c
    fdt_strerror.c
    fdt_wip.c
)

set_target_properties(logo_fdt PROPERTIES FOLDER "logo_fdt")

target_include_directories(logo_fdt PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()

include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS logo_fdt RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR

