cmake_minimum_required(VERSION 3.10)
project(Engine LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64")

# Vulkan SDK
find_package(Vulkan REQUIRED)

# Vendored dependencies
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../Vendor/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)

set(CGLM_STATIC ON CACHE BOOL "Static cglm build" FORCE)
set(CGLM_SHARED OFF CACHE BOOL "Disable shared cglm build" FORCE)
set(CGLM_USE_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../Vendor/cglm ${CMAKE_CURRENT_BINARY_DIR}/cglm)

# Engine source files
set(ENGINE_SOURCES
        App.c
)

# Create the Engine static library
add_library(Engine STATIC ${ENGINE_SOURCES})

# Public include directories
# - Engine/Include contains the public headers (App.h, etc.)
target_include_directories(Engine
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIVATE
        ${Vulkan_INCLUDE_DIRS}
)

# Link dependencies (PUBLIC so they propagate to executables)
target_link_libraries(Engine
        PUBLIC
        Vulkan::Vulkan
        glfw
        cglm
)

# Add math library on Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(Engine PUBLIC m)
endif()
