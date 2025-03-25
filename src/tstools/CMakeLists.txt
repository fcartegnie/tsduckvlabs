if(BUILD_SHARED_LIBS)
    set(tsp_tsduck tsduck)
else()
    set(tsp_tsduck tsducklib)
endif()
add_executable(tsp tsp.cpp)
target_link_libraries(tsp ${tsp_tsduck})
target_compile_definitions(tsp PRIVATE TS_WINVER_FILETYPE=VFT_APP)

add_executable(tsxml tsxml.cpp)
target_compile_definitions(tsxml PRIVATE TS_WINVER_FILETYPE=VFT_APP)
target_link_libraries(tsxml ${tsp_tsduck})

add_executable(tsversion tsversion.cpp)
target_compile_definitions(tsversion PRIVATE TS_WINVER_FILETYPE=VFT_APP)
target_link_libraries(tsversion ${tsp_tsduck})

install(TARGETS tsxml tsp tsversion
    COMPONENT tsduck
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
