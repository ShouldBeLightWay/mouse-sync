add_executable(mouse-sync-cli main.cpp)

set_target_properties(mouse-sync-cli PROPERTIES OUTPUT_NAME "mouse-sync")

target_link_libraries(mouse-sync-cli
    PRIVATE
        mouse-sync-core
    mouse-sync-backends
        CLI11::CLI11
)

target_compile_definitions(mouse-sync-cli PRIVATE
    MOUSE_SYNC_VERSION="${CMAKE_PROJECT_VERSION}"
)

if(WIN32)
    target_compile_definitions(mouse-sync-cli PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
endif()
