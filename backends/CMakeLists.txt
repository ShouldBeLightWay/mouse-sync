add_library(mouse-sync-backends STATIC
    src/backend_dispatch.cpp
)

target_link_libraries(mouse-sync-backends
    PUBLIC
        mouse-sync-core
)

if(WIN32)
    add_subdirectory(windows)
    target_link_libraries(mouse-sync-backends PRIVATE mouse-sync-windows-backend)
endif()

if(UNIX AND NOT APPLE)
    add_subdirectory(linux/kde_wayland)
    target_link_libraries(mouse-sync-backends PRIVATE mouse-sync-kde-wayland-backend)
endif()
