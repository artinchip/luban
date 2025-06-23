add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/ge_data")

file(GLOB APP_SOURCES_PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/public/*.c)
file(GLOB APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/ge_*.c)
foreach(sourcefile ${APP_SOURCES})
	file(RELATIVE_PATH filename ${CMAKE_CURRENT_SOURCE_DIR} ${sourcefile})
	string( REPLACE ".c" "" file ${filename} )
	list (APPEND bin ${file})
	add_executable(${file} ${filename} ${APP_SOURCES_PUBLIC})
	target_link_libraries(${file} mpp_ge m)
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