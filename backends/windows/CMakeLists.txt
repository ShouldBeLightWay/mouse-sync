add_library(mouse-sync-windows-backend STATIC
    src/windows_backend.cpp
)

target_include_directories(mouse-sync-windows-backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mouse-sync-windows-backend
    PUBLIC  mouse-sync-core
    PRIVATE # WinAPI is implicit on Windows
)

# Require at least Windows Vista for Registry/SPI APIs we use
target_compile_definitions(mouse-sync-windows-backend PRIVATE
    WINVER=0x0600
    _WIN32_WINNT=0x0600
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
