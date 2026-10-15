cmake_minimum_required(VERSION 3.22)

project(Ohpe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OHPE_BUILD_TESTS  "Build the unit tests" ON)
option(OHPE_BUILD_PLUGIN "Build the JUCE plug-in, standalone app and UI (off = core library + core tests only)" ON)
option(OHPE_LTO         "Link-time optimisation for the plug-in in Release builds" ON)
option(OHPE_COPY_PLUGIN  "Copy the plug-in to the system plug-in folder after build" OFF)
set(OHPE_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8.0.15 checkout (otherwise JUCE is downloaded)")

if(MSVC)
    add_compile_options(/utf-8) # sources are UTF-8 (German texts)
endif()

include(FetchContent)

# ---------------------------------------------------------------- third-party (header only)
# Local copies can be used offline with -DFETCHCONTENT_SOURCE_DIR_DOCTEST=... / _NLOHMANN_JSON=...
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

if(OHPE_BUILD_TESTS)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW ON)
    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")
    FetchContent_MakeAvailable(doctest)
    enable_testing()
endif()

# ---------------------------------------------------------------- core (JUCE-free)
add_subdirectory(src/core)
if(OHPE_BUILD_TESTS)
    add_subdirectory(tests/core)
endif()

# ---------------------------------------------------------------- plug-in, standalone, UI
if(OHPE_BUILD_PLUGIN)
    include(cmake/OhpePlugin.cmake)
endif()
