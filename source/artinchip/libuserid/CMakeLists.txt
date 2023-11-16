cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(userid LANGUAGES C)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC}	${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
	${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

add_library(userid_parser SHARED
    src/part.c
    src/parser.c
)

target_include_directories(userid_parser PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(userid_parser z)

add_executable(userid src/userid_reader.c)
target_link_libraries(userid userid_parser)
# set_target_properties(userid PROPERTIES INSTALL_RPATH "/usr/local/lib")

# Install
# install directories
if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "ERROR: CMAKE_INSTALL_PREFIX is not defined.")
endif()

include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    install(TARGETS userid_parser RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}")
endif() # CMAKE_INSTALL_FULL_LIBDIR
if(DEFINED CMAKE_INSTALL_FULL_INCLUDEDIR)
    install(FILES inc/artinchip/userid.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/artinchip/")
endif() # CMAKE_INSTALL_FULL_LIBDIR
if(DEFINED CMAKE_INSTALL_FULL_BINDIR)
    install(TARGETS userid RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif() # CMAKE_INSTALL_FULL_BINDIR
