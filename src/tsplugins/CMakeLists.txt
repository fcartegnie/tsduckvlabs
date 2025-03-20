foreach(num ${TSPLUGINS})
    add_library(tsplugin_${num} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/tsplugin_${num}.cpp)
    target_link_libraries(tsplugin_${num} tsduck)
    set_target_properties(tsplugin_${num} PROPERTIES 
        PREFIX "" # tsplugin_name instead of libtsplugin_name
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()
