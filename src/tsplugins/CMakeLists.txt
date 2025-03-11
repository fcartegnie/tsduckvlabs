foreach(num ${TSPLUGINS})
    add_library(tsplugin_${num} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/tsplugin_${num}.cpp)
    target_link_libraries(tsplugin_${num} tsduck)
endforeach()
