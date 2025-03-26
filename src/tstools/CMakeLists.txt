if(BUILD_SHARED_LIBS)
    set(tsp_tsduck tsduck)
else()
    set(tsp_tsduck tsducklib)
endif()
add_executable(tsp tsp.cpp)
target_link_libraries(tsp PRIVATE "$<TARGET_FILE:allplugins>")
target_link_libraries(tsp PRIVATE ${tsp_tsduck})
target_compile_definitions(tsp PRIVATE TS_WINVER_FILETYPE=VFT_APP)

add_executable(tsxml tsxml.cpp)
target_link_libraries(tsxml PRIVATE "$<TARGET_FILE:allplugins>")
target_compile_definitions(tsxml PRIVATE TS_WINVER_FILETYPE=VFT_APP)
target_link_libraries(tsxml PRIVATE ${tsp_tsduck})

add_executable(tsversion tsversion.cpp)
target_link_libraries(tsversion PRIVATE "$<TARGET_FILE:allplugins>")
target_compile_definitions(tsversion PRIVATE TS_WINVER_FILETYPE=VFT_APP)
target_link_libraries(tsversion PRIVATE ${tsp_tsduck})

add_executable(tslatencymonitor tslatencymonitor.cpp)
target_link_libraries(tslatencymonitor PRIVATE "$<TARGET_FILE:allplugins>")
target_compile_definitions(tslatencymonitor PRIVATE TS_WINVER_FILETYPE=VFT_APP)
target_link_libraries(tslatencymonitor PRIVATE ${tsp_tsduck})

install(TARGETS tsxml tsp tsversion
    COMPONENT tsduck
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
