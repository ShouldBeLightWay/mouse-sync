add_library(mouse-sync-kde-wayland-backend STATIC
    src/kde_wayland_backend.cpp
)

target_include_directories(mouse-sync-kde-wayland-backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mouse-sync-kde-wayland-backend
    PUBLIC
        mouse-sync-core
)