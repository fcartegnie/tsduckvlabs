set(tsp_SOURCES tsp.cpp)
if(BUILD_SHARED_LIBS)
    set(tsp_tsduck tsduck)
else()
    list(TRANSFORM TSPLUGINS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/../tsplugins/tsplugin_ OUTPUT_VARIABLE tsplugin_static_SOURCES)
    list(TRANSFORM tsplugin_static_SOURCES APPEND ".cpp")
    list(APPEND tsp_SOURCES ${tsplugin_static_SOURCES})

    set(tsp_tsduck tsducklib)
endif()
add_executable(tsp ${tsp_SOURCES})
target_link_libraries(tsp ${tsp_tsduck})
target_compile_definitions(tsp PRIVATE TS_WINVER_FILETYPE=VFT_APP)

add_executable(tsxml tsxml.cpp)
target_compile_definitions(tsxml PRIVATE TS_WINVER_FILETYPE=VFT_APP)
target_link_libraries(tsxml ${tsp_tsduck})

install(TARGETS tsxml tsp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
