cmake_minimum_required(VERSION 3.22)

# Universal binary for Apple silicon (arm64) + Intel; must be set before project()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif()

project(AudioPlugins VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(KIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

option(KIT_BUILD_TESTS "Build DSP unit tests, RT-safety tests and screenshot tools" ON)

# JUCE: use a local checkout with -DJUCE_SOURCE_DIR=/path/to/JUCE, otherwise download it.
set(JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(KIT_BUILD_TESTS)
    enable_testing()
endif()

include(cmake/KitPlugin.cmake)
add_subdirectory(common)

# Every folder in plugins/ with a CMakeLists.txt is a plugin.
file(GLOB plugin_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*")
list(SORT plugin_dirs)
foreach(dir ${plugin_dirs})
    if(IS_DIRECTORY "${dir}" AND EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
    endif()
endforeach()
