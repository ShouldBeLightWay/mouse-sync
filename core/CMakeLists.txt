# core is header-only: expose include directory as an INTERFACE library
add_library(mouse-sync-core INTERFACE)

target_include_directories(mouse-sync-core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mouse-sync-core INTERFACE nlohmann_json::nlohmann_json)
