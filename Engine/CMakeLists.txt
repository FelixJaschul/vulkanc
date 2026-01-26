cmake_minimum_required(VERSION 3.10)
project(Engine LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64")

# Vulkan SDK setup
set(VULKAN_SDK_ROOT "/Users/felixjaschul/VulkanSDK/1.4.335.1/macOS")
set(Vulkan_INCLUDE_DIR ${VULKAN_SDK_ROOT}/include)
set(Vulkan_LIBRARY ${VULKAN_SDK_ROOT}/lib/libvulkan.dylib)

# Find GLFW
find_package(glfw3 REQUIRED)

# Engine source files
set(ENGINE_SOURCES
        App.c
)

# Create the Engine static library
add_library(Engine STATIC ${ENGINE_SOURCES})

# Public include directories
# - Engine/Include contains the public headers (App.h, etc.)
# - Parent directory (..) allows including "Engine/Include/Core.h" from app code
target_include_directories(Engine
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIVATE
        ${Vulkan_INCLUDE_DIR}
        /opt/homebrew/include
)

# Link dependencies (PUBLIC so they propagate to executables)
target_link_libraries(Engine
        PUBLIC
        ${Vulkan_LIBRARY}
        glfw
        /opt/homebrew/lib/libcglm.dylib
)

# Add math library on Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(Engine PUBLIC m)
endif()