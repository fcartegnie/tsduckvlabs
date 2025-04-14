include(GNUInstallDirs)
set(TSPLUGIN_DIR ${CMAKE_INSTALL_LIBDIR}/tsduck)

foreach(plugin_source ${TSPLUGINS})
    string(REGEX REPLACE ".cpp$" "" plugin_name ${plugin_source})
    add_library(${plugin_name} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/${plugin_source})
    target_link_libraries(${plugin_name} PRIVATE ${tsduck_libs})
    set_target_properties(${plugin_name} PROPERTIES 
        PREFIX "" # tsplugin_name instead of libtsplugin_name
        LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}/${TSPLUGIN_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}/${TSPLUGIN_DIR}>"
    )
    install(TARGETS ${plugin_name}
        COMPONENT tsduck
        LIBRARY DESTINATION ${TSPLUGIN_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endforeach()
