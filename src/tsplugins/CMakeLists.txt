include(GNUInstallDirs)
set(TSPLUGIN_DIR ${CMAKE_INSTALL_LIBDIR}/tsduck)

foreach(num ${TSPLUGINS})
    add_library(tsplugin_${num} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/tsplugin_${num}.cpp)
    target_link_libraries(tsplugin_${num} tsduck)
    set_target_properties(tsplugin_${num} PROPERTIES 
        PREFIX "" # tsplugin_name instead of libtsplugin_name
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_LIBDIR}/tsduck
    )
    install(TARGETS tsplugin_${num}
        COMPONENT tsduck
        LIBRARY DESTINATION ${TSPLUGIN_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endforeach()
