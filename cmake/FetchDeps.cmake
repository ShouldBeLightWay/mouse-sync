include(FetchContent)

# ── nlohmann/json ────────────────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
# Only build the headers; skip tests and benchmarks
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# ── CLI11 ─────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
)
set(CLI11_PRECOMPILED OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(CLI11)
