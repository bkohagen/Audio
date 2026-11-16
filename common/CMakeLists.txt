# Shared kit: DSP building blocks (header-only, framework-free) + UI components.
# INTERFACE library: its sources are compiled into each plugin that links it.
add_library(kit INTERFACE)
target_include_directories(kit INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_sources(kit INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/kit/ui/Components.cpp")
